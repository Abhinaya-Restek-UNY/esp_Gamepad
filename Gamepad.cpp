#include "Gamepad.hpp"
#include "bt/uni_bt.h"
#include "controller/uni_gamepad.h"
#include "esp_system.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "uni_hid_device.h"
#include <algorithm>
extern "C" {

#include "btstack_port_esp32.h"
#include "btstack_run_loop.h"
}

Gamepad *Gamepad::instance = nullptr;

struct uni_platform Gamepad::custom_platform = {
    .name = "OOP_Custom",
    .init = on_init,
    .on_init_complete = on_init_complete,
    .on_device_discovered = on_device_discovered,
    .on_device_connected = on_device_connected,
    .on_device_disconnected = on_device_disconnected,
    .on_device_ready = on_device_ready,
    .on_controller_data = on_controller_data,
    .on_oob_event = on_oob_event,
};

Gamepad::Gamepad() { instance = this; }

void Gamepad::start() {
  xTaskCreate(bt_task, "bluepad_task", 4096, nullptr, 5, nullptr);
}

void Gamepad::bt_task(void *arg) {
  uni_bt_allow_incoming_connections(true);
  btstack_init();
  // uni_bt_stop_scanning_safe();
  uni_platform_set_custom(&custom_platform);
  uni_init(0, nullptr);
  printf("Starting Bluetooth Task...\n");
  btstack_run_loop_execute();
  vTaskDelete(NULL);
}

int16_t Gamepad::get_dpad_x() { return this->dpad_x; }
int16_t Gamepad::get_dpad_y() { return this->dpad_y; };

void Gamepad::on_event(uni_hid_device_t *d, uni_controller_t *ctl) {
  instance->device_ptr = d;
  if (ctl->klass == UNI_CONTROLLER_CLASS_GAMEPAD) {

    uni_gamepad_t *gp = &ctl->gamepad;

    instance->throttle = gp->throttle;
    instance->brake = gp->brake;

    if (instance->throttle < 1 && ctl->gamepad.misc_buttons & 0x80) {
      instance->throttle = 1024;
    }

    if (instance->brake < 1 && ctl->gamepad.misc_buttons & 0x40) {
      instance->brake = 1024;
    }

    instance->current_l_joy = joy_data_t{gp->axis_x << 6, (gp->axis_y << 6)};

    instance->current_r_joy = joy_data_t{gp->axis_rx << 6, gp->axis_ry << 6};

    instance->buttons = gp->buttons | (gp->misc_buttons << 10);

    if (gp->dpad & DPAD_LEFT) {
      this->dpad_x = -32767;
    } else if (gp->dpad & DPAD_RIGHT) {
      this->dpad_x = 32767;
    } else {
      this->dpad_x = 0;
    }

    if (gp->dpad & DPAD_DOWN) {
      this->dpad_y = -32767;
    } else if (gp->dpad & DPAD_UP) {
      this->dpad_y = 32767;
    } else {
      this->dpad_y = 0;
    }
  }
}

int16_t Gamepad::get_brake() { return this->brake.load(); }
int16_t Gamepad::get_throttle() { return this->throttle.load(); }

bool Gamepad::is_pressed(Gamepad::ButtonCode code) {
  return (this->buttons & code) != 0;
};

bool Gamepad::is_just_pressed(Gamepad::ButtonCode code) {
  uint8_t ret = 0;

  int32_t just_pressed_mask = this->buttons & ~this->prev_buttons;

  if (just_pressed_mask & code) {
    this->prev_buttons |= code;
    ret = 1;
  } else if ((this->buttons & code) == 0) {
    this->prev_buttons &= ~code;
  }

  return ret;
};

void Gamepad::get_r_joy(joy_data_t *rjoy) {
  joy_data_t joy = this->current_r_joy;
  rjoy->x =
      std::clamp((int)(joy.x) - (int)(this->offset_r_joy.x), -32767, 32767);

  rjoy->y =
      std::clamp((int)(joy.y) - (int)(this->offset_r_joy.y), -32767, 32767);
}

void Gamepad::get_l_joy(joy_data_t *ljoy) {
  joy_data_t joy = this->current_l_joy;

  ljoy->x =
      std::clamp((int)(joy.x) - (int)(this->offset_l_joy.x), -32767, 32767);
  ljoy->y =
      std::clamp((int)(joy.y) - (int)(this->offset_l_joy.y), -32767, 32767);
}

void Gamepad::on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
  if (instance != nullptr) {
    instance->on_event(d, ctl);
  }
}

void Gamepad::zero_l_joy() { this->offset_l_joy = this->current_l_joy; };

void Gamepad::zero_r_joy() { this->offset_r_joy = this->current_r_joy; }

void Gamepad::on_init(int argc, const char **argv) {}

void Gamepad::on_init_complete() {
  uni_bt_start_scanning_and_autoconnect_unsafe();
  uni_bt_allow_incoming_connections(true);
}

uni_error_t Gamepad::on_device_discovered(bd_addr_t addr, const char *name,
                                          uint16_t cod, uint8_t rssi) {
  return UNI_ERROR_SUCCESS;
}

void Gamepad::on_device_connected(uni_hid_device_t *d) {

  instance->_is_connected = true;
  printf("Gamepad connected.\n");
}

void Gamepad::on_device_disconnected(uni_hid_device_t *d) {
  instance->_is_connected = false;
  esp_restart();
}

uni_error_t Gamepad::on_device_ready(uni_hid_device_t *d) {
  return UNI_ERROR_SUCCESS;
}

const uni_property_t *Gamepad::get_property(uni_property_idx_t idx) {
  return nullptr;
}

void Gamepad::on_oob_event(uni_platform_oob_event_t event, void *data) {}

void Gamepad::play_rumble() {
  static btstack_context_callback_registration_t rumble_callback;
  if (this->device_ptr != nullptr) {

    // We pass the controller's internal index as the context.
    // This is safer than passing the pointer, just in case the controller
    // disconnects!
    int gamepad_idx = uni_hid_device_get_idx_for_instance(this->device_ptr);

    rumble_callback.callback = &safe_rumble_task;
    rumble_callback.context = (void *)gamepad_idx;

    // Hand it off to the BTstack loop to execute on its next cycle
    btstack_run_loop_execute_on_main_thread(&rumble_callback);
  }
}

bool Gamepad::is_connected() { return this->_is_connected; }

void Gamepad::safe_rumble_task(void *context) {
  // Cast the context back to a device index
  int idx = (int)context;

  // Fetch the device safely from inside the BT thread
  uni_hid_device_t *d = uni_hid_device_get_instance_for_idx(idx);

  // Safety check: Ensure the gamepad hasn't disconnected in the last
  // millisecond
  if (d != NULL && d->report_parser.play_dual_rumble != NULL) {
    // play_dual_rumble(device, delay_ms, duration_ms, weak_motor, strong_motor)
    d->report_parser.play_dual_rumble(d, 0, 250, 0x80, 0x80);
  }
}

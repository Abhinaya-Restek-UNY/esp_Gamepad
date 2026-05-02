#include "Gamepad.hpp"
#include "bt/uni_bt.h"
#include "controller/uni_gamepad.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
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

Gamepad::Gamepad() {
  instance = this;
  this->data_mutex = xSemaphoreCreateMutex();
}

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

void Gamepad::on_event(uni_hid_device_t *d, uni_controller_t *ctl) {
  if (ctl->klass == UNI_CONTROLLER_CLASS_GAMEPAD) {

    if (xSemaphoreTake(instance->data_mutex, 0) == pdTRUE) {

      uni_gamepad_t *gp = &ctl->gamepad;

      instance->current_l_joy.x = (gp->axis_x << 6);
      instance->current_l_joy.y = (gp->axis_y << 6);

      instance->current_r_joy.x = (gp->axis_rx << 6);
      instance->current_r_joy.y = (gp->axis_ry << 6);

      instance->buttons = gp->buttons | (gp->misc_buttons << 10);

      xSemaphoreGive(instance->data_mutex);
    }
  }
}

bool Gamepad::is_pressed(Gamepad::ButtonCode code) {
  if (xSemaphoreTake(this->data_mutex, portMAX_DELAY) == pdTRUE) {
    xSemaphoreGive(this->data_mutex);
    return (this->buttons & code) != 0;
  };
  return false;
};

void Gamepad::get_r_joy(joy_data_t *rjoy) {
  if (xSemaphoreTake(this->data_mutex, portMAX_DELAY) == pdTRUE) {
    *rjoy = this->current_r_joy;
    rjoy->x -= this->offset_r_joy.x;
    rjoy->y -= this->offset_r_joy.y;
    xSemaphoreGive(this->data_mutex);
  };
}
void Gamepad::get_l_joy(joy_data_t *ljoy) {
  if (xSemaphoreTake(this->data_mutex, portMAX_DELAY) == pdTRUE) {
    *ljoy = this->current_l_joy;

    ljoy->x -= this->offset_l_joy.x;
    ljoy->y -= this->offset_l_joy.y;
    xSemaphoreGive(this->data_mutex);
  };
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

void Gamepad::on_device_connected(uni_hid_device_t *d) {}

void Gamepad::on_device_disconnected(uni_hid_device_t *d) {}

uni_error_t Gamepad::on_device_ready(uni_hid_device_t *d) {
  return UNI_ERROR_SUCCESS;
}

const uni_property_t *Gamepad::get_property(uni_property_idx_t idx) {
  return nullptr;
}

void Gamepad::on_oob_event(uni_platform_oob_event_t event, void *data) {}

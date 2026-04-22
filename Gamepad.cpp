#include "Gamepad.hpp"
extern "C" {

#include "btstack_port_esp32.h"
#include "btstack_run_loop.h"
}

// Initialize the static instance pointer to nullptr
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
  // Create a FreeRTOS task for the Bluetooth loop.
  // 4096 bytes of stack memory is generally safe for BTstack.
  xTaskCreate(bt_task, "bluepad_task", 4096, nullptr, 5, nullptr);
}

void Gamepad::bt_task(void *arg) {

  btstack_init();
  uni_platform_set_custom(&custom_platform);
  uni_init(0, nullptr);
  printf("Starting Bluetooth Task...\n");
  btstack_run_loop_execute();
  vTaskDelete(NULL);
}

void Gamepad::on_event(uni_hid_device_t *d, uni_controller_t *ctl) {
  if (ctl->klass == UNI_CONTROLLER_CLASS_GAMEPAD) {
    uni_gamepad_t *gp = &ctl->gamepad;
    printf("OOP Event Triggered! Buttons: 0x%04x | X: %ld | Y: %ld\n",
           gp->buttons, (long int)gp->axis_x, (long int)gp->axis_y);
  }
}

void Gamepad::on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
  // Route the static callback into our class instance
  if (instance != nullptr) {
    instance->on_event(d, ctl);
  }
}

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

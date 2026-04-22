#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <functional>
#include <uni.h>

using on_direction_cb = std::function<void(int16_t, int16_t)>;
using on_yaw_cb = std::function<void(int16_t, int16_t)>;
using on_grip = std::function<void(int16_t, int16_t)>;

class Gamepad {
public:
  Gamepad();
  void start();

  // The non-static method that will actually handle the data
  void on_event(uni_hid_device_t *d, uni_controller_t *ctl);

private:
  // The bridge from static callbacks to our object instance
  static Gamepad *instance;

  // The FreeRTOS task function
  static void bt_task(void *arg);

  static struct uni_platform custom_platform;

  // --- Static Callbacks ---
  static void on_init(int argc, const char **argv);
  static void on_init_complete();
  static uni_error_t on_device_discovered(bd_addr_t addr, const char *name,
                                          uint16_t cod, uint8_t rssi);
  static void on_device_connected(uni_hid_device_t *d);
  static void on_device_disconnected(uni_hid_device_t *d);
  static uni_error_t on_device_ready(uni_hid_device_t *d);
  static void on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl);
  static const uni_property_t *get_property(uni_property_idx_t idx);
  static void on_oob_event(uni_platform_oob_event_t event, void *data);
};

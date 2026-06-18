#pragma once

#include "PresistentConfig.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <functional>
#include <uni.h>

#define MAX_THROTTLE_BRAKE 1024

/**
 * @brief Callback types for mapping gamepad inputs to external functions.
 * (Note: These are defined but currently unused in the underlying
 * implementation)
 */
using on_direction_cb =
    std::function<void(int16_t, int16_t)>; ///< Callback for directional inputs
                                           ///< (e.g., left joystick/D-pad)

using on_yaw_cb =
    std::function<void(int16_t, int16_t)>; ///< Callback for yaw/rotational
                                           ///< inputs (e.g., right joystick)

using on_grip = std::function<void(
    int16_t, int16_t)>; ///< Callback for grip/trigger inputs (e.g., L2/R2)

/**
 * @brief A wrapper class for handling Bluetooth gamepads using
 * Bluepad32/BTStack.
 * * Uses a singleton-like instance pointer to bridge static C-style callbacks
 * required by the `uni_platform` struct to the non-static C++ class methods.
 */
class Gamepad {
public:
  enum ButtonCode {
    CROSS = 0b0000000000000001,
    CIRCLE = 0b0000000000000010,
    SQUARE = 0b0000000000000100,
    TRIANGLE = 0b0000000000001000,
    L1 = 0b0000000000010000,
    R1 = 0b0000000000100000,
    L2 = 0b0000000001000000,
    R2 = 0b0000000010000000,
    L3 = 0b0000000100000000,
    R3 = 0b0000001000000000,
    PS_BUTTON = 0b0000010000000000,
    SHARE = 0b0000100000000000,
    OPTIONS = 0b0001000000000000,

  };

  struct joy_data_t {
    int x;
    int y;
  };

  /**
   * @brief Constructs the Gamepad object and assigns the global instance
   * pointer.
   */
  Gamepad();

  /**
   * @brief Spawns the FreeRTOS task to initialize and run the BTStack loop.
   */
  void start();

  /**
   * @brief Non-static handler for processing incoming gamepad events.
   * @param d Pointer to the generic HID device struct.
   * @param ctl Pointer to the controller struct containing axis/button states.
   */
  void on_event(uni_hid_device_t *d, uni_controller_t *ctl);

  void zero_l_joy();

  void zero_r_joy();

  bool is_pressed(Gamepad::ButtonCode code);
  bool is_just_pressed(Gamepad::ButtonCode code);
  bool is_connected();

  void get_r_joy(joy_data_t *rjoy);
  void get_l_joy(joy_data_t *ljoy);

  void play_rumble();

  int16_t get_dpad_x();
  int16_t get_dpad_y();

  int16_t get_brake();
  int16_t get_throttle();

  void lock();
  void unlock();

private:
  static void safe_rumble_task(void *context);
  joy_data_t offset_r_joy = joy_data_t{0, 0};
  joy_data_t offset_l_joy = joy_data_t{0, 0};
  PresistentConfig<bd_addr_t> address;
  PresistentConfig<bool> is_locked;

  std::atomic<int16_t> dpad_x = 0;
  std::atomic<int16_t> dpad_y = 0;
  std::atomic<joy_data_t> current_r_joy = joy_data_t{0, 0};
  std::atomic<joy_data_t> current_l_joy = joy_data_t{0, 0};
  std::atomic<bool> _is_connected = false;
  std::atomic<uni_hid_device_t *> device_ptr = NULL;

  std::atomic<uint16_t> buttons = 0;
  std::atomic<uint16_t> prev_buttons = 0;
  std::atomic<uint16_t> just_pressed = 0;
  std::atomic<int16_t> throttle = 0;
  std::atomic<int16_t> brake = 0;
  /**
   * @brief Global pointer to the active Gamepad instance.
   * Necessary for routing C-style static callbacks back into the C++ object
   * context.
   */
  static Gamepad *instance;

  /**
   * @brief The FreeRTOS task function that initializes Bluepad32 and executes
   * the BTStack run loop.
   * @param arg Unused FreeRTOS task argument.
   */
  static void bt_task(void *arg);

  /**
   * @brief Custom Bluepad32 platform configuration struct containing our
   * callback bindings.
   */
  static struct uni_platform custom_platform;

  // --- Static Callbacks (Bluepad32 C-API Bindings) ---

  /**
   * @brief Called when the Bluepad32 platform is initializing.
   */
  static void on_init(int argc, const char **argv);

  /**
   * @brief Called when platform initialization is complete.
   * Starts scanning and allows incoming connections.
   */
  static void on_init_complete();

  /**
   * @brief Called when a new Bluetooth HID device is discovered.
   * @param addr Bluetooth MAC address of the device.
   * @param name Name of the device.
   * @param cod Class of Device.
   * @param rssi Signal strength.
   * @return uni_error_t UNI_ERROR_SUCCESS to accept the device.
   */
  static uni_error_t on_device_discovered(bd_addr_t addr, const char *name,
                                          uint16_t cod, uint8_t rssi);

  /**
   * @brief Called when a device successfully connects.
   * @param d Pointer to the connected HID device.
   */
  static void on_device_connected(uni_hid_device_t *d);

  /**
   * @brief Called when a device disconnects.
   * @param d Pointer to the disconnected HID device.
   */
  static void on_device_disconnected(uni_hid_device_t *d);

  /**
   * @brief Called when a connected device is parsed and ready to send reports.
   * @param d Pointer to the ready HID device.
   * @return uni_error_t UNI_ERROR_SUCCESS.
   */
  static uni_error_t on_device_ready(uni_hid_device_t *d);

  /**
   * @brief Intercepts controller data and routes it to the active instance's
   * on_event() method.
   * @param d Pointer to the HID device sending data.
   * @param ctl Pointer to the parsed controller data state.
   */
  static void on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl);

  /**
   * @brief Retrieves custom platform properties (currently unimplemented).
   * @param idx Property index requested.
   * @return const uni_property_t* nullptr.
   */
  static const uni_property_t *get_property(uni_property_idx_t idx);

  /**
   * @brief Out-Of-Band (OOB) event handler.
   * @param event The OOB event type.
   * @param data Pointer to event-specific data.
   */
  static void on_oob_event(uni_platform_oob_event_t event, void *data);
};

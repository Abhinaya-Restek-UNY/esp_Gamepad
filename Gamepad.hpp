/**
 * @file Gamepad.hpp
 * @brief An object-oriented wrapper for handling Bluetooth gamepads using
 * Bluepad32/BTStack.
 * * @note **DEPENDENCY REQUIREMENT:** This library requires `bluepad32`,
 * `btstack`, and `esp_PresistentConfig` to be installed in your ESP-IDF
 * project's components. This component does not handle the base installation or
 * menuconfig setup of Bluepad32.
 */

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
 * @brief A wrapper class for handling Bluetooth gamepads.
 * * Uses a singleton-like instance pointer to bridge static C-style callbacks
 * required by the `uni_platform` struct to the non-static C++ class methods.
 * Internal state is managed using `std::atomic` to ensure thread-safe reads
 * from the main application loop while the BTStack task updates data in the
 * background.
 */
class Gamepad {
public:
  /**
   * @brief Bitmask definitions for standard gamepad buttons.
   */
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

  /**
   * @brief Represents 2D Cartesian coordinates for joystick axes.
   */
  struct joy_data_t {
    int x;
    int y;
  };

  /**
   * @brief Constructs the Gamepad object and assigns the global instance
   * pointer. Initializes persistent configuration for Bluetooth MAC address
   * locking.
   */
  Gamepad();

  /**
   * @brief Spawns the FreeRTOS task to initialize and run the BTStack loop.
   * Must be called to begin listening for Bluetooth controller connections.
   */
  void start();

  /**
   * @brief Non-static handler for processing incoming gamepad events.
   * @param d Pointer to the generic HID device struct.
   * @param ctl Pointer to the controller struct containing parsed axis/button
   * states.
   */
  void on_event(uni_hid_device_t *d, uni_controller_t *ctl);

  /**
   * @brief Sets the current left joystick position as the new zero/center
   * offset.
   */
  void zero_l_joy();

  /**
   * @brief Sets the current right joystick position as the new zero/center
   * offset.
   */
  void zero_r_joy();

  /**
   * @brief Checks if a specific button is currently held down.
   * @param code The ButtonCode to check.
   * @return true if the button is pressed, false otherwise.
   */
  bool is_pressed(Gamepad::ButtonCode code);

  /**
   * @brief Checks if a specific button was pressed exactly on this processing
   * frame.
   * @param code The ButtonCode to check.
   * @return true if the button transitioned from unpressed to pressed, false
   * otherwise.
   */
  bool is_just_pressed(Gamepad::ButtonCode code);

  /**
   * @brief Checks the active connection status of the gamepad.
   * @return true if a gamepad is currently connected, false otherwise.
   */
  bool is_connected();

  /**
   * @brief Retrieves the current state of the right joystick, accounting for
   * offsets.
   * @param rjoy Pointer to a joy_data_t struct to populate with the axis
   * values.
   */
  void get_r_joy(joy_data_t *rjoy);

  /**
   * @brief Retrieves the current state of the left joystick, accounting for
   * offsets.
   * @param ljoy Pointer to a joy_data_t struct to populate with the axis
   * values.
   */
  void get_l_joy(joy_data_t *ljoy);

  /**
   * @brief Safely triggers a dual-rumble effect on the connected gamepad.
   */
  void play_rumble();

  /**
   * @brief Retrieves the D-Pad X-axis state.
   * @return -32767 for Left, 32767 for Right, 0 for neutral.
   */
  int16_t get_dpad_x();

  /**
   * @brief Retrieves the D-Pad Y-axis state.
   * @return -32767 for Down, 32767 for Up, 0 for neutral.
   */
  int16_t get_dpad_y();

  /**
   * @brief Retrieves the analog brake (L2) trigger value.
   * @return Integer value representing brake pressure (0 to
   * MAX_THROTTLE_BRAKE).
   */
  int16_t get_brake();

  /**
   * @brief Retrieves the analog throttle (R2) trigger value.
   * @return Integer value representing throttle pressure (0 to
   * MAX_THROTTLE_BRAKE).
   */
  int16_t get_throttle();

  /**
   * @brief Locks the system to the currently connected gamepad's MAC address.
   * Prevents other Bluetooth controllers from connecting. Saves to NVS.
   */
  void lock();

  /**
   * @brief Unlocks the system, allowing any Bluetooth controller to connect.
   * Clears the MAC address restriction from NVS.
   */
  void unlock();

private:
  /**
   * @brief Thread-safe proxy function executed by the BTstack loop to trigger
   * rumble.
   * @param context Void pointer cast of the internal controller index.
   */
  static void safe_rumble_task(void *context);

  joy_data_t offset_r_joy = joy_data_t{0, 0};
  joy_data_t offset_l_joy = joy_data_t{0, 0};

  PresistentConfig<bd_addr_t> address;
  PresistentConfig<bool> is_locked;

  // Thread-safe atomic state variables updated by the BT task and read by the
  // main task.
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

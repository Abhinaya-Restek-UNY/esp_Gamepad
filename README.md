# esp_Gamepad

[![ESP-IDF Component](https://img.shields.io/badge/ESP--IDF-Component-green)](#)

An object-oriented, thread-safe C++ wrapper for handling Bluetooth gamepads on the ESP32 using [Bluepad32](https://github.com/ricardoquesada/bluepad32).

This library abstracts away the C-style callbacks, FreeRTOS task management, and pointer handling required by Bluepad32. It provides a clean, polling-friendly C++ API with built-in thread safety (using `std::atomic`) and MAC address locking via persistent NVS storage.

## Features

- **Object-Oriented API:** No more static C callbacks. Just instantiate the class and poll for inputs.
- **Thread-Safe:** Background BTStack tasks update controller state atomically, allowing safe polling from your main application loop.
- **Controller Locking:** Automatically lock the ESP32 to a specific controller's MAC address using `esp_PresistentConfig`, preventing other devices from hijacking the connection.
- **Rumble Support:** Safely dispatch dual-motor rumble commands back to the BT thread.
- **Deadzone & Centering:** Built-in methods to easily zero out joystick offsets.

## ⚠️ Important Prerequisites

This component **requires Bluepad32** to be installed in your ESP-IDF project. Because Bluepad32 relies on a custom Python integration script to patch ESP-IDF and BTStack, it **cannot** be downloaded automatically by the IDF Component Manager. 

**You must install Bluepad32 manually before using this library:**
1. Follow the [Bluepad32 ESP-IDF Integration Guide](https://bluepad32.readthedocs.io/en/latest/plat_espidf/).
2. Run their `integrate_btstack.py` script as instructed.
3. Ensure you can successfully build a basic Bluepad32 project before proceeding.

## Installation

Once Bluepad32 is installed, you can add this wrapper and its persistent storage dependency to your project via `idf_component.yml`:

```yaml
dependencies:
  esp_Gamepad:
    git: [https://github.com/Abhinaya-Restek-UNY/esp_Gamepad.git](https://github.com/Abhinaya-Restek-UNY/esp_Gamepad.git) # Replace with your actual URL
```

*Note: The ESP-IDF Component Manager will automatically fetch the required `esp_PresistentConfig` dependency for you.*

## Quick Start

Here is a minimal example of how to initialize the gamepad and read inputs in your main loop.

```cpp
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "Gamepad.hpp"

// Instantiate the Gamepad object
Gamepad myGamepad;

extern "C" void app_main(void) {
    // 1. Initialize NVS (Required for MAC address locking)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2. Start the Gamepad Bluetooth task
    printf("Starting Gamepad listener...\n");
    myGamepad.start();

    // 3. Main Application Loop
    while (true) {
        if (myGamepad.is_connected()) {
            
            // Check button states
            if (myGamepad.is_just_pressed(Gamepad::CROSS)) {
                printf("Cross button was just pressed!\n");
                myGamepad.play_rumble();
            }

            // Read Joysticks
            Gamepad::joy_data_t left_joy;
            myGamepad.get_l_joy(&left_joy);
            
            if (abs(left_joy.y) > 1000) {
                printf("Left Joystick Y: %d\n", left_joy.y);
            }

            // Read Triggers
            int brake = myGamepad.get_brake();
            int throttle = myGamepad.get_throttle();
            if (throttle > 0) {
                printf("Throttle: %d\n", throttle);
            }
        }
        
        // Poll every 20ms (50Hz)
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}
```

## Core API Reference

### Initialization & State
- `void start()`: Spawns the FreeRTOS task and starts listening for Bluetooth connections.
- `bool is_connected()`: Returns `true` if a gamepad is actively connected.

### Inputs (Thread-Safe Polling)
- `bool is_pressed(ButtonCode code)`: Returns `true` if the specified button is currently held down.
- `bool is_just_pressed(ButtonCode code)`: Returns `true` if the button was pressed exactly on this polling frame.
- `void get_l_joy(joy_data_t *ljoy)`: Populates the struct with the Left Joystick X/Y axes (-32767 to 32767).
- `void get_r_joy(joy_data_t *rjoy)`: Populates the struct with the Right Joystick X/Y axes (-32767 to 32767).
- `int16_t get_brake()`: Returns the L2 analog trigger value (0 to 1024).
- `int16_t get_throttle()`: Returns the R2 analog trigger value (0 to 1024).
- `int16_t get_dpad_x() / get_dpad_y()`: Returns simulated analog values for D-Pad presses (-32767, 0, or 32767).

### Hardware Control
- `void play_rumble()`: Triggers a 250ms dual-motor rumble on the controller.
- `void zero_l_joy() / void zero_r_joy()`: Sets the current joystick position as the new physical center (0,0) offset.

### Security / MAC Locking
- `void lock()`: Saves the currently connected controller's MAC address to NVS. No other controllers will be allowed to connect.
- `void unlock()`: Clears the locked MAC address, allowing any controller to connect again.

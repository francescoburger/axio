#pragma once

// Confirmed BNO055 wiring on the Seeed XIAO ESP32C3.
// Change GPIO definitions here for future wiring revisions.
#define SDA_PIN 6  // D4
#define SCL_PIN 7  // D5
#define BNO055_I2C_ADDRESS 0x29

// User-confirmed GPIOs; button to GND with pull-up.
#define BUTTON_PIN 5
#define BATTERY_ADC_PIN 2  // XIAO ESP32C3 pad A0 / D0, confirmed by the user.

#define BUTTON_ACTIVE_LEVEL 0
#define BUTTON_USE_PULLUP 1

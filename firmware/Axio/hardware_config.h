#pragma once

// User-confirmed pads on both XIAO boards: SDA D4, SCL D5, button D3,
// battery divider A0 / D0. GPIO numbers differ between the boards.
#if defined(ARDUINO_XIAO_ESP32S3)
#define AXIO_BOARD_NAME "XIAO_ESP32S3"
#define SDA_PIN 5  // D4
#define SCL_PIN 6  // D5
#define BUTTON_PIN 4  // D3, RTC GPIO for deep-sleep wake.
#define BATTERY_ADC_PIN 1  // A0 / D0
#define FEEDBACK_LED_PIN 21  // Onboard user LED, active LOW.
#define FEEDBACK_LED_ACTIVE_LEVEL 0
#else
#if defined(ARDUINO_ARCH_ESP32) && !defined(ARDUINO_XIAO_ESP32C3)
#error "Select XIAO_ESP32C3 or XIAO_ESP32S3 for Axio"
#endif
#define AXIO_BOARD_NAME "XIAO_ESP32C3"
#define SDA_PIN 6  // D4
#define SCL_PIN 7  // D5
#define BUTTON_PIN 5  // D3
#define BATTERY_ADC_PIN 2  // XIAO ESP32C3 pad A0 / D0, confirmed by the user.
#define FEEDBACK_LED_PIN 3  // D1: external LED through a series resistor to GND.
#define FEEDBACK_LED_ACTIVE_LEVEL 1
#endif

#define BNO055_I2C_ADDRESS 0x29
// Button to GND with internal pull-up.
#define BUTTON_ACTIVE_LEVEL 0
#define BUTTON_USE_PULLUP 1

#pragma once

// Cablaggio BNO055 confermato sulla Seeed XIAO ESP32C3.
// Modificare qui i GPIO per eventuali cablaggi futuri.
#define SDA_PIN 6  // D4
#define SCL_PIN 7  // D5
#define BNO055_I2C_ADDRESS 0x29

// Fase 4: GPIO confermati dall’utente; pulsante verso GND con pull-up.
#define BUTTON_PIN 5
#define BATTERY_ADC_PIN 2  // Pad A0 / D0 sulla XIAO ESP32C3, confermato dall’utente.

#define BUTTON_ACTIVE_LEVEL 0
#define BUTTON_USE_PULLUP 1

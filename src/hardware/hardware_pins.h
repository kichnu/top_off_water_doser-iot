#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// Seeed Xiao ESP32-C3 Pin Mapping
#define PUMP_RELAY_PIN      10            // Pump relay control (HIGH = pump ON)
// #define STATUS_LED_PIN      2            // ERROR signal
#define WATER_SENSOR_1_PIN   3            // Float sensor 1 (pull-up, active LOW)
#define WATER_SENSOR_2_PIN   4            // Float sensor 2 (pull-up, active LOW)
#define RTC_SDA_PIN         6            // DS3231M I2C SDA
#define RTC_SCL_PIN         7            // DS3231M I2C SCL

// ============== PINY SYSTEMOWE ==============
#define ERROR_SIGNAL_PIN    2   // 2 Pin sygnalizacji błędów ERR0/1/2
#define RESET_PIN          8  // Pin fizycznego resetu

#endif
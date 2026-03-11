#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// ============================================================
// Seeed Xiao ESP32-C3 — Pin Mapping
// ============================================================
//
//                 ┌─────────────────────┐
//            5V ──┤ 5V             3.3V ├── 3.3V
//           GND ──┤ GND             GND ├── GND
//  [D0] GPIO2  ──┤ D0             D10  ├── GPIO10  [D10]
//  [D1] GPIO3  ──┤ D1              D9  ├── GPIO9   [D9]
//  [D2] GPIO4  ──┤ D2              D8  ├── GPIO8   [D8]
//  [D3] GPIO5  ──┤ D3              D7  ├── GPIO20  [D7]
//  [D4] GPIO6  ──┤ D4/SDA          D6  ├── GPIO21  [D6]
//  [D5] GPIO7  ──┤ D5/SCL          5V  ├── (USB)
//                 └─────────────────────┘
//                        [USB-C]
//
// ============================================================

// ============== TOP-OFF SYSTEM ==============
#define ATO_PUMP_RELAY_PIN          21  // Pompa top-off (LOW = ON, active-LOW relay) [D6]
#define WATER_SENSOR_PIN             9  // Czujnik pływakowy — para równoległa (INPUT_PULLUP, active LOW)

// ============== KALKWASSER SYSTEM (2 + 1 kanał) ==============
#define PERYSTALTIC_PUMP_DRIVER_PIN   10  // Pompa dozująca A (HIGH = ON)
#define MIXING_PUMP_RELAY_PIN        3  // Pompa dozująca A (HIGH = ON)
#define RESERVE_PUMP_RELAY_PIN       4  // Pompa dozująca A (HIGH = ON)


// ============== I2C (DS3231 RTC + FRAM MB85RC256V) ==============
#define I2C_SDA_PIN          6
#define I2C_SCL_PIN          7

// ============== SYSTEM ==============
#define ERROR_SIGNAL_PIN     8  // Sygnalizacja błędów — buzzer/LED (HIGH = aktywny)
#define RESET_PIN            5  // Przycisk resetu / provisioning (INPUT_PULLUP, active LOW)

// ============== WOLNE (rezerwa) ==============
// GPIO 2  — zwolniony (był ATO_PUMP_RELAY_PIN; ADC1_CH2 = konflikt z WiFi/periman)
// GPIO 4  — zwolniony po usunięciu drugiego czujnika
// GPIO 20 — UART0 RX (Serial = USB CDC na XIAO, GPIO20 fizycznie wolny)
// GPIO 21 — zajęty: ATO_PUMP_RELAY_PIN [D6]

#endif // HARDWARE_PINS_H

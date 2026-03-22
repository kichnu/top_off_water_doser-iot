#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// ============================================================
// Seeed Xiao ESP32-C3 — Pin Mapping
// ============================================================
//
//                                          ┌────────────────┐
//                               GPIO  2  ──┤                ├── 5V
//       MIXING_PUMP_RELAY_PIN   GPIO  3  ──┤                ├── GND
//          ATO_PUMP_RELAY_PIN   GPIO  4  ──┤                ├── 3.3V
//                   RESET_PIN   GPIO  5  ──┤                ├── GPIO 10  
//                 I2C_SDA_PIN   GPIO  6  ──┤                ├── GPIO  9  ERROR_SIGNAL_PIN 
//                 I2C_SCL_PIN   GPIO  7  ──┤                ├── GPIO  8 
//  AVAILABLE_WATER_SENSOR_PIN   GPIO 21  ──┤                ├── GPIO 20  WATER_SENSOR_PIN 
//                                          └────────────────┘
//                       
//
// ============================================================

// ============== TOP-OFF SYSTEM ==============
#define ATO_PUMP_RELAY_PIN 4  // Pompa top-off (LOW = ON, active-LOW relay) [D2]
#define WATER_SENSOR_PIN 20  // Czujnik pływakowy — para równoległa (INPUT_PULLUP, active LOW)
#define AVAILABLE_WATER_SENSOR_PIN 21   // Czujnik pływakowy  (INPUT_PULLUP, active LOW)

// ============== KALKWASSER SYSTEM (2 + 1 kanał) ==============
#define PERYSTALTIC_PUMP_DRIVER_PIN 10  // Pompa dozująca A (HIGH = ON)
#define MIXING_PUMP_RELAY_PIN 3  // Pompa miksująca (LOW = ON, active-LOW relay) [D1]
// #define RESERVE_PUMP_RELAY_PIN       4  // Rezerwa (LOW = ON, active-LOW relay) [D2]


// ============== I2C (DS3231 RTC + FRAM MB85RC256V) ==============
#define I2C_SDA_PIN 6
#define I2C_SCL_PIN 7

// ============== SYSTEM ==============
#define RESET_PIN 5  // Przycisk resetu / provisioning (INPUT_PULLUP, active LOW)

// ============== DFPLAYER PRO (Fermion DFR0768) — UART1 ==============
#define DFPLAYER_TX_PIN 9   // ESP32 TX → DFPlayer RX  (był ERROR_SIGNAL_PIN)
#define DFPLAYER_RX_PIN 8   // DFPlayer TX → ESP32 RX  (był wolny)

// ============== WOLNE (rezerwa) ==============
// GPIO 2  — zwolniony (był ATO_PUMP_RELAY_PIN; ADC1_CH2 = konflikt z WiFi/periman)
// GPIO 20 — UART0 RX (Serial = USB CDC na XIAO, GPIO20 fizycznie wolny)
// GPIO 21 — UART0 TX (Serial = USB CDC na XIAO, GPIO21 fizycznie wolny)

#endif // HARDWARE_PINS_H

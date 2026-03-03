#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H
#include <Arduino.h>

// ============================================================
// DEBOUNCING CZUJNIKA WEJŚCIOWEGO
// Czujniki pływakowe (para równoległa) na jednym pinie.
// Próbkowanie co DEBOUNCE_INTERVAL_MS ms.
// Wyzwolenie po DEBOUNCE_COUNTER kolejnych odczytach LOW.
// Efektywny czas debouncingu = DEBOUNCE_INTERVAL_MS × DEBOUNCE_COUNTER
// ============================================================
#define DEBOUNCE_INTERVAL_MS     200   // ms między próbkowaniami
#define DEBOUNCE_COUNTER           5   // kolejnych LOW wymaganych (= 1000 ms efektywnie)

// ============================================================
// RELEASE VERIFICATION (po uruchomieniu pompy)
// Czujnik musi wrócić do HIGH (RELEASE_DEBOUNCE_COUNT razy)
// zanim cykl zostanie uznany za zakończony.
// ============================================================
#define RELEASE_CHECK_INTERVAL_MS  500  // ms między sprawdzeniami czujnika
#define RELEASE_DEBOUNCE_COUNT       3  // kolejnych HIGH potrzebnych do potwierdzenia

// ============================================================
// LOGGING
// ============================================================
#define LOGGING_TIME                 5  // s — czas fazy logowania po cyklu

// ============================================================
// SYGNALIZACJA BŁĘDÓW (ERROR_SIGNAL_PIN)
// ============================================================
#define ERROR_PULSE_HIGH           100  // ms — czas impulsu HIGH
#define ERROR_PULSE_LOW            100  // ms — przerwa między impulsami
#define ERROR_PAUSE               2000  // ms — pauza przed powtórzeniem sekwencji

// ============================================================
// DOMYŚLNE WARTOŚCI KONFIGURACJI ALGORYTMU
// Używane gdy FRAM nie zawiera zapisanej konfiguracji.
// Nadpisywane przez użytkownika przez Provisioning lub GUI.
// ============================================================
#define DEFAULT_MAX_DOSE_ML         300     // Limit bezpieczeństwa jednej dolewki [ml]
#define DEFAULT_EMA_ALPHA          0.20f    // Współczynnik wygładzania EMA [0.10–0.40]
#define DEFAULT_VOL_YELLOW_PCT       30     // Próg żółty — odchylenie objętości [%]
#define DEFAULT_VOL_RED_PCT          60     // Próg czerwony — odchylenie objętości [%]
#define DEFAULT_RATE_YELLOW_PCT      40     // Próg żółty — odchylenie tempa [%]
#define DEFAULT_RATE_RED_PCT         80     // Próg czerwony — odchylenie tempa [%]
#define DEFAULT_HISTORY_WINDOW_S  86400     // Okno historii [s] — domyślnie 24h
#define DEFAULT_MIN_BOOTSTRAP         5     // Min. dolewek przed aktywacją alertów

// ============================================================
// SPRAWDZENIA INTEGRALNOŚCI STAŁYCH
// ============================================================
static_assert(DEBOUNCE_INTERVAL_MS >= 50 && DEBOUNCE_INTERVAL_MS <= 2000,
    "DEBOUNCE_INTERVAL_MS must be 50-2000 ms");
static_assert(DEBOUNCE_COUNTER >= 2 && DEBOUNCE_COUNTER <= 20,
    "DEBOUNCE_COUNTER must be 2-20");
static_assert(RELEASE_DEBOUNCE_COUNT >= 1 && RELEASE_DEBOUNCE_COUNT <= 10,
    "RELEASE_DEBOUNCE_COUNT must be 1-10");
static_assert(DEFAULT_MAX_DOSE_ML >= 50 && DEFAULT_MAX_DOSE_ML <= 2000,
    "DEFAULT_MAX_DOSE_ML must be 50-2000 ml");

// ============================================================
// STANY ALGORYTMU
// ============================================================
enum AlgorithmState {
    STATE_IDLE = 0,         // Oczekiwanie — sensor callback wyzwoli przejście
    STATE_DEBOUNCING,       // Debouncing czujnika w toku (obsługiwany przez water_sensors)
    STATE_PUMPING,          // Pompa aktywna — czekanie na zwolnienie czujnika lub timeout
    STATE_LOGGING,          // Krótka faza po cyklu — zapis do FRAM, obliczenie EMA
    STATE_ERROR,            // Błąd — oczekiwanie na reset
    STATE_MANUAL_OVERRIDE   // Ręczna pompa aktywna
};

// ============================================================
// KODY BŁĘDÓW
// ============================================================
enum ErrorCode {
    ERROR_NONE        = 0,
    ERROR_DAILY_LIMIT = 1,  // Przekroczono limit 24h
    ERROR_TIMEOUT     = 2,  // Pompa zatrzymana przez timeout (czujnik nie zwolnił)
    ERROR_BOTH        = 3   // Oba błędy jednocześnie
};

// ============================================================
// REKORD DOLEWKI — zapisywany do ring buffer FRAM po każdym cyklu
// Rozmiar: 4+2+4+4+1+1+1+1 = 18 bajtów
// ============================================================
struct TopOffRecord {
    // Pola ułożone dla naturalnego wyrównania — sizeof = 20 bajtów
    uint32_t timestamp;       // Unix timestamp UTC (z DS3231 RTC)
    uint32_t interval_s;      // Czas od poprzedniej dolewki [s]
    float    rate_ml_h;       // Tempo zużycia wody [ml/h] = volume_ml / interval_s × 3600
    uint16_t volume_ml;       // Faktycznie dolana objętość [ml] = duration_s × flow_rate
    int8_t   dev_volume_pct;  // Odchylenie obj. od EMA_volume [%] — zakres -127..+127
    int8_t   dev_rate_pct;    // Odchylenie tempa od EMA_rate [%]
    uint8_t  alert_level;     // 0 = OK, 1 = ŻÓŁTY, 2 = CZERWONY
    uint8_t  flags;           // Bitmaska flag (patrz poniżej)
    uint8_t  _pad[2];         // Wyrównanie do wielokrotności 4 bajtów

    // Flags bitmask
    static const uint8_t FLAG_MANUAL    = 0x01;  // Dolewka wyzwolona ręcznie z GUI
    static const uint8_t FLAG_TIMEOUT   = 0x02;  // Zatrzymana przez timeout — czujnik nie zwolnił
    static const uint8_t FLAG_BOOTSTRAP = 0x04;  // Rekord z okresu budowania historii EMA
};
static_assert(sizeof(TopOffRecord) == 20, "TopOffRecord must be exactly 20 bytes");

// ============================================================
// BLOK EMA — persystowany w FRAM, przeżywa restart
// Rozmiar: 4+4+4+1+3 = 16 bajtów
// ============================================================
struct EmaBlock {
    float   ema_volume_ml;    // EMA typowej objętości dolewki [ml]
    float   ema_interval_s;   // EMA typowego interwału między dolewkami [s]
    float   ema_rate_ml_h;    // EMA tempa zużycia wody [ml/h]
    uint8_t bootstrap_count;  // Liczba zebranych dolewek (do DEFAULT_MIN_BOOTSTRAP)
    uint8_t _pad[3];
};
static_assert(sizeof(EmaBlock) == 16, "EmaBlock must be exactly 16 bytes");

// ============================================================
// KONFIGURACJA ALGORYTMU — persystowana w FRAM, nadpisuje defaults z kodu
// Rozmiar: 2+4+1+1+1+1+4+1+1 = 16 bajtów
// ============================================================
struct TopOffConfig {
    // Pola ułożone dla naturalnego wyrównania — sizeof = 20 bajtów
    float    ema_alpha;         // Współczynnik EMA [0.10–0.40]
    uint32_t history_window_s;  // Okno historii [s] (86400 / 129600 / 172800)
    uint16_t max_dose_ml;       // Limit bezpieczeństwa jednej dolewki [ml]
    uint8_t  vol_yellow_pct;    // Próg żółty — odchylenie objętości [%]
    uint8_t  vol_red_pct;       // Próg czerwony — odchylenie objętości [%]
    uint8_t  rate_yellow_pct;   // Próg żółty — odchylenie tempa [%]
    uint8_t  rate_red_pct;      // Próg czerwony — odchylenie tempa [%]
    uint8_t  is_configured;     // 0xA5 = konfiguracja zapisana w FRAM
    uint8_t  _pad[5];           // Wyrównanie do wielokrotności 4 bajtów
};
static_assert(sizeof(TopOffConfig) == 20, "TopOffConfig must be exactly 20 bytes");

#define TOPOFF_CONFIG_MAGIC  0xA5  // Marker: konfiguracja zapisana

// ============================================================
// POMOCNICZE OBLICZENIA
// ============================================================

// Timeout pompy [s] wynikający z limitu bezpieczeństwa i kalibracji
inline uint16_t calculateMaxPumpTime(uint16_t maxDoseMl, float flowRateMlS) {
    if (flowRateMlS <= 0.01f) return 120;  // fallback 2 min gdy brak kalibracji
    return (uint16_t)(maxDoseMl / flowRateMlS);
}

// Clamp odchylenia do zakresu int8_t
inline int8_t clampDevPct(float dev) {
    if (dev > 127.0f) return 127;
    if (dev < -127.0f) return -127;
    return (int8_t)dev;
}

#endif // ALGORITHM_CONFIG_H

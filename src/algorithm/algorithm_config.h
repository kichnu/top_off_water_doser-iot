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
// LOGGING
// ============================================================
#define LOGGING_TIME                 5  // s — czas fazy logowania po cyklu (bez błędu)

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
#define DEFAULT_DOSE_ML             150     // Stała dawka wody na jeden cykl [ml]
#define DEFAULT_DAILY_LIMIT_ML     2000     // Limit dobowy niezależny od algorytmu [ml]
#define DEFAULT_EMA_ALPHA          0.20f    // Współczynnik wygładzania EMA [0.10–0.40]
#define DEFAULT_RATE_YELLOW_SIGMA   150     // Próg żółty — odchylenie w jednostkach sigma [%] (1.5×)
#define DEFAULT_RATE_RED_SIGMA      250     // Próg czerwony — odchylenie w jednostkach sigma [%] (2.5×)
#define DEFAULT_HISTORY_WINDOW_S  86400     // Okno historii [s] — domyślnie 24h
#define DEFAULT_MIN_BOOTSTRAP         5     // Min. dolewek przed aktywacją alertów

// ============================================================
// SPRAWDZENIA INTEGRALNOŚCI STAŁYCH
// ============================================================
static_assert(DEBOUNCE_INTERVAL_MS >= 50 && DEBOUNCE_INTERVAL_MS <= 2000,
    "DEBOUNCE_INTERVAL_MS must be 50-2000 ms");
static_assert(DEBOUNCE_COUNTER >= 2 && DEBOUNCE_COUNTER <= 20,
    "DEBOUNCE_COUNTER must be 2-20");
static_assert(DEFAULT_DOSE_ML >= 50 && DEFAULT_DOSE_ML <= 2000,
    "DEFAULT_DOSE_ML must be 50-2000 ml");

// ============================================================
// STANY ALGORYTMU
// ============================================================
enum AlgorithmState {
    STATE_IDLE = 0,         // Oczekiwanie — sensor callback wyzwoli przejście
    STATE_DEBOUNCING,       // Debouncing czujnika w toku (obsługiwany przez water_sensors)
    STATE_PUMPING,          // Pompa aktywna — stała dawka, czekanie na zakończenie
    STATE_LOGGING,          // Krótka faza po cyklu — zapis do FRAM, obliczenie EMA
    STATE_ERROR,            // Błąd — oczekiwanie na reset (GUI lub przycisk)
    STATE_MANUAL_OVERRIDE   // Ręczna pompa aktywna
};

// ============================================================
// KODY BŁĘDÓW
// Każdy niezerowy kod → STATE_ERROR, wymaga resetu
// ============================================================
enum ErrorCode {
    ERROR_NONE        = 0,
    ERROR_DAILY_LIMIT = 1,  // Przekroczono dobowy limit wody
    ERROR_RED_ALERT   = 2,  // Czerwony alert — anomalia tempa zużycia wody
    ERROR_BOTH        = 3   // Oba błędy jednocześnie
};

// ============================================================
// REKORD DOLEWKI — zapisywany do ring buffer FRAM po każdym cyklu
// Rozmiar: 4+4+4+2+1+1+1+1+2 = 20 bajtów
// ============================================================
struct TopOffRecord {
    uint32_t timestamp;       // Unix timestamp UTC (z DS3231 RTC)
    uint32_t interval_s;      // Czas od poprzedniej dolewki [s] (0 = brak poprzedniego)
    float    rate_ml_h;       // Tempo zużycia wody [ml/h] = dose_ml / interval_s × 3600
    uint16_t cycle_num;       // Sekwencyjny numer cyklu (ciągły między restartami)
    int8_t   dev_rate_sigma;  // Odchylenie tempa w jednostkach sigma: (rate - EMA_rate) / EMA_dev × 100
    uint8_t  alert_level;     // 0 = OK, 1 = ŻÓŁTY (warning), 2 = CZERWONY (error)
    uint8_t  hour_of_day;     // Godzina lokalna (0–23) — analiza rytmu dobowego parowania
    uint8_t  flags;           // Bitmaska flag (patrz poniżej)
    uint8_t  _pad[2];         // Wyrównanie do wielokrotności 4 bajtów

    // Flags bitmask
    static const uint8_t FLAG_MANUAL    = 0x01;  // Dolewka wyzwolona ręcznie z GUI
    static const uint8_t FLAG_BOOTSTRAP = 0x02;  // Rekord z okresu budowania historii EMA
    static const uint8_t FLAG_DAILY_LIM = 0x04;  // Przekroczono limit dobowy po tym cyklu
    static const uint8_t FLAG_RED_ALERT = 0x08;  // Czerwony alert — system zatrzymany
};
static_assert(sizeof(TopOffRecord) == 20, "TopOffRecord must be exactly 20 bytes");

// ============================================================
// BLOK EMA — persystowany w FRAM, przeżywa restart
// Rozmiar: 4+4+4+1+3 = 16 bajtów
// ============================================================
struct EmaBlock {
    float   ema_rate_ml_h;    // EMA tempa zużycia wody [ml/h]
    float   ema_interval_s;   // EMA typowego interwału między dolewkami [s]
    float   ema_dev_ml_h;     // EMA absolutnych odchyleń od ema_rate [ml/h] — podstawa dynamicznych progów
    uint8_t bootstrap_count;  // Liczba zebranych dolewek (do DEFAULT_MIN_BOOTSTRAP)
    uint8_t _pad[3];
};
static_assert(sizeof(EmaBlock) == 16, "EmaBlock must be exactly 16 bytes");

// ============================================================
// KONFIGURACJA ALGORYTMU — persystowana w FRAM, nadpisuje defaults z kodu
// Rozmiar: 4+4+2+2+1+1+1+1+4 = 20 bajtów
// ============================================================
struct TopOffConfig {
    float    ema_alpha;           // Współczynnik EMA [0.10–0.40]
    uint32_t history_window_s;    // Okno historii [s] (86400 / 129600 / 172800)
    uint16_t dose_ml;             // Stała dawka wody na jeden cykl [ml]
    uint16_t daily_limit_ml;      // Dobowy limit bezpieczeństwa [ml] — niezależny od EMA
    uint8_t  rate_yellow_sigma;   // Próg żółty — mnożnik sigma [%], np. 150 = 1.5× EMA_dev
    uint8_t  rate_red_sigma;      // Próg czerwony — mnożnik sigma [%], np. 250 = 2.5× EMA_dev
    uint8_t  is_configured;       // 0xA7 = konfiguracja zapisana w FRAM (nowa wersja)
    uint8_t  _pad[1];             // Wyrównanie
    uint16_t available_ml;        // Dostępna woda w zbiorniku [ml] — dekrementowana po każdym cyklu
    uint16_t available_max_ml;    // Pojemność zbiornika przy ostatnim uzupełnieniu [ml]
};
static_assert(sizeof(TopOffConfig) == 20, "TopOffConfig must be exactly 20 bytes");

// Zmiana magic relative do poprzedniej wersji — wymusza reset konfiguracji FRAM
#define TOPOFF_CONFIG_MAGIC  0xA7

// ============================================================
// POMOCNICZE OBLICZENIA
// ============================================================

// Czas dozowania [s] wynikający z dawki i kalibracji pompy
inline uint16_t calculateDoseTime(uint16_t doseMl, float flowRateMlS) {
    if (flowRateMlS <= 0.01f) return 60;  // fallback 1 min gdy brak kalibracji
    uint16_t t = (uint16_t)(doseMl / flowRateMlS);
    return (t < 1) ? 1 : t;
}

// Clamp odchylenia sigma do zakresu int8_t
inline int8_t clampSigma(float dev) {
    if (dev > 127.0f) return 127;
    if (dev < -127.0f) return -127;
    return (int8_t)dev;
}

#endif // ALGORITHM_CONFIG_H

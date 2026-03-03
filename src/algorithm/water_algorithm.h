#ifndef WATER_ALGORITHM_H
#define WATER_ALGORITHM_H

#include "algorithm_config.h"
#include "../hardware/fram_controller.h"

// ============================================================
// WaterAlgorithm — maszyna stanów sterująca pompą top-off.
//
// Stany: IDLE → DEBOUNCING → PUMPING → LOGGING → IDLE
//
// Dozowanie dynamiczne (Wariant B + timeout):
//   Pompa startuje po potwierdzeniu debouncingu czujnika.
//   Zatrzymuje się gdy czujnik wróci do HIGH (woda dolana)
//   lub gdy upłynie max_pump_time (limit bezpieczeństwa).
//   Faktyczna objętość = czas_pracy × flow_rate.
// ============================================================

class WaterAlgorithm {
private:
    AlgorithmState currentState;

    // ---- Timing ----
    uint32_t stateStartMs;       // millis() ostatniej zmiany stanu
    uint32_t pumpStartMs;        // millis() startu pompy w bieżącym cyklu
    uint32_t maxPumpDurationMs;  // Limit bezpieczeństwa [ms] = max_dose_ml / flow_rate

    // ---- Release verification (STATE_PUMPING) ----
    uint32_t lastReleaseCheckMs; // millis() ostatniego sprawdzenia czujnika po start pompy
    uint8_t  releaseCounter;     // Licznik kolejnych HIGH po uruchomieniu pompy
    bool     lastCycleTimeout;   // true = ostatni cykl zatrzymany przez timeout

    // ---- Tracking interwałów ----
    uint32_t lastTopOffTimestamp; // Unix UTC ostatniej zakończonej dolewki

    // ---- Rolling 24h (cache, przeliczany po każdym cyklu) ----
    uint16_t rolling24hVolumeMl;

    // ---- Konfiguracja (FRAM lub defaults) ----
    TopOffConfig config;

    // ---- EMA (persystowane w FRAM) ----
    EmaBlock ema;

    // ---- Obsługa błędów ----
    ErrorCode lastError;
    bool      errorSignalActive;
    uint32_t  errorSignalStart;
    uint8_t   errorPulseCount;
    bool      errorPulseState;

    // ---- Flagi stanu ----
    bool systemWasDisabled;
    bool cycleLogged;

    // ---- Metody prywatne ----
    void handleSystemDisable();

    // Pump control
    void startAutoPump();
    void finishPumpCycle();
    void updateReleaseVerification();

    // EMA i metryki
    void    updateEMA(uint16_t volumeMl, uint32_t intervalS, float rateMlH);
    uint8_t calculateAlertLevel(int8_t devVolPct, int8_t devRatePct) const;
    int8_t  calculateDevPct(float current, float ema_val) const;
    uint16_t scanRolling24h() const;   // Skanuje ring buffer FRAM

    // Error signal
    void startErrorSignal(ErrorCode error);
    void updateErrorSignal();
    void checkResetButton();

    // FRAM
    void loadConfigFromFRAM();
    void loadEmaFromFRAM();
    void saveEmaToFRAM();

public:
    WaterAlgorithm();

    // Wywołaj z setup() po initFRAM()
    void initFromFRAM();

    // Wywołaj z loop()
    void update();

    // ---- Callbacki z water_sensors ----
    void onDebounceStart();     // Pierwsze LOW wykryte — debouncing startuje
    void onDebounceComplete();  // Debouncing zaliczony — czas na pompę
    void onDebounceReset();     // Sensor wrócił do HIGH podczas debouncing

    // ---- Status ----
    AlgorithmState getState()            const { return currentState; }
    const char*    getStateString()      const;
    String         getStateDescription() const;
    bool           isInCycle()           const;
    uint32_t       getRemainingSeconds() const;
    ErrorCode      getLastError()        const { return lastError; }
    uint16_t       getRolling24hVolume() const { return rolling24hVolumeMl; }
    uint32_t       getPumpElapsedMs()    const;

    // ---- Konfiguracja ----
    const TopOffConfig& getConfig() const { return config; }
    void setConfig(const TopOffConfig& cfg);

    // ---- EMA (tylko do odczytu dla API) ----
    const EmaBlock& getEma() const { return ema; }

    // ---- Interfejs ręczny (manual pump) ----
    bool requestManualPump(uint16_t durationMs);
    void onManualPumpComplete();
    void addManualVolume(uint16_t volumeMl);  // Wywoływane przez pump_controller

    // ---- Reset / odzyskiwanie ----
    void resetFromError();
    bool resetSystem();

    // ---- System disable ----
    bool wasSystemDisabled() const { return systemWasDisabled; }
    void clearSystemWasDisabled()  { systemWasDisabled = false; }
};

extern WaterAlgorithm waterAlgorithm;

#endif // WATER_ALGORITHM_H

#ifndef WATER_ALGORITHM_H
#define WATER_ALGORITHM_H

#include "algorithm_config.h"
#include "../hardware/fram_controller.h"

// ============================================================
// WaterAlgorithm — maszyna stanów sterująca pompą top-off.
//
// Stany: IDLE → DEBOUNCING → PUMPING → LOGGING → IDLE
//                                     ↘ STATE_ERROR (limit dobowy / czerwony alert)
//
// Dozowanie stałe:
//   Pompa startuje po potwierdzeniu debouncingu czujnika.
//   Pracuje przez stały czas = dose_ml / flow_rate [s].
//   Dawka (dose_ml) konfigurowana w GUI/provisioning.
//   Rate [ml/h] = dose_ml / interval_od_poprzedniej × 3600.
//   EMA śledzi rate i typowe odchylenia (ema_dev_ml_h) → dynamiczne progi alertów.
// ============================================================

class WaterAlgorithm {
private:
    AlgorithmState currentState;

    // ---- Timing ----
    uint32_t stateStartMs;        // millis() ostatniej zmiany stanu
    uint32_t pumpStartMs;         // millis() startu pompy w bieżącym cyklu

    // ---- Tracking interwałów ----
    uint32_t lastTopOffTimestamp; // Unix UTC ostatniej zakończonej dolewki
    uint16_t totalCycleCount;     // Sekwencyjny numer cyklu (ciągły między restartami)

    // ---- Rolling 24h (cache, przeliczany po każdym cyklu) ----
    uint16_t rolling24hVolumeMl;
    uint32_t manualResetTs;   // RAM-only: ignore records older than this (0 = inactive)

    // ---- Konfiguracja (FRAM lub defaults) ----
    TopOffConfig config;

    // ---- EMA (persystowane w FRAM) ----
    EmaBlock ema;

    // ---- Obsługa błędów ----
    ErrorCode lastError;

    // ---- Czujnik dostępności wody ----
    uint8_t lowReservoirCount;  // Licznik kolejnych wykryć LOW (w RAM, reset przy restarcie)

    // ---- Flagi stanu ----
    bool systemWasDisabled;
    bool cycleLogged;

    // ---- Metody prywatne ----
    void handleSystemDisable();

    // Pump control
    void startAutoPump();
    void finishPumpCycle();

    // Czujnik dostępności wody
    void checkAvailableWaterSensor();

    // EMA i metryki
    void    updateEMA(uint32_t intervalS, float rateMlH);
    uint8_t calculateAlertLevel(int8_t devRateSigma) const;
    int8_t  calculateDevSigma(float rateMlH) const;
    uint16_t scanRolling24h() const;

    // Error handling
    void startErrorSignal(ErrorCode error);
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
    uint16_t       getTotalCycleCount()  const { return totalCycleCount; }

    // ---- Czujnik dostępności wody ----
    uint8_t getLowReservoirCount()   const { return lowReservoirCount; }
    bool    isLowReservoirWarning()  const { return lowReservoirCount > 0 && lowReservoirCount < LOW_RESERVOIR_CRITICAL_COUNT; }

    // ---- Konfiguracja ----
    const TopOffConfig& getConfig() const { return config; }
    void setConfig(const TopOffConfig& cfg);

    // ---- EMA (tylko do odczytu dla API) ----
    const EmaBlock& getEma() const { return ema; }

    // ---- Interfejs ręczny (manual pump) ----
    bool requestManualPump(uint16_t durationMs);
    void onManualPumpComplete();
    void addManualVolume(uint16_t volumeMl);

    // ---- Reset / odzyskiwanie ----
    void resetFromError();
    bool resetSystem();
    bool resetCycleHistory();  // Czyści ring buffer FRAM + in-memory EMA
    void resetRolling24h();    // RAM-only reset: zeruje licznik dobowy bez kasowania historii

    // ---- System disable ----
    bool wasSystemDisabled() const { return systemWasDisabled; }
    void clearSystemWasDisabled()  { systemWasDisabled = false; }
};

extern WaterAlgorithm waterAlgorithm;

#endif // WATER_ALGORITHM_H

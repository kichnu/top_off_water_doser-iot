#include "water_algorithm.h"
#include "../core/logging.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../hardware/hardware_pins.h"
#include "../hardware/rtc_controller.h"
#include "../hardware/fram_controller.h"
#include "../config/config.h"
#include "algorithm_config.h"
#include <time.h>

WaterAlgorithm waterAlgorithm;

// ============================================================
// Konstruktor
// ============================================================
WaterAlgorithm::WaterAlgorithm() {
    currentState     = STATE_IDLE;
    stateStartMs     = 0;
    pumpStartMs      = 0;

    lastTopOffTimestamp = 0;
    totalCycleCount     = 0;
    rolling24hVolumeMl  = 0;

    config.dose_ml           = DEFAULT_DOSE_ML;
    config.daily_limit_ml    = DEFAULT_DAILY_LIMIT_ML;
    config.ema_alpha         = DEFAULT_EMA_ALPHA;
    config.rate_yellow_sigma = DEFAULT_RATE_YELLOW_SIGMA;
    config.rate_red_sigma    = DEFAULT_RATE_RED_SIGMA;
    config.history_window_s  = DEFAULT_HISTORY_WINDOW_S;
    config.is_configured     = 0x00;

    ema.ema_rate_ml_h  = 0.0f;
    ema.ema_interval_s = 0.0f;
    ema.ema_dev_ml_h   = 0.0f;
    ema.bootstrap_count = 0;

    lastError = ERROR_NONE;

    lowReservoirCount = 0;

    systemWasDisabled = false;
    cycleLogged       = false;

    pinMode(RESET_PIN, INPUT_PULLUP);
    pinMode(AVAILABLE_WATER_SENSOR_PIN, INPUT_PULLUP);

    LOG_INFO("WaterAlgorithm: constructor done");
}

// ============================================================
// initFromFRAM — wywołaj z setup() po initFRAM()
// ============================================================
void WaterAlgorithm::initFromFRAM() {
    // Sprawdź config — od jego ważności zależy czy dane FRAM są kompatybilne
    TopOffConfig framCfg;
    bool configValid = loadTopOffConfigFromFRAM(framCfg) &&
                       framCfg.is_configured == TOPOFF_CONFIG_MAGIC;

    // Sprawdź czy ring buffer nie zawiera danych legacy (count > 60 = korupcja)
    if (!isTopOffRingBufferValid()) {
        LOG_WARNING("WaterAlgorithm: ring buffer count out of range — clearing FRAM data");
        framBusy = true;
        clearTopOffRingBuffer();
        framBusy = false;
    }

    if (configValid) {
        config = framCfg;
        LOG_INFO("WaterAlgorithm: config loaded from FRAM");

        // Config OK → wczytaj EMA i historię
        loadEmaFromFRAM();

        TopOffRecord lastRecord;
        if (loadLastTopOffRecord(lastRecord)) {
            lastTopOffTimestamp = lastRecord.timestamp;
            totalCycleCount     = lastRecord.cycle_num + 1;
            LOG_INFO("WaterAlgorithm: last record restored: ts=%lu cycle=%d",
                     lastTopOffTimestamp, lastRecord.cycle_num);
        }

        rolling24hVolumeMl = scanRolling24h();

    } else {
        // Config niekompatybilny (nowy format 0xA6) → reset FRAM danych
        // Stare rekordy i EMA z poprzedniego formatu są bezużyteczne
        LOG_WARNING("WaterAlgorithm: FRAM config outdated or absent — hard reset");
        LOG_WARNING("  Clearing ring buffer and EMA (FRAM format change)");
        framBusy = true;
        clearTopOffRingBuffer();
        framBusy = false;
        // config, ema, totalCycleCount — zostają przy defaults/zerach z konstruktora
    }

    LOG_INFO("====================================");
    LOG_INFO("WaterAlgorithm: initFromFRAM done");
    LOG_INFO("  dose_ml        : %d ml", config.dose_ml);
    LOG_INFO("  daily_limit_ml : %d ml", config.daily_limit_ml);
    LOG_INFO("  ema_alpha      : %.2f", config.ema_alpha);
    LOG_INFO("  bootstrap_count: %d/%d", ema.bootstrap_count, DEFAULT_MIN_BOOTSTRAP);
    LOG_INFO("  ema_rate       : %.2f ml/h", ema.ema_rate_ml_h);
    LOG_INFO("  ema_dev        : %.2f ml/h", ema.ema_dev_ml_h);
    LOG_INFO("  rolling_24h    : %d ml", rolling24hVolumeMl);
    LOG_INFO("  next_cycle_num : %d", totalCycleCount);
    LOG_INFO("====================================");
}

// ============================================================
// FRAM — ładowanie i zapis konfiguracji (pomocnicze)
// ============================================================
void WaterAlgorithm::loadConfigFromFRAM() {
    // Pusta — logika przeniesiona do initFromFRAM() dla spójności resetu
}

void WaterAlgorithm::loadEmaFromFRAM() {
    EmaBlock framEma;
    if (loadEmaBlockFromFRAM(framEma)) {
        ema = framEma;
        LOG_INFO("WaterAlgorithm: EMA loaded — bootstrap=%d rate=%.2f dev=%.2f",
                 ema.bootstrap_count, ema.ema_rate_ml_h, ema.ema_dev_ml_h);
    } else {
        LOG_INFO("WaterAlgorithm: EMA absent in FRAM — starting from zero");
    }
}

void WaterAlgorithm::saveEmaToFRAM() {
    framBusy = true;
    saveEmaBlockToFRAM(ema);
    framBusy = false;
}

// ============================================================
// GŁÓWNA PĘTLA
// ============================================================
void WaterAlgorithm::update() {
    checkResetButton();
    checkPumpAutoEnable();

    if (isSystemDisabled()) {
        handleSystemDisable();
        return;
    }

    if (systemWasDisabled) {
        LOG_INFO("SYSTEM RE-ENABLED — sensor state will be picked up by water_sensors");
        systemWasDisabled = false;
        resetSensorProcess();
    }

    switch (currentState) {

        case STATE_IDLE:
            break;

        case STATE_DEBOUNCING:
            // Obsługiwane przez water_sensors.cpp — callback onDebounceComplete()
            break;

        case STATE_PUMPING:
            // Pompa pracuje stały czas — pump_controller zatrzyma ją automatycznie.
            // Gdy isPumpActive() == false, cykl jest zakończony.
            if (!isPumpActive()) {
                finishPumpCycle();
            }
            break;

        case STATE_LOGGING:
            if (millis() - stateStartMs >= (uint32_t)(LOGGING_TIME * 1000)) {
                LOG_INFO("LOGGING complete — returning to IDLE");
                currentState = STATE_IDLE;
                stateStartMs = millis();
                cycleLogged  = false;
            }
            break;

        case STATE_ERROR:
            // Oczekiwanie na reset — obsługiwane przez checkResetButton()
            break;

        case STATE_MANUAL_OVERRIDE:
            // Obsługiwane przez pump_controller — callback onManualPumpComplete()
            break;
    }
}

// ============================================================
// CALLBACKI Z water_sensors
// ============================================================

void WaterAlgorithm::onDebounceStart() {
    if (currentState != STATE_IDLE) return;
    currentState = STATE_DEBOUNCING;
    stateStartMs = millis();
    LOG_INFO("ALGORITHM: IDLE → DEBOUNCING");
}

void WaterAlgorithm::onDebounceComplete() {
    if (currentState != STATE_DEBOUNCING) return;
    resetSensorProcess();
    checkAvailableWaterSensor();
    if (currentState == STATE_ERROR) return;  // alarm krytyczny — nie startuj pompy
    startAutoPump();
}

// ============================================================
// CZUJNIK DOSTĘPNOŚCI WODY — sprawdzenie po debouncing
// ============================================================

void WaterAlgorithm::checkAvailableWaterSensor() {
    bool sensorLow = (digitalRead(AVAILABLE_WATER_SENSOR_PIN) == LOW);

    if (sensorLow) {
        if (lowReservoirCount < 255) lowReservoirCount++;

        if (lowReservoirCount >= LOW_RESERVOIR_CRITICAL_COUNT) {
            LOG_WARNING("LOW RESERVOIR CRITICAL: count=%d → STATE_ERROR", lowReservoirCount);
            startErrorSignal(ERROR_LOW_RESERVOIR);
        } else {
            LOG_WARNING("LOW RESERVOIR WARNING: count=%d/%d — system continues",
                        lowReservoirCount, LOW_RESERVOIR_CRITICAL_COUNT - 1);
        }
    } else {
        if (lowReservoirCount > 0) {
            LOG_INFO("Available water sensor OK — alarm counter reset (%d → 0)", lowReservoirCount);
        }
        lowReservoirCount = 0;
    }
}

void WaterAlgorithm::onDebounceReset() {
    if (currentState == STATE_DEBOUNCING) {
        LOG_INFO("ALGORITHM: Debounce reset — back to IDLE");
        currentState = STATE_IDLE;
        stateStartMs = millis();
    }
}

// ============================================================
// PUMP CONTROL — stała dawka
// ============================================================

void WaterAlgorithm::startAutoPump() {
    uint16_t doseTimeS = calculateDoseTime(config.dose_ml, currentPumpSettings.volumePerSecond);

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("AUTO PUMP START");
    LOG_INFO("  dose_ml      : %d ml", config.dose_ml);
    LOG_INFO("  flow_rate    : %.2f ml/s (current)", currentPumpSettings.volumePerSecond);
    LOG_INFO("  dose_time    : %d s", doseTimeS);
    LOG_INFO("  rolling_24h  : %d / %d ml", rolling24hVolumeMl, config.daily_limit_ml);

    if (ema.bootstrap_count >= DEFAULT_MIN_BOOTSTRAP && ema.ema_dev_ml_h > 0.01f) {
        float yDev = config.rate_yellow_sigma / 100.0f * ema.ema_dev_ml_h;
        float rDev = config.rate_red_sigma    / 100.0f * ema.ema_dev_ml_h;
        LOG_INFO("  EMA rate     : %.1f ml/h", ema.ema_rate_ml_h);
        LOG_INFO("  warn range   : [%.1f – %.1f] ml/h (±%.1f%%σ)",
                 ema.ema_rate_ml_h - yDev, ema.ema_rate_ml_h + yDev,
                 (float)config.rate_yellow_sigma);
        LOG_INFO("  err  range   : [%.1f – %.1f] ml/h (±%.1f%%σ)",
                 ema.ema_rate_ml_h - rDev, ema.ema_rate_ml_h + rDev,
                 (float)config.rate_red_sigma);
    } else {
        LOG_INFO("  EMA rate     : %.1f ml/h (bootstrap %d/%d)",
                 ema.ema_rate_ml_h, ema.bootstrap_count, DEFAULT_MIN_BOOTSTRAP);
    }
    LOG_INFO("====================================");

    pumpStartMs = millis();
    triggerPump(doseTimeS, "AUTO_PUMP");

    currentState = STATE_PUMPING;
    stateStartMs = millis();
}

// ============================================================
// FINALIZACJA CYKLU — wywoływana gdy pompa skończyła pracę
// ============================================================

void WaterAlgorithm::finishPumpCycle() {
    // Timestamp i godzina lokalna
    uint32_t nowTs = getUnixTimestamp();
    time_t   t     = (time_t)nowTs;
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    uint8_t hourOfDay = (uint8_t)tmLocal.tm_hour;

    // Interwał od poprzedniej dolewki
    uint32_t intervalS = (lastTopOffTimestamp > 0 && nowTs > lastTopOffTimestamp)
                         ? (nowTs - lastTopOffTimestamp) : 0;
    lastTopOffTimestamp = nowTs;

    // Rate [ml/h] — sensowne dopiero od 2. zdarzenia
    float rateMlH = (intervalS > 0)
                    ? ((float)config.dose_ml / intervalS * 3600.0f)
                    : 0.0f;

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("CYCLE COMPLETE");
    LOG_INFO("  dose_ml   : %d ml", config.dose_ml);
    LOG_INFO("  interval  : %lu s", intervalS);
    LOG_INFO("  rate      : %.2f ml/h", rateMlH);
    LOG_INFO("  hour      : %d", hourOfDay);
    LOG_INFO("====================================");

    // Aktualizuj EMA (od 2. zdarzenia gdy rate jest znane)
    if (intervalS > 0) {
        updateEMA(intervalS, rateMlH);
    } else {
        // Pierwsze zdarzenie — inicjuj EMA_interval, rate pozostaje 0
        ema.ema_interval_s = 0.0f;
        if (ema.bootstrap_count < 255) ema.bootstrap_count++;
    }

    // Odchylenie w sigma i poziom alertu
    int8_t  devRateSigma = calculateDevSigma(rateMlH);
    uint8_t alertLevel   = calculateAlertLevel(devRateSigma);

    bool redAlertHit = (alertLevel == 2 && ema.bootstrap_count > DEFAULT_MIN_BOOTSTRAP);

    // Buduj rekord — flagi dobowego limitu ustawiamy po zapisie
    TopOffRecord record;
    record.timestamp      = nowTs;
    record.interval_s     = intervalS;
    record.rate_ml_h      = rateMlH;
    record.cycle_num      = totalCycleCount++;
    record.dev_rate_sigma = devRateSigma;
    record.alert_level    = alertLevel;
    record.hour_of_day    = hourOfDay;
    record.flags          = 0;
    if (ema.bootstrap_count <= DEFAULT_MIN_BOOTSTRAP) record.flags |= TopOffRecord::FLAG_BOOTSTRAP;
    if (redAlertHit)                                  record.flags |= TopOffRecord::FLAG_RED_ALERT;

    // Zapisz do FRAM — od teraz bieżący cykl jest w ring buffer
    framBusy = true;
    saveTopOffRecord(record);
    saveEmaToFRAM();
    framBusy = false;

    // Skanuj rolling_24h po zapisie — bieżący cykl uwzględniony
    rolling24hVolumeMl = scanRolling24h();

    // Dekrementuj dostępną wodę w zbiorniku
    if (config.available_max_ml > 0) {
        config.available_ml = (config.available_ml >= config.dose_ml)
                              ? config.available_ml - config.dose_ml : 0;
        framBusy = true;
        saveTopOffConfigToFRAM(config);
        framBusy = false;
    }

    bool dailyLimitHit = (rolling24hVolumeMl >= config.daily_limit_ml);

    LOG_INFO("METRICS: dev_sigma=%d alert=%d bootstrap=%d/%d rolling=%d/%d ml avail=%d/%d ml",
             devRateSigma, alertLevel, ema.bootstrap_count, DEFAULT_MIN_BOOTSTRAP,
             rolling24hVolumeMl, config.daily_limit_ml,
             config.available_ml, config.available_max_ml);

    // Obsługa błędów — po zapisaniu wszystkich danych
    if (dailyLimitHit) {
        LOG_WARNING("DAILY LIMIT: %d ml >= %d ml — entering ERROR state",
                    rolling24hVolumeMl, config.daily_limit_ml);
        startErrorSignal(ERROR_DAILY_LIMIT);
        return;
    }

    if (redAlertHit) {
        LOG_WARNING("RED ALERT: rate anomaly detected — entering ERROR state");
        startErrorSignal(ERROR_RED_ALERT);
        return;
    }

    // Normalny przebieg — LOGGING
    currentState = STATE_LOGGING;
    stateStartMs = millis();
    cycleLogged  = true;
}

// ============================================================
// EMA — obliczenia
// ============================================================

void WaterAlgorithm::updateEMA(uint32_t intervalS, float rateMlH) {
    float alpha = config.ema_alpha;

    if (ema.bootstrap_count == 0) {
        // Pierwsze zdarzenie z rate — inicjuj EMA bezpośrednio
        ema.ema_rate_ml_h  = rateMlH;
        ema.ema_interval_s = (float)intervalS;
        ema.ema_dev_ml_h   = 0.0f;  // Brak danych o odchyleniu jeszcze
    } else {
        float dev = fabsf(rateMlH - ema.ema_rate_ml_h);
        ema.ema_rate_ml_h  = alpha * rateMlH   + (1.0f - alpha) * ema.ema_rate_ml_h;
        ema.ema_interval_s = alpha * intervalS  + (1.0f - alpha) * ema.ema_interval_s;
        ema.ema_dev_ml_h   = alpha * dev        + (1.0f - alpha) * ema.ema_dev_ml_h;
    }

    if (ema.bootstrap_count < 255) ema.bootstrap_count++;

    LOG_INFO("EMA: rate=%.2f ml/h dev=%.2f ml/h int=%.0fs (bootstrap=%d)",
             ema.ema_rate_ml_h, ema.ema_dev_ml_h, ema.ema_interval_s,
             ema.bootstrap_count);
}

int8_t WaterAlgorithm::calculateDevSigma(float rateMlH) const {
    // Brak danych lub bootstrap — nie liczymy odchyleń
    if (ema.bootstrap_count < DEFAULT_MIN_BOOTSTRAP) return 0;
    if (ema.ema_dev_ml_h < 0.01f) return 0;

    float sigma = (rateMlH - ema.ema_rate_ml_h) / ema.ema_dev_ml_h * 100.0f;
    return clampSigma(sigma);
}

uint8_t WaterAlgorithm::calculateAlertLevel(int8_t devRateSigma) const {
    if (ema.bootstrap_count < DEFAULT_MIN_BOOTSTRAP) return 0;

    int absSigma = devRateSigma < 0 ? -devRateSigma : devRateSigma;

    if (absSigma >= (int)config.rate_red_sigma)    return 2;
    if (absSigma >= (int)config.rate_yellow_sigma)  return 1;
    return 0;
}

// ============================================================
// Rolling 24h — skanuje ring buffer FRAM
// ============================================================

uint16_t WaterAlgorithm::scanRolling24h() const {
    uint32_t nowTs    = getUnixTimestamp();
    uint32_t windowS  = config.history_window_s;
    uint32_t cutoffTs = (nowTs > windowS) ? (nowTs - windowS) : 0;

    uint16_t total = 0;
    TopOffRecord records[TOPOFF_HISTORY_SIZE];
    uint16_t count = loadTopOffHistory(records, TOPOFF_HISTORY_SIZE);

    for (uint16_t i = 0; i < count; i++) {
        if (records[i].timestamp >= cutoffTs) {
            total += config.dose_ml;  // Każdy rekord = jedna stała dawka
        }
    }
    return total;
}

// ============================================================
// Konfiguracja
// ============================================================

void WaterAlgorithm::setConfig(const TopOffConfig& cfg) {
    config = cfg;
    config.is_configured = TOPOFF_CONFIG_MAGIC;

    framBusy = true;
    saveTopOffConfigToFRAM(config);
    framBusy = false;

    LOG_INFO("Config updated: dose=%d ml, daily_limit=%d ml, alpha=%.2f, y_sigma=%d, r_sigma=%d",
             config.dose_ml, config.daily_limit_ml, config.ema_alpha,
             config.rate_yellow_sigma, config.rate_red_sigma);
}

// ============================================================
// SYSTEM DISABLE
// ============================================================

void WaterAlgorithm::handleSystemDisable() {
    if (currentState == STATE_IDLE) {
        if (!systemWasDisabled) {
            LOG_INFO("System disabled while IDLE");
            systemWasDisabled = true;
        }
        return;
    }

    if (currentState == STATE_ERROR || currentState == STATE_LOGGING) {
        if (!systemWasDisabled) {
            systemWasDisabled = true;
        }
        return;
    }

    LOG_WARNING("SYSTEM DISABLE — interrupting cycle (state: %s)", getStateString());

    if (isPumpActive()) {
        stopPump();
        LOG_WARNING("Pump stopped by system disable");
    }

    resetSensorProcess();
    currentState      = STATE_IDLE;
    stateStartMs      = millis();
    systemWasDisabled = true;
}

// ============================================================
// ERROR SIGNAL (GPIO ERROR_SIGNAL_PIN)
// Wszystkie błędy → STATE_ERROR, wymaga resetu
// ============================================================

void WaterAlgorithm::startErrorSignal(ErrorCode error) {
    lastError    = error;
    currentState = STATE_ERROR;
    stateStartMs = millis();
    LOG_WARNING("Error: code=%d → STATE_ERROR", (int)error);
}

// ============================================================
// RESET BUTTON
// ============================================================

void WaterAlgorithm::checkResetButton() {
    static uint32_t pressStart = 0;
    static bool     wasPressed = false;

    bool pressed = (digitalRead(RESET_PIN) == LOW);

    if (pressed && !wasPressed) {
        pressStart = millis();
        wasPressed = true;
    } else if (!pressed && wasPressed) {
        wasPressed = false;
        if (currentState == STATE_ERROR) {
            resetFromError();
        }
    }
}

// ============================================================
// MANUAL PUMP
// ============================================================

bool WaterAlgorithm::requestManualPump(uint16_t durationMs) {
    if (currentState != STATE_IDLE) {
        LOG_WARNING("Manual pump rejected — not in IDLE (state: %s)", getStateString());
        return false;
    }
    currentState = STATE_MANUAL_OVERRIDE;
    stateStartMs = millis();
    LOG_INFO("Manual pump started (%d ms)", durationMs);
    return true;
}

void WaterAlgorithm::onManualPumpComplete() {
    if (currentState == STATE_MANUAL_OVERRIDE) {
        LOG_INFO("Manual pump complete — returning to IDLE");
        currentState = STATE_IDLE;
        stateStartMs = millis();
        resetSensorProcess();
    }
}

void WaterAlgorithm::addManualVolume(uint16_t volumeMl) {
    rolling24hVolumeMl += volumeMl;
    LOG_INFO("Manual volume added: %d ml (rolling 24h: %d ml)", volumeMl, rolling24hVolumeMl);
}

// ============================================================
// RESET / RECOVERY
// ============================================================

void WaterAlgorithm::resetFromError() {
    lastError    = ERROR_NONE;
    currentState = STATE_IDLE;
    stateStartMs = millis();
    resetSensorProcess();
    LOG_INFO("Error cleared — returned to IDLE");
}

bool WaterAlgorithm::resetSystem() {
    if (currentState == STATE_LOGGING) return false;

    if (isPumpActive()) stopPump();

    lastError    = ERROR_NONE;
    currentState = STATE_IDLE;
    stateStartMs = millis();
    resetSensorProcess();
    LOG_INFO("System reset to IDLE");
    return true;
}

bool WaterAlgorithm::resetCycleHistory() {
    if (isInCycle()) return false;  // Nie kasuj danych gdy cykl w toku

    framBusy = true;
    bool ok = clearTopOffRingBuffer();  // Czyści FRAM: count, wptr, EMA checksum
    framBusy = false;

    // Reset in-memory EMA — inaczej algorytm liczyłby alerty na starych danych
    ema = EmaBlock{};
    lastTopOffTimestamp = 0;
    rolling24hVolumeMl  = 0;

    LOG_INFO("WaterAlgorithm: cycle history and EMA reset");
    return ok;
}

// ============================================================
// STATUS GETTERS
// ============================================================

const char* WaterAlgorithm::getStateString() const {
    switch (currentState) {
        case STATE_IDLE:            return "IDLE";
        case STATE_DEBOUNCING:      return "DEBOUNCING";
        case STATE_PUMPING:         return "PUMPING";
        case STATE_LOGGING:         return "LOGGING";
        case STATE_ERROR:           return "ERROR";
        case STATE_MANUAL_OVERRIDE: return "MANUAL";
        default:                    return "UNKNOWN";
    }
}

String WaterAlgorithm::getStateDescription() const {
    switch (currentState) {
        case STATE_IDLE:
            return "Waiting for sensor";
        case STATE_DEBOUNCING:
            return String("Debouncing sensor (") +
                   getDebounceCounter() + "/" + DEBOUNCE_COUNTER + ")";
        case STATE_PUMPING: {
            uint32_t elapsed = (millis() - pumpStartMs) / 1000UL;
            uint16_t total   = calculateDoseTime(config.dose_ml, currentPumpSettings.volumePerSecond);
            return String("Pumping ") + config.dose_ml + "ml (" + elapsed + "/" + total + "s)";
        }
        case STATE_LOGGING:
            return "Logging cycle data";
        case STATE_ERROR:
            if (lastError == ERROR_LOW_RESERVOIR) return "Reservoir empty — refill and reset";
            if (lastError == ERROR_DAILY_LIMIT)   return "Daily limit reached — reset required";
            if (lastError == ERROR_RED_ALERT)     return "Rate anomaly — reset required";
            return String("Error: code ") + (int)lastError + " — reset required";
        case STATE_MANUAL_OVERRIDE:
            return "Manual pump active";
        default:
            return "Unknown";
    }
}

bool WaterAlgorithm::isInCycle() const {
    return currentState != STATE_IDLE && currentState != STATE_ERROR;
}

uint32_t WaterAlgorithm::getRemainingSeconds() const {
    switch (currentState) {
        case STATE_PUMPING:
            return getPumpRemainingTime();
        case STATE_LOGGING: {
            uint32_t elapsed = millis() - stateStartMs;
            uint32_t total   = (uint32_t)(LOGGING_TIME * 1000);
            if (elapsed >= total) return 0;
            return (total - elapsed) / 1000UL;
        }
        default:
            return 0;
    }
}

uint32_t WaterAlgorithm::getPumpElapsedMs() const {
    if (currentState != STATE_PUMPING) return 0;
    return millis() - pumpStartMs;
}

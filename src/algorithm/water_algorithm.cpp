#include "water_algorithm.h"
#include "../core/logging.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../hardware/hardware_pins.h"
#include "../hardware/rtc_controller.h"
#include "../hardware/fram_controller.h"
#include "../config/config.h"
#include "algorithm_config.h"

WaterAlgorithm waterAlgorithm;

// ============================================================
// Konstruktor — minimalny init, FRAM ładowany później
// ============================================================
WaterAlgorithm::WaterAlgorithm() {
    currentState     = STATE_IDLE;
    stateStartMs     = 0;
    pumpStartMs      = 0;
    maxPumpDurationMs = (uint32_t)calculateMaxPumpTime(DEFAULT_MAX_DOSE_ML,
                            currentPumpSettings.volumePerSecond) * 1000UL;

    lastReleaseCheckMs  = 0;
    releaseCounter      = 0;
    lastCycleTimeout    = false;

    lastTopOffTimestamp = 0;
    rolling24hVolumeMl  = 0;

    // Config — defaulty z kodu (nadpisane z FRAM przez initFromFRAM())
    config.max_dose_ml     = DEFAULT_MAX_DOSE_ML;
    config.ema_alpha       = DEFAULT_EMA_ALPHA;
    config.vol_yellow_pct  = DEFAULT_VOL_YELLOW_PCT;
    config.vol_red_pct     = DEFAULT_VOL_RED_PCT;
    config.rate_yellow_pct = DEFAULT_RATE_YELLOW_PCT;
    config.rate_red_pct    = DEFAULT_RATE_RED_PCT;
    config.history_window_s = DEFAULT_HISTORY_WINDOW_S;
    config.is_configured   = 0x00;

    // EMA — zerowe, bootstrap od zera
    ema.ema_volume_ml  = 0.0f;
    ema.ema_interval_s = 0.0f;
    ema.ema_rate_ml_h  = 0.0f;
    ema.bootstrap_count = 0;

    lastError         = ERROR_NONE;
    errorSignalActive = false;
    errorPulseCount   = 0;
    errorPulseState   = false;
    errorSignalStart  = 0;

    systemWasDisabled = false;
    cycleLogged       = false;

    pinMode(ERROR_SIGNAL_PIN, OUTPUT);
    digitalWrite(ERROR_SIGNAL_PIN, LOW);
    pinMode(RESET_PIN, INPUT_PULLUP);

    LOG_INFO("WaterAlgorithm: constructor done");
}

// ============================================================
// initFromFRAM — wywołaj z setup() po initFRAM()
// ============================================================
void WaterAlgorithm::initFromFRAM() {
    loadConfigFromFRAM();
    loadEmaFromFRAM();

    // Odtwórz last top-off timestamp z najnowszego rekordu FRAM
    TopOffRecord lastRecord;
    if (loadLastTopOffRecord(lastRecord)) {
        lastTopOffTimestamp = lastRecord.timestamp;
        LOG_INFO("WaterAlgorithm: last top-off timestamp restored: %lu", lastTopOffTimestamp);
    }

    // Przelicz rolling 24h
    rolling24hVolumeMl = scanRolling24h();

    // Przelicz timeout pompy z aktualnej kalibracji i config
    maxPumpDurationMs = (uint32_t)calculateMaxPumpTime(
        config.max_dose_ml, currentPumpSettings.volumePerSecond) * 1000UL;

    LOG_INFO("====================================");
    LOG_INFO("WaterAlgorithm: initFromFRAM done");
    LOG_INFO("  max_dose_ml    : %d ml", config.max_dose_ml);
    LOG_INFO("  ema_alpha      : %.2f", config.ema_alpha);
    LOG_INFO("  history_window : %lu s", config.history_window_s);
    LOG_INFO("  bootstrap_count: %d/%d", ema.bootstrap_count, DEFAULT_MIN_BOOTSTRAP);
    LOG_INFO("  rolling_24h    : %d ml", rolling24hVolumeMl);
    LOG_INFO("  max_pump_time  : %lu s", maxPumpDurationMs / 1000UL);
    LOG_INFO("====================================");
}

// ============================================================
// FRAM — ładowanie i zapis konfiguracji
// ============================================================
void WaterAlgorithm::loadConfigFromFRAM() {
    TopOffConfig framCfg;
    if (loadTopOffConfigFromFRAM(framCfg) && framCfg.is_configured == TOPOFF_CONFIG_MAGIC) {
        config = framCfg;
        LOG_INFO("WaterAlgorithm: config loaded from FRAM");
    } else {
        LOG_INFO("WaterAlgorithm: FRAM config absent — using defaults");
    }
}

void WaterAlgorithm::loadEmaFromFRAM() {
    EmaBlock framEma;
    if (loadEmaBlockFromFRAM(framEma)) {
        ema = framEma;
        LOG_INFO("WaterAlgorithm: EMA loaded — bootstrap=%d, vol=%.1f, rate=%.2f",
                 ema.bootstrap_count, ema.ema_volume_ml, ema.ema_rate_ml_h);
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
    updateErrorSignal();
    checkPumpAutoEnable();

    // ---- Obsługa wyłączenia systemu ----
    if (isSystemDisabled()) {
        handleSystemDisable();
        return;
    }

    // ---- Obsługa ponownego włączenia ----
    if (systemWasDisabled) {
        LOG_INFO("SYSTEM RE-ENABLED — sensor state will be picked up by water_sensors");
        systemWasDisabled = false;
        resetSensorProcess();  // Pozwól sensorowi zacząć od nowa
    }

    switch (currentState) {

        case STATE_IDLE:
            // Nic — przejście wyzwalane przez onDebounceComplete()
            break;

        case STATE_DEBOUNCING:
            // Obsługiwane przez water_sensors.cpp — callback onDebounceComplete()
            break;

        case STATE_PUMPING:
            updateReleaseVerification();
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

    // Sprawdź limit 24h zanim wystartuje pompa
    rolling24hVolumeMl = scanRolling24h();
    if (rolling24hVolumeMl >= config.max_dose_ml * 10) {
        // Heurystyka: jeśli suma dobowa przekracza 10× jednorazowy max → alarm
        // Właściwy limit dobowy jako parametr konfiguracyjny — do dodania w v2
        LOG_WARNING("ALGORITHM: 24h limit check — rolling=%d ml", rolling24hVolumeMl);
    }

    resetSensorProcess();
    startAutoPump();
}

void WaterAlgorithm::onDebounceReset() {
    if (currentState == STATE_DEBOUNCING) {
        LOG_INFO("ALGORITHM: Debounce reset — back to IDLE");
        currentState = STATE_IDLE;
        stateStartMs = millis();
    }
}

// ============================================================
// PUMP CONTROL
// ============================================================

void WaterAlgorithm::startAutoPump() {
    // Przelicz timeout z aktualnej kalibracji i config
    maxPumpDurationMs = (uint32_t)calculateMaxPumpTime(
        config.max_dose_ml, currentPumpSettings.volumePerSecond) * 1000UL;

    uint16_t maxPumpTimeS = (uint16_t)(maxPumpDurationMs / 1000UL);
    if (maxPumpTimeS < 1) maxPumpTimeS = 1;

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("AUTO PUMP START");
    LOG_INFO("  max_dose_ml  : %d ml", config.max_dose_ml);
    LOG_INFO("  flow_rate    : %.2f ml/s", currentPumpSettings.volumePerSecond);
    LOG_INFO("  safety_time  : %d s", maxPumpTimeS);
    LOG_INFO("====================================");

    pumpStartMs        = millis();
    lastReleaseCheckMs = pumpStartMs;
    releaseCounter     = 0;
    lastCycleTimeout   = false;

    triggerPump(maxPumpTimeS, "AUTO_PUMP");

    currentState = STATE_PUMPING;
    stateStartMs = millis();
}

// ============================================================
// RELEASE VERIFICATION (STATE_PUMPING)
// ============================================================

void WaterAlgorithm::updateReleaseVerification() {
    uint32_t now = millis();

    // Sprawdź czy pompa sama się zatrzymała (timeout przez pump_controller)
    if (!isPumpActive()) {
        if (!lastCycleTimeout) {
            // Sprawdź czy sensor potwierdził — jeśli pompa stoi ale sensor nie zaliczył release
            bool sensorLow = readWaterSensor();
            if (sensorLow) {
                // Pompa stanęła przez timeout — sensor nadal LOW
                lastCycleTimeout = true;
                LOG_WARNING("");
                LOG_WARNING("PUMP TIMEOUT — sensor did not release!");
            }
        }
        finishPumpCycle();
        return;
    }

    // Odstęp między sprawdzeniami
    if (now - lastReleaseCheckMs < (uint32_t)RELEASE_CHECK_INTERVAL_MS) return;
    lastReleaseCheckMs = now;

    bool sensorHigh = !readWaterSensor();  // HIGH = woda dolana, sensor nieaktywny

    if (sensorHigh) {
        releaseCounter++;
        LOG_INFO("RELEASE: HIGH %d/%d", releaseCounter, RELEASE_DEBOUNCE_COUNT);

        if (releaseCounter >= RELEASE_DEBOUNCE_COUNT) {
            LOG_INFO("");
            LOG_INFO("====================================");
            LOG_INFO("RELEASE: Sensor confirmed — stopping pump");
            LOG_INFO("====================================");
            stopPump();
            lastCycleTimeout = false;
            finishPumpCycle();
        }
    } else {
        if (releaseCounter > 0) {
            LOG_INFO("RELEASE: LOW again — counter reset (was %d)", releaseCounter);
        }
        releaseCounter = 0;
    }
}

// ============================================================
// FINALIZACJA CYKLU — po zatrzymaniu pompy
// ============================================================

void WaterAlgorithm::finishPumpCycle() {
    uint32_t durationMs = millis() - pumpStartMs;
    uint32_t durationS  = durationMs / 1000UL;
    uint16_t volumeMl   = (uint16_t)(durationS * currentPumpSettings.volumePerSecond);

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("CYCLE COMPLETE");
    LOG_INFO("  duration  : %lu s", durationS);
    LOG_INFO("  volume    : %d ml", volumeMl);
    LOG_INFO("  timeout   : %s", lastCycleTimeout ? "YES" : "no");
    LOG_INFO("====================================");

    // Timestamp i interwał
    uint32_t nowTs     = getUnixTimestamp();
    uint32_t intervalS = (lastTopOffTimestamp > 0 && nowTs > lastTopOffTimestamp)
                         ? (nowTs - lastTopOffTimestamp) : 0;
    lastTopOffTimestamp = nowTs;

    // Rate [ml/h]
    float rateMlH = (intervalS > 0)
                    ? ((float)volumeMl / intervalS * 3600.0f)
                    : 0.0f;

    // Aktualizuj EMA
    updateEMA(volumeMl, intervalS, rateMlH);

    // Oblicz odchylenia
    int8_t devVolPct  = calculateDevPct((float)volumeMl, ema.ema_volume_ml);
    int8_t devRatePct = calculateDevPct(rateMlH, ema.ema_rate_ml_h);

    // Poziom alertu
    uint8_t alertLevel = lastCycleTimeout ? 2 : calculateAlertLevel(devVolPct, devRatePct);

    // Flagi
    uint8_t flags = 0;
    if (lastCycleTimeout)                          flags |= TopOffRecord::FLAG_TIMEOUT;
    if (ema.bootstrap_count < DEFAULT_MIN_BOOTSTRAP) flags |= TopOffRecord::FLAG_BOOTSTRAP;

    // Buduj rekord
    TopOffRecord record;
    record.timestamp      = nowTs;
    record.volume_ml      = volumeMl;
    record.interval_s     = intervalS;
    record.rate_ml_h      = rateMlH;
    record.dev_volume_pct = devVolPct;
    record.dev_rate_pct   = devRatePct;
    record.alert_level    = alertLevel;
    record.flags          = flags;

    // Zapis do FRAM
    framBusy = true;
    saveTopOffRecord(record);
    saveEmaToFRAM();
    framBusy = false;

    // Odśwież cache rolling 24h
    rolling24hVolumeMl = scanRolling24h();

    // Error signal dla timeout
    if (lastCycleTimeout) {
        startErrorSignal(ERROR_TIMEOUT);
    }

    LOG_INFO("METRICS: dev_vol=%d%% dev_rate=%d%% alert=%d bootstrap=%d/%d",
             devVolPct, devRatePct, alertLevel,
             ema.bootstrap_count, DEFAULT_MIN_BOOTSTRAP);

    // Przejdź do LOGGING
    currentState = STATE_LOGGING;
    stateStartMs = millis();
    cycleLogged  = true;
}

// ============================================================
// EMA — obliczenia
// ============================================================

void WaterAlgorithm::updateEMA(uint16_t volumeMl, uint32_t intervalS, float rateMlH) {
    float alpha = config.ema_alpha;

    if (ema.bootstrap_count == 0) {
        // Pierwsze zdarzenie — inicjuj EMA bezpośrednio
        ema.ema_volume_ml  = (float)volumeMl;
        ema.ema_interval_s = (float)intervalS;
        ema.ema_rate_ml_h  = rateMlH;
    } else {
        ema.ema_volume_ml  = alpha * volumeMl  + (1.0f - alpha) * ema.ema_volume_ml;
        ema.ema_interval_s = alpha * intervalS + (1.0f - alpha) * ema.ema_interval_s;
        ema.ema_rate_ml_h  = alpha * rateMlH   + (1.0f - alpha) * ema.ema_rate_ml_h;
    }

    if (ema.bootstrap_count < 255) ema.bootstrap_count++;

    LOG_INFO("EMA: vol=%.1f int=%.0fs rate=%.2f ml/h (bootstrap=%d)",
             ema.ema_volume_ml, ema.ema_interval_s, ema.ema_rate_ml_h,
             ema.bootstrap_count);
}

int8_t WaterAlgorithm::calculateDevPct(float current, float ema_val) const {
    if (ema_val < 0.001f || ema.bootstrap_count < DEFAULT_MIN_BOOTSTRAP) return 0;
    float dev = (current - ema_val) / ema_val * 100.0f;
    return clampDevPct(dev);
}

uint8_t WaterAlgorithm::calculateAlertLevel(int8_t devVolPct, int8_t devRatePct) const {
    if (ema.bootstrap_count < DEFAULT_MIN_BOOTSTRAP) return 0;  // Bootstrap — brak alertów

    uint8_t alertVol  = 0;
    uint8_t alertRate = 0;

    int absVol  = devVolPct  < 0 ? -devVolPct  : devVolPct;
    int absRate = devRatePct < 0 ? -devRatePct : devRatePct;

    if (absVol >= config.vol_red_pct)      alertVol  = 2;
    else if (absVol >= config.vol_yellow_pct) alertVol = 1;

    if (absRate >= config.rate_red_pct)       alertRate = 2;
    else if (absRate >= config.rate_yellow_pct) alertRate = 1;

    return (alertVol > alertRate) ? alertVol : alertRate;
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
            total += records[i].volume_ml;
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

    // Przelicz timeout pompy
    maxPumpDurationMs = (uint32_t)calculateMaxPumpTime(
        config.max_dose_ml, currentPumpSettings.volumePerSecond) * 1000UL;

    framBusy = true;
    saveTopOffConfigToFRAM(config);
    framBusy = false;

    LOG_INFO("Config updated: max_dose=%d ml, alpha=%.2f, pump_time=%lu s",
             config.max_dose_ml, config.ema_alpha, maxPumpDurationMs / 1000UL);
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
// ============================================================

void WaterAlgorithm::startErrorSignal(ErrorCode error) {
    lastError         = error;
    errorSignalActive = true;
    errorSignalStart  = millis();
    errorPulseCount   = 0;
    errorPulseState   = false;
    digitalWrite(ERROR_SIGNAL_PIN, LOW);

    LOG_WARNING("Error signal started: code=%d", (int)error);

    if (error != ERROR_TIMEOUT) {
        currentState = STATE_ERROR;
        stateStartMs = millis();
    }
}

void WaterAlgorithm::updateErrorSignal() {
    if (!errorSignalActive) return;

    uint32_t now     = millis();
    uint32_t elapsed = now - errorSignalStart;
    uint8_t  pulses  = (uint8_t)lastError;
    if (pulses == 0) pulses = 1;

    uint32_t cycleDuration = pulses * (ERROR_PULSE_HIGH + ERROR_PULSE_LOW) + ERROR_PAUSE;
    uint32_t pos           = elapsed % cycleDuration;
    uint32_t pulseEnd      = (uint32_t)pulses * (ERROR_PULSE_HIGH + ERROR_PULSE_LOW);

    bool newState;
    if (pos >= pulseEnd) {
        newState = false;  // PAUSE
    } else {
        uint32_t inCycle = pos % (ERROR_PULSE_HIGH + ERROR_PULSE_LOW);
        newState = (inCycle < ERROR_PULSE_HIGH);
    }

    if (newState != errorPulseState) {
        errorPulseState = newState;
        digitalWrite(ERROR_SIGNAL_PIN, newState ? HIGH : LOW);
    }
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
            LOG_INFO("Reset button — clearing error state");
            resetFromError();
        } else if (errorSignalActive) {
            // ERROR_TIMEOUT: alarm aktywny, ale system działa dalej (nie w STATE_ERROR)
            errorSignalActive = false;
            lastError         = ERROR_NONE;
            digitalWrite(ERROR_SIGNAL_PIN, LOW);
            LOG_INFO("Reset button — alarm cleared (timeout warning acknowledged)");
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
    if (currentState != STATE_ERROR) return;

    errorSignalActive = false;
    lastError         = ERROR_NONE;
    digitalWrite(ERROR_SIGNAL_PIN, LOW);

    currentState = STATE_IDLE;
    stateStartMs = millis();
    resetSensorProcess();
    LOG_INFO("Error cleared — returned to IDLE");
}

bool WaterAlgorithm::resetSystem() {
    if (currentState == STATE_LOGGING) return false;

    if (isPumpActive()) stopPump();

    errorSignalActive = false;
    lastError         = ERROR_NONE;
    digitalWrite(ERROR_SIGNAL_PIN, LOW);

    currentState = STATE_IDLE;
    stateStartMs = millis();
    resetSensorProcess();
    LOG_INFO("System reset to IDLE");
    return true;
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
            return String("Pumping (") + elapsed + "s elapsed)";
        }
        case STATE_LOGGING:
            return "Logging cycle data";
        case STATE_ERROR:
            return String("Error: code ") + (int)lastError;
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
        case STATE_PUMPING: {
            uint32_t elapsed = millis() - pumpStartMs;
            if (elapsed >= maxPumpDurationMs) return 0;
            return (maxPumpDurationMs - elapsed) / 1000UL;
        }
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


#include <Arduino.h>
#include <Wire.h>
#include "core/logging.h"
#include "config/config.h"
#include "config/credentials_manager.h"
#include "hardware/rtc_controller.h"
#include "hardware/fram_controller.h"
#include "hardware/hardware_pins.h"
#include "hardware/water_sensors.h"
#include "hardware/pump_controller.h"
#include "network/wifi_manager.h"
#include "security/auth_manager.h"
#include "security/session_manager.h"
#include "security/rate_limiter.h"
#include "web/web_server.h"
#include "algorithm/water_algorithm.h"
#include "algorithm/kalkwasser_scheduler.h"
#include "hardware/mixing_pump.h"
#include "hardware/peristaltic_pump.h"
#include "hardware/audio_player.h"
#include "provisioning/prov_detector.h"
#include "provisioning/ap_core.h"
#include "provisioning/ap_server.h"

void setup() {
    // Initialize core systems
    initLogging();
    delay(5000); // Wait for serial monitor

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("=== ESP32-C3 Water System Starting ===");
    LOG_INFO("Single Mode - Captive Portal Provisioning");
    LOG_INFO("ESP32 Flash size: %d", ESP.getFlashChipSize());
    LOG_INFO("Free heap: %d", ESP.getFreeHeap());
    LOG_INFO("CHECKING PROVISIONING BUTTON");

    // Check if provisioning button is held
    if (checkProvisioningButton()) {
        LOG_INFO("");
        LOG_INFO("====================================");
        LOG_INFO("    ENTERING PROVISIONING MODE");
        LOG_INFO("====================================");
        LOG_INFO("Initializing FRAM for credential storage");

        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        Wire.setClock(100000);

        if (!initFRAM()) {
            LOG_INFO("WARNING: FRAM initialization failed!");
            LOG_INFO("Credentials may not be saved properly");
            delay(2000);
        } else {
            LOG_INFO("");
            LOG_INFO("FRAM ready");
        }

        if (!startAccessPoint()) {
            LOG_INFO("");
            LOG_INFO("FATAL: Failed to start AP - halting");
            while(1) delay(1000);
        }
        
        // Start DNS Server for captive portal
        if (!startDNSServer()) {
            LOG_INFO("");
            LOG_INFO("FATAL: Failed to start DNS - halting");
            while(1) delay(1000);
        }
        
        LOG_INFO("");
        LOG_INFO("====================================");
        LOG_INFO("=== PROVISIONING MODE ACTIVE ===");
        LOG_INFO("Connect to WiFi network:");
        LOG_INFO("SSID: ESP32-WATER-SETUP");
        LOG_INFO("Password: setup12345");
        LOG_INFO("URL: http://");
        LOG_INFO("%s", getAPIPAddress().toString());

        // Start Web Server
        if (!startWebServer()) {
            Serial.println("FATAL: Failed to start web server - halting");
            while(1) delay(1000);
        }
        
        LOG_INFO("");
        LOG_INFO("=== CAPTIVE PORTAL READY ===");
        LOG_INFO("Open a browser or wait for captive portal popup");
  
        // Enter blocking loop - never returns
        runProvisioningLoop();
        
        // This line will never be reached
    }
    
    // === PRODUCTION MODE SETUP ===
    LOG_INFO("");
    LOG_INFO("=== Production Mode - Full Water System ===");

    initWaterSensor();
    initPumpController();

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);  // Init I2C before FRAM
    Wire.setClock(100000);

    initNVS();          // initNVS() wywołuje initFRAM() wewnętrznie (config.cpp)
    loadVolumeFromNVS();
    waterAlgorithm.initFromFRAM();
    initMixingPump();
    initPeristalticPump();
    audioPlayer.init();
    kalkwasserScheduler.init();

    bool credentials_loaded = initCredentialsManager();
    LOG_INFO("");
    if (credentials_loaded) {
        LOG_INFO("Device ID: %s", getDeviceID());
    } else {
        LOG_INFO("FALLBACK_MODE");
    }
    initWiFi();
    initializeRTC();
    delay(2000);

    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("Initializing network...");
    LOG_INFO("[INIT] Initializing RTC...");
    LOG_INFO("RTC Status: %s", getRTCInfo().c_str());
    LOG_INFO("[INIT] Waiting for RTC to stabilize...");
    LOG_INFO("====================================");
    
    if (!isRTCWorking()) {
        LOG_WARNING("⚠️ WARNING: RTC not working properly");
        LOG_WARNING("⚠️ Daily volume tracking may be affected");
        LOG_WARNING("");
    }
    // Initialize security
    initAuthManager();
    initSessionManager();
    initRateLimiter();
    
    // Initialize VPS logger
    // initVPSLogger();
    
    // Initialize web server
    initWebServer();
    
    // Post-init diagnostics
    Serial.println();
    LOG_INFO("");
    LOG_INFO("====================================");
    LOG_INFO("SYSTEM POST-INIT STATUS");
    LOG_INFO("RTC Working: %s", isRTCWorking() ? "YES" : "NO");
    LOG_INFO("RTC Info: %s", getRTCInfo().c_str());
    LOG_INFO("Current Time: %s", getCurrentTimestamp().c_str());
    LOG_INFO("Water Algorithm:");
    LOG_INFO("  State: %s", waterAlgorithm.getStateString());
    LOG_INFO("  Rolling 24h: %d ml", waterAlgorithm.getRolling24hVolume());
    LOG_INFO("  Dose: %d ml, daily limit: %d ml", waterAlgorithm.getConfig().dose_ml, waterAlgorithm.getConfig().daily_limit_ml);
    LOG_INFO("  EMA bootstrap: %d rate=%.2f score=%.2f",
             waterAlgorithm.getEma().bootstrap_count,
             waterAlgorithm.getEma().ema_rate_ml_h,
             waterAlgorithm.getEma().alarm_score);
    
    if (isWiFiConnected()) {
        LOG_INFO("Dashboard: http://");
        LOG_INFO("%s", getLocalIP().toString());
    }
    LOG_INFO("Current Time: %s", getCurrentTimestamp().c_str());
    LOG_INFO("=== System initialization complete ===");
    LOG_INFO("====================================");
}

void loop() {
    // Production mode loop - full water system
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    
    // DAILY RESTART CHECK - 24 godziny
    if (now > 86400000UL) { // 24h w ms
        Serial.println("=== DAILY RESTART: 24h uptime reached ===");
        
        if (isPumpActive())              stopPump();
        if (isMixingPumpActive())        stopMixingPump();
        if (isPeristalticPumpRunning())  stopPeristalticPump();
        audioPlayer.stop();
        delay(1000);
        
        Serial.println("System restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    
    // Sensor + algorytm — każdy cykl loop
    updateWaterSensor();
    waterAlgorithm.update();
    kalkwasserScheduler.update();

    // Pozostałe systemy co 100 ms
    if (now - lastUpdate >= 100) {
        updatePumpController();
        audioPlayer.update();
        updateSessionManager();
        updateRateLimiter();
        updateWiFi();
        lastUpdate = now;
    }

    delay(100);
}
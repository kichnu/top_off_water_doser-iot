
#include "web_handlers.h"
#include "web_server.h"
#include "html_pages.h"
#include "../security/auth_manager.h"
#include "../security/session_manager.h"
#include "../security/rate_limiter.h"
#include "../hardware/pump_controller.h"
#include "../hardware/water_sensors.h"
#include "../hardware/fram_controller.h"
#include "../hardware/rtc_controller.h"
#include "../network/wifi_manager.h"
#include "../config/config.h"
#include "../core/logging.h"
#include <ArduinoJson.h>
#include "../config/credentials_manager.h"
#include "../algorithm/water_algorithm.h"

void handleDashboard(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->redirect("login");
        return;
    }
    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html", (const uint8_t*)DASHBOARD_HTML, strlen(DASHBOARD_HTML));
    request->send(response);
}

void handleLoginPage(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();
    if (!isIPAllowed(clientIP) && !isTrustedProxy(clientIP)) {
        request->send(403, "text/plain", "Forbidden");
        return;
    }

    AsyncWebServerResponse* response = request->beginResponse_P(
        200, "text/html", (const uint8_t*)LOGIN_HTML, strlen(LOGIN_HTML));
    request->send(response);
}

void handleLogin(AsyncWebServerRequest* request) {
    IPAddress clientIP = request->client()->remoteIP();

    // Whitelist check - block login from unknown IPs (proxy allowed)
    if (!isIPAllowed(clientIP) && !isTrustedProxy(clientIP)) {
        LOG_WARNING("Login attempt from non-whitelisted IP: %s", clientIP.toString().c_str());
        request->send(403, "application/json", "{\"success\":false,\"error\":\"Access denied\"}");
        return;
    }

    if (isRateLimited(clientIP) || isIPBlocked(clientIP)) {
        request->send(429, "text/plain", "Too Many Requests");
        return;
    }

    // Check if FRAM credentials are loaded
    if (!areCredentialsLoaded()) {
        recordFailedAttempt(clientIP);
        JsonDocument error_response;
        error_response["success"] = false;
        error_response["error"] = "System not configured";
        error_response["message"] = "FRAM credentials required. Use Captive Portal to configure.";
        error_response["setup_instructions"] = "1. Hold button 5s during boot  2. Connect to ESP32-WATER-SETUP  3. Configure credentials in browser";
        
        String response_str;
        serializeJson(error_response, response_str);
        request->send(503, "application/json", response_str);  // Service Unavailable
        return;
    }
 
    if (!request->hasParam("password", true)) {
        recordFailedAttempt(clientIP);
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing password\"}");
        return;
    }
    
    String password = request->getParam("password", true)->value();
    
    if (verifyPassword(password)) {
        String token = createSession(clientIP);
        String cookie = "session_token=" + token + "; Path=/; HttpOnly; Max-Age=" + String(SESSION_TIMEOUT_MS / 1000);
        
        AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true}");
        response->addHeader("Set-Cookie", cookie);
        request->send(response);
    } else {
        recordFailedAttempt(clientIP);
                
        JsonDocument error_response;
        error_response["success"] = false;
        error_response["error"] = "Invalid password";
        
        if (!areCredentialsLoaded()) {
            error_response["message"] = "System requires FRAM credential programming";
        }
        
        String response_str;
        serializeJson(error_response, response_str);
        request->send(401, "application/json", response_str);
    }
}

void handleLogout(AsyncWebServerRequest* request) {
    if (request->hasHeader("Cookie")) {
        String cookie = request->getHeader("Cookie")->value();
        int tokenStart = cookie.indexOf("session_token=");
        if (tokenStart != -1) {
            tokenStart += 14;
            int tokenEnd = cookie.indexOf(";", tokenStart);
            if (tokenEnd == -1) tokenEnd = cookie.length();
            
            String token = cookie.substring(tokenStart, tokenEnd);
            destroySession(token);
        }
    }
    
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"success\":true}");
    response->addHeader("Set-Cookie", "session_token=; Path=/; HttpOnly; Max-Age=0");
    request->send(response);
}

void handleStatus(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    JsonDocument json;
    
    // ============================================
    // HARDWARE STATUS (for badges)
    // ============================================
    json["sensor_active"] = readWaterSensor();
    json["pump_active"] = isPumpActive();
    json["system_error"] = (waterAlgorithm.getState() == STATE_ERROR);
    
    // ============================================
    // SYSTEM DISABLE STATUS (NEW)
    // ============================================
    json["system_disabled"] = isSystemDisabled();
    
    
    // ============================================
    // PROCESS STATUS (for description + remaining time)
    // ============================================
    json["state_description"] = waterAlgorithm.getStateDescription();
    json["remaining_seconds"] = waterAlgorithm.getRemainingSeconds();
    
    // ============================================
    // EXISTING STATUS FIELDS
    // ============================================
    json["water_status"] = getSensorPhaseString();
    json["pump_running"] = isPumpActive();  // kept for backwards compatibility
    json["pump_remaining"] = getPumpRemainingTime();  // kept for backwards compatibility
    json["wifi_status"] = getWiFiStatus();
    json["wifi_connected"] = isWiFiConnected();
    json["rtc_time"] = getCurrentTimestamp();
    json["rtc_working"] = isRTCWorking();
    json["rtc_info"] = getTimeSourceInfo();
    json["rtc_hardware"] = isRTCHardware(); 
    json["rtc_needs_sync"] = rtcNeedsSynchronization();
    json["rtc_battery_issue"] = isBatteryIssueDetected();
    json["free_heap"] = ESP.getFreeHeap();
    json["uptime"] = millis();
    
    // ============================================
    // DEVICE INFO
    // ============================================
    json["device_id"] = getDeviceID();
    json["credentials_source"] = areCredentialsLoaded() ? "FRAM" : "FALLBACK";
    json["vps_url"] = getVPSURL();
    json["authentication_enabled"] = areCredentialsLoaded();
    
    if (!areCredentialsLoaded()) {
        json["setup_required"] = true;
        json["setup_message"] = "Use Captive Portal to configure FRAM credentials";
    }
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleDirectPumpOn(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    uint16_t duration = currentPumpSettings.manualCycleSeconds;
    String mode = "bistable";
    if (request->hasParam("mode", true) && request->getParam("mode", true)->value() == "monostable") {
        duration = DIRECT_PUMP_SAFETY_TIMEOUT_S;
        mode = "monostable";
    }

    bool success = directPumpOn(duration);

    JsonDocument json;
    json["success"] = success;
    json["duration"] = duration;
    json["mode"] = mode;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleDirectPumpOff(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    directPumpOff();

    JsonDocument json;
    json["success"] = true;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handlePumpStop(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    stopPump();
    
    JsonDocument json;
    json["success"] = true;
    json["message"] = "Pump stopped";
    
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
    
    LOG_INFO("Pump manually stopped via web");
}

void handlePumpSettings(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        // Return current settings
        JsonDocument json;
        json["success"] = true;
        json["volume_per_second"] = currentPumpSettings.volumePerSecond;
        json["normal_cycle"] = currentPumpSettings.manualCycleSeconds;
        json["extended_cycle"] = currentPumpSettings.calibrationCycleSeconds;
        json["auto_mode"] = currentPumpSettings.autoModeEnabled;
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
        
    } else if (request->method() == HTTP_POST) {
        if (!request->hasParam("volume_per_second", true)) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"No volume_per_second parameter\"}");
            return;
        }
        
        String volumeStr = request->getParam("volume_per_second", true)->value();
        float newVolume = volumeStr.toFloat();
        
        if (newVolume < 0.1 || newVolume > 20) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be between 0.1-20\"}");
            return;
        }
        
        currentPumpSettings.volumePerSecond = newVolume;

        // Save to NVS (non-volatile storage)
        saveVolumeToNVS();
        
        LOG_INFO("Volume per second updated to %.1f ml/s", newVolume);
        
        JsonDocument response;
        response["success"] = true;
        response["volume_per_second"] = newVolume;
        response["message"] = "Volume per second updated successfully";
        
        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
    }
}

// ============================================
// SYSTEM TOGGLE HANDLER (NEW)
// Replaces old pump toggle with full system control
// ============================================
void handleSystemToggle(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        JsonDocument json;
        json["success"] = true;
        json["enabled"] = !isSystemDisabled();

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

    } else if (request->method() == HTTP_POST) {
        bool currentlyDisabled = isSystemDisabled();
        setSystemState(currentlyDisabled);  // toggle

        JsonDocument json;
        json["success"] = true;
        json["enabled"] = currentlyDisabled;  // was disabled → now enabled, and vice versa

        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);

        LOG_INFO("System %s via web interface", currentlyDisabled ? "ENABLED" : "DISABLED");
    }
}

// ============================================
// LEGACY PUMP TOGGLE (kept for compatibility)
// Will be removed after frontend migration
// ============================================
void handlePumpToggle(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }
    
    if (request->method() == HTTP_GET) {
        // Return current state
        JsonDocument json;
        json["success"] = true;
        json["enabled"] = pumpGlobalEnabled;
        
        if (!pumpGlobalEnabled && pumpDisabledTime > 0) {
            unsigned long remaining = PUMP_AUTO_ENABLE_MS - (millis() - pumpDisabledTime);
            json["remaining_seconds"] = remaining / 1000;
        } else {
            json["remaining_seconds"] = 0;
        }
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
        
    } else if (request->method() == HTTP_POST) {
        // Toggle pump state
        setPumpGlobalState(!pumpGlobalEnabled);
        
        JsonDocument json;
        json["success"] = true;
        json["enabled"] = pumpGlobalEnabled;
        json["message"] = pumpGlobalEnabled ? "Pump enabled" : "Pump disabled for 30 minutes";
        
        if (!pumpGlobalEnabled) {
            json["remaining_seconds"] = PUMP_AUTO_ENABLE_MS / 1000;
        } else {
            json["remaining_seconds"] = 0;
        }
        
        String response;
        serializeJson(json, response);
        request->send(200, "application/json", response);
    }
}

void handleResetStatistics(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    bool success = resetErrorStatsInFRAM();

    JsonDocument json;
    json["success"] = success;
    json["message"] = success ? "Statistics reset successfully" : "Failed to reset statistics";

    if (success) {
        ErrorStats stats;
        if (loadErrorStatsFromFRAM(stats)) {
            json["reset_timestamp"] = stats.last_reset_timestamp;
        }
    }

    String response;
    serializeJson(json, response);
    request->send(success ? 200 : 500, "application/json", response);

    LOG_INFO("Statistics reset requested via web interface - success: %s", success ? "true" : "false");
}

void handleGetStatistics(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    ErrorStats stats;
    bool success = loadErrorStatsFromFRAM(stats);

    JsonDocument json;
    json["success"] = success;

    if (success) {
        json["gap1_fail_sum"] = stats.gap1_fail_sum;
        json["gap2_fail_sum"] = stats.gap2_fail_sum;
        json["water_fail_sum"] = stats.water_fail_sum;
        json["last_reset_timestamp"] = stats.last_reset_timestamp;

        time_t resetTime = (time_t)stats.last_reset_timestamp;
        struct tm* timeinfo = localtime(&resetTime);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M", timeinfo);
        json["last_reset_formatted"] = String(timeStr);
    } else {
        json["error"] = "Failed to load statistics";
    }

    String response;
    serializeJson(json, response);
    request->send(success ? 200 : 500, "application/json", response);
}

// ========================================
// ROLLING 24H VOLUME HANDLERS
// ========================================

void handleGetDailyVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    JsonDocument json;
    json["success"] = true;
    json["rolling_24h_ml"] = waterAlgorithm.getRolling24hVolume();
    json["history_window_s"] = waterAlgorithm.getConfig().history_window_s;
    json["dose_ml"]       = waterAlgorithm.getConfig().dose_ml;
    json["daily_limit_ml"] = waterAlgorithm.getConfig().daily_limit_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleResetDailyVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    // Rolling 24h window resets automatically — no manual reset supported
    request->send(501, "application/json",
        "{\"success\":false,\"error\":\"Not supported — rolling 24h window resets automatically\"}");
}

// ===============================
// AVAILABLE VOLUME — physical water remaining in reservoir
// ===============================

void handleGetAvailableVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    JsonDocument json;
    json["success"]           = true;
    json["available_ml"]      = waterAlgorithm.getConfig().available_ml;
    json["available_max_ml"]  = waterAlgorithm.getConfig().available_max_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleSetAvailableVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing value parameter\"}");
        return;
    }
    int value = request->getParam("value", true)->value().toInt();
    if (value < 100 || value > 20000) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be 100-20000 ml\"}");
        return;
    }
    TopOffConfig cfg = waterAlgorithm.getConfig();
    cfg.available_ml     = (uint16_t)value;
    cfg.available_max_ml = (uint16_t)value;
    waterAlgorithm.setConfig(cfg);

    JsonDocument json;
    json["success"]          = true;
    json["available_ml"]     = cfg.available_ml;
    json["available_max_ml"] = cfg.available_max_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleRefillAvailableVolume(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }
    // Refill: reset available_ml back to available_max_ml
    TopOffConfig cfg = waterAlgorithm.getConfig();
    if (cfg.available_max_ml == 0) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Available volume not configured\"}");
        return;
    }
    cfg.available_ml = cfg.available_max_ml;
    waterAlgorithm.setConfig(cfg);

    JsonDocument json;
    json["success"]          = true;
    json["available_ml"]     = cfg.available_ml;
    json["available_max_ml"] = cfg.available_max_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

// ===============================
// MAX DOSE (per-cycle safety limit) HANDLERS
// ===============================

void handleGetFillWaterMax(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    JsonDocument json;
    json["success"]       = true;
    json["dose_ml"]       = waterAlgorithm.getConfig().dose_ml;
    json["daily_limit_ml"] = waterAlgorithm.getConfig().daily_limit_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleSetFillWaterMax(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing value parameter\"}");
        return;
    }

    int value = request->getParam("value", true)->value().toInt();

    if (value < 100 || value > 10000) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be 100-10000 ml\"}");
        return;
    }

    TopOffConfig cfg = waterAlgorithm.getConfig();
    cfg.daily_limit_ml = (uint16_t)value;
    waterAlgorithm.setConfig(cfg);

    JsonDocument json;
    json["success"]        = true;
    json["daily_limit_ml"] = waterAlgorithm.getConfig().daily_limit_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

void handleSetDose(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "application/json", "{\"success\":false,\"error\":\"Unauthorized\"}");
        return;
    }

    if (!request->hasParam("value", true)) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing value parameter\"}");
        return;
    }

    int value = request->getParam("value", true)->value().toInt();

    if (value < 50 || value > 2000) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Value must be 50-2000 ml\"}");
        return;
    }

    TopOffConfig cfg = waterAlgorithm.getConfig();
    cfg.dose_ml = (uint16_t)value;
    waterAlgorithm.setConfig(cfg);

    JsonDocument json;
    json["success"] = true;
    json["dose_ml"] = waterAlgorithm.getConfig().dose_ml;

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

// ===============================
// TOP-OFF HISTORY HANDLER
// ===============================

extern volatile bool framBusy;

void handleGetCycleHistory(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    if (framBusy) {
        request->send(503, "application/json", "{\"success\":false,\"error\":\"System busy\"}");
        return;
    }

    static TopOffRecord buf[TOPOFF_HISTORY_SIZE];
    uint16_t count = loadTopOffHistory(buf, TOPOFF_HISTORY_SIZE);

    JsonDocument doc;
    doc["success"] = true;
    doc["total"]   = count;

    // EMA context — needed by frontend charts
    const EmaBlock&    ema = waterAlgorithm.getEma();
    const TopOffConfig& cfg = waterAlgorithm.getConfig();
    doc["ema_rate"]     = ema.ema_rate_ml_h;
    doc["ema_dev"]      = ema.ema_dev_ml_h;
    doc["bootstrap"]    = ema.bootstrap_count;
    doc["yellow_sigma"] = cfg.rate_yellow_sigma;
    doc["red_sigma"]    = cfg.rate_red_sigma;

    JsonArray arr = doc["cycles"].to<JsonArray>();

    // Iterate newest-first (ring buffer returns oldest-first)
    for (int i = (int)count - 1; i >= 0; i--) {
        const TopOffRecord& r = buf[i];
        JsonObject obj = arr.add<JsonObject>();

        obj["ts"]           = r.timestamp;
        obj["interval_s"]   = r.interval_s;
        obj["rate_ml_h"]    = r.rate_ml_h;
        obj["dev_sigma"]    = r.dev_rate_sigma;
        obj["alert"]        = r.alert_level;
        obj["hour"]         = r.hour_of_day;
        obj["cycle"]        = r.cycle_num;
        obj["manual"]       = (bool)(r.flags & TopOffRecord::FLAG_MANUAL);
        obj["bootstrap"]    = (bool)(r.flags & TopOffRecord::FLAG_BOOTSTRAP);
        obj["daily_lim"]    = (bool)(r.flags & TopOffRecord::FLAG_DAILY_LIM);
        obj["red_alert"]    = (bool)(r.flags & TopOffRecord::FLAG_RED_ALERT);
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

// ===============================
// SYSTEM RESET HANDLER
// ===============================

void handleSystemReset(AsyncWebServerRequest* request) {
    if (!checkAuthentication(request)) {
        request->send(401, "text/plain", "Unauthorized");
        return;
    }

    bool success = waterAlgorithm.resetSystem();

    JsonDocument json;
    json["success"] = success;
    json["state"] = waterAlgorithm.getStateString();
    json["message"] = success ? "System reset to IDLE" : "Reset blocked - logging in progress";

    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response);
}

// ===============================
// HEALTH CHECK HANDLER
// ===============================

void handleHealth(AsyncWebServerRequest *request) {
    IPAddress sourceIP = request->client()->remoteIP();
    if (!isIPAllowed(sourceIP) && !isTrustedProxy(sourceIP)) {
        request->send(403, "application/json", "{\"error\":\"Forbidden\"}");
        return;
    }

    String json = "{";
    json += "\"status\":\"ok\",";
    json += "\"device_name\":\"" + String(getDeviceID()) + "\",";
    json += "\"uptime\":" + String(millis());
    json += "}";

    request->send(200, "application/json", json);
}
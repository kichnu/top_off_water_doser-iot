#include "prov_detector.h"
#include "prov_config.h"
#include "../core/logging.h"
#include "../hardware/hardware_pins.h"

// bool checkProvisioningButton() {
//     // Configure button pin as input with pull-up
//     pinMode(PROV_BUTTON_PIN, INPUT_PULLUP);
    
//     LOG_INFO("Checking provisioning button on GPIO%d...", PROV_BUTTON_PIN);
    
//     // Initial debounce delay
//     delay(PROV_BUTTON_DEBOUNCE_MS);
    
//     // Check if button is pressed (LOW = pressed with pull-up)
//     if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
//         LOG_INFO("Provisioning button not pressed - normal boot");
//         return false;
//     }
    
//     LOG_INFO("Provisioning button pressed - checking hold time...");
    
//     // Button is pressed - check if held for required time
//     unsigned long pressStart = millis();
    
//     while (millis() - pressStart < PROV_BUTTON_HOLD_MS) {
//         // Check every 50ms if button is still pressed
//         if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
//             // Button released too early
//             unsigned long heldTime = millis() - pressStart;
//             LOG_INFO("Button released after %lums (required: %dms)", 
//                      heldTime, PROV_BUTTON_HOLD_MS);
//             return false;
//         }
//         delay(50);
//     }
    
//     // Button held for full duration
//     LOG_INFO("✓ Provisioning button held for %dms - entering provisioning mode", 
//              PROV_BUTTON_HOLD_MS);
    
//     return true;
// }


bool checkProvisioningButton() {
    // Configure button pin as input with pull-up
    pinMode(PROV_BUTTON_PIN, INPUT_PULLUP);
    
    // Configure feedback pin as output
    pinMode(PROV_FEEDBACK_PIN, OUTPUT);
    digitalWrite(PROV_FEEDBACK_PIN, LOW);
    
    LOG_INFO("Checking provisioning button on GPIO%d...", PROV_BUTTON_PIN);
    
    // Initial debounce delay
    delay(PROV_BUTTON_DEBOUNCE_MS);
    
    // Check if button is pressed (LOW = pressed with pull-up)
    if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
        LOG_INFO("Provisioning button not pressed - normal boot");
        digitalWrite(PROV_FEEDBACK_PIN, LOW);  // Ensure feedback is off
        return false;
    }
    
    LOG_INFO("Provisioning button pressed - checking hold time...");
    LOG_INFO("Audio feedback active - hold for %dms (+%dms margin)", 
             PROV_BUTTON_HOLD_MS, PROV_HOLD_MARGIN_MS);
    
    // Button is pressed - check if held for required time
    // Total time = PROV_BUTTON_HOLD_MS + PROV_HOLD_MARGIN_MS
    unsigned long totalHoldTime = PROV_BUTTON_HOLD_MS + PROV_HOLD_MARGIN_MS;
    unsigned long pressStart = millis();
    unsigned long lastBeepToggle = millis();
    bool beepState = false;
    
    while (millis() - pressStart < totalHoldTime) {
        // Check if button is still pressed
        if (digitalRead(PROV_BUTTON_PIN) == HIGH) {
            // Button released too early
            unsigned long heldTime = millis() - pressStart;
            digitalWrite(PROV_FEEDBACK_PIN, LOW);  // Turn off feedback
            LOG_INFO("Button released after %lums (required: %lums)", 
                     heldTime, totalHoldTime);
            return false;
        }
        
        // Handle beep pattern
        unsigned long now = millis();
        unsigned long beepInterval = beepState ? PROV_BEEP_HIGH_MS : PROV_BEEP_LOW_MS;
        
        if (now - lastBeepToggle >= beepInterval) {
            beepState = !beepState;
            digitalWrite(PROV_FEEDBACK_PIN, beepState ? HIGH : LOW);
            lastBeepToggle = now;
        }
        
        delay(5);  // Small delay to prevent busy loop
    }
    
    // Button held for full duration
    digitalWrite(PROV_FEEDBACK_PIN, LOW);  // Turn off feedback
    
    LOG_INFO("✓ Provisioning button held for %lums - entering provisioning mode", 
             totalHoldTime);
    
    return true;
}
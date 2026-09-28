#include "wifi_manager.h"
#include "config/config.h"
#include <esp_wifi.h>

WiFiManager::WiFiManager() {
    // Constructor
}

bool WiFiManager::setupAP() {
    Serial.println("=== Starting WiFi AP Setup ===");

    // WiFi was pre-initialized in main.cpp BEFORE timing task starts.
    // This prevents the high-priority timing task from starving WiFi init.
    // Now we reconfigure it with proper SSID, IP, and settings.

    // CRITICAL: On ESP32-S3, if AP is already running, we must stop it first
    // before restarting with new configuration, otherwise it crashes in ieee80211_hostap_attach
    if (WiFi.getMode() & WIFI_AP) {
        Serial.println("Stopping existing AP before reconfiguration...");
        WiFi.softAPdisconnect(true);  // true = delete the AP interface
        delay(100);  // Give WiFi stack time to clean up
    }

    // Create unique SSID with MAC address
    // Use softAPmacAddress() for AP mode, not macAddress() (which is for STA mode)
    // Note: We need WiFi mode set to AP before we can get MAC address
    WiFi.mode(WIFI_AP);
    delay(50);  // Small delay to ensure mode change is processed
    
    String macAddr = WiFi.softAPmacAddress();

    Serial.printf("AP MAC Address: %s\n", macAddr.c_str());

    if (macAddr.length() == 0 || macAddr == "00:00:00:00:00:00") {
        _apSSID = String(WIFI_AP_SSID_PREFIX) + "-ESP32";
    } else {
        macAddr.replace(":", "");
        _apSSID = String(WIFI_AP_SSID_PREFIX) + "-" + macAddr.substring(8);
    }

    // For AP mode we want maximum stability, not power saving
    WiFi.setSleep(false);

    // Configure AP IP settings
    // 192.168.8.x (NOT 192.168.4.x) — 192.168.4.x collides with the home
    // network (DeerFiber 192.168.4.0/22) and breaks the Mac's routing to it.
    WiFi.softAPConfig(
        IPAddress(192, 168, 8, 1),
        IPAddress(192, 168, 8, 1),
        IPAddress(255, 255, 255, 0)
    );

    delay(200); // Delay 200ms to ensure the AP is ready

    // DIAGNOSTIC: force MAX TX power (20 dBm). The S3's NVS is blank, so its
    // default TX power may be low -> beacon too weak to reach the Mac. Set
    // after WiFi is initialized (mode set) but before softAP() so it applies.
    {
      int8_t txp = -127;
      esp_wifi_get_max_tx_power(&txp);
      Serial.printf("[NCD] AP TX power before=%d dBm\n", txp);
      esp_err_t e = esp_wifi_set_max_tx_power(20);
      esp_wifi_get_max_tx_power(&txp);
      Serial.printf("[NCD] AP TX power set->20 (err=%d) now=%d dBm\n", e, txp);
    }

    Serial.printf("Starting AP with SSID: %s\n", _apSSID.c_str());

    // Start AP with configured settings
    bool ap_started = WiFi.softAP(_apSSID.c_str(), WIFI_AP_PASSWORD, 1, 0, 4);

    if (ap_started) {
        Serial.println("=== WiFi AP Started ===");
        Serial.printf("SSID: %s\n", _apSSID.c_str());
        Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());

        // Set WiFi protocol AFTER AP is started
        esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11N);

        return true;
    } else {
        Serial.println("ERROR: WiFi AP failed to start");
        return false;
    }
}

// wifi_manager.cpp — WiFi 连接管理实现

#include "wifi_manager.h"

WiFiManager::WiFiManager()
    : _connectStartTime(0)
    , _connected(false)
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
}

bool WiFiManager::connect() {
    return connect(WIFI_SSID, WIFI_PASSWORD);
}

bool WiFiManager::connect(const char* ssid, const char* password) {
    if (_connected && WiFi.isConnected()) {
        return true;
    }

    Serial.printf("[WiFi] 正在连接 %s ...\n", ssid);
    WiFi.begin(ssid, password);

    _connectStartTime = millis();
    _connected = false;

    while (!WiFi.isConnected()) {
        if (millis() - _connectStartTime > WIFI_TIMEOUT_MS) {
            Serial.printf("[WiFi] 连接超时 (%d ms)\n", WIFI_TIMEOUT_MS);
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    _connected = true;
    Serial.printf("[WiFi] 已连接，IP: %s, RSSI: %d dBm\n",
                  getLocalIP().c_str(), getRSSI());
    return true;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    _connected = false;
    Serial.println("[WiFi] 已断开");
}

bool WiFiManager::isConnected() const {
    return _connected && WiFi.isConnected();
}

String WiFiManager::getLocalIP() const {
    return WiFi.localIP().toString();
}

int WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

String WiFiManager::getStatusString() const {
    if (!isConnected()) {
        return "WiFi: DISCONNECTED";
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "WiFi: %s (%d dBm)",
             getLocalIP().c_str(), getRSSI());
    return String(buf);
}

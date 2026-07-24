// mqtt_manager.cpp — MQTT 客户端管理实现

#include "mqtt_manager.h"

MqttManager* MqttManager::_instance = nullptr;

MqttManager::MqttManager(WiFiClient& wifiClient)
    : _client(wifiClient)
    , _userCallback(nullptr)
    , _port(MQTT_PORT)
    , _connected(false)
{
    _instance = this;
    _client.setCallback(_onMessage);
    _client.setBufferSize(512);  // 足够容纳 JSON 消息
}

bool MqttManager::connect() {
    return connect(MQTT_BROKER, MQTT_PORT);
}

bool MqttManager::connect(const char* broker, uint16_t port) {
    if (_connected && _client.connected()) {
        return true;
    }

    _broker = broker;
    _port = port;

    Serial.printf("[MQTT] 正在连接 %s:%d ...\n", broker, port);
    _client.setServer(broker, port);

    // 尝试连接，使用客户端 ID
    if (_client.connect(MQTT_CLIENT_ID)) {
        _connected = true;
        Serial.printf("[MQTT] 已连接，客户端 ID: %s\n", MQTT_CLIENT_ID);

        // 订阅控制命令主题（预留）
        _client.subscribe(MQTT_TOPIC_COMMAND);

        // 发布上线消息
        String statusMsg = "{\"status\":\"online\",\"client\":\"" +
                           String(MQTT_CLIENT_ID) + "\"}";
        publish(MQTT_TOPIC_STATUS, statusMsg, true);

        return true;
    }

    Serial.printf("[MQTT] 连接失败，状态: %d\n", _client.state());
    _connected = false;
    return false;
}

void MqttManager::disconnect() {
    if (_client.connected()) {
        // 发布下线消息
        String statusMsg = "{\"status\":\"offline\"}";
        publish(MQTT_TOPIC_STATUS, statusMsg, true);
        _client.disconnect();
    }
    _connected = false;
    Serial.println("[MQTT] 已断开");
}

bool MqttManager::isConnected() const {
    return _connected && _client.connected();
}

void MqttManager::loop() {
    if (!_client.connected()) {
        _connected = false;
        // 自动重连（间隔 5 秒）
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            Serial.println("[MQTT] 连接断开，尝试重连...");
            connect();
        }
        return;
    }
    _client.loop();
}

bool MqttManager::publish(const char* topic, const String& payload, bool retained) {
    if (!isConnected()) {
        return false;
    }
    bool ok = _client.publish(topic, payload.c_str(), retained);
    if (!ok) {
        Serial.printf("[MQTT] 发布失败: %s -> %s\n", topic, payload.c_str());
    }
    return ok;
}

bool MqttManager::subscribe(const char* topic) {
    if (!isConnected()) {
        return false;
    }
    return _client.subscribe(topic);
}

bool MqttManager::unsubscribe(const char* topic) {
    if (!isConnected()) {
        return false;
    }
    return _client.unsubscribe(topic);
}

void MqttManager::setCallback(MqttCallback cb) {
    _userCallback = cb;
}

String MqttManager::getLastError() const {
    switch (_client.state()) {
        case MQTT_CONNECTED:         return "Connected";
        case MQTT_CONNECT_BAD_PROTOCOL: return "Bad protocol";
        case MQTT_CONNECT_BAD_CLIENT_ID: return "Bad client ID";
        case MQTT_CONNECT_UNAVAILABLE:   return "Server unavailable";
        case MQTT_CONNECT_BAD_CREDENTIALS: return "Bad credentials";
        case MQTT_CONNECT_UNAUTHORIZED:   return "Unauthorized";
        default: return "Unknown";
    }
}

void MqttManager::_onMessage(char* topic, byte* payload, unsigned int length) {
    if (!_instance) return;

    String topicStr(topic);
    String payloadStr(reinterpret_cast<char*>(payload), length);

    Serial.printf("[MQTT] 收到命令: %s -> %s\n", topicStr.c_str(), payloadStr.c_str());

    if (_instance->_userCallback) {
        _instance->_userCallback(topicStr, payloadStr);
    }
}

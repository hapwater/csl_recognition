// mqtt_manager.h — MQTT 客户端管理

#ifndef MQTT_MANAGER_H_
#define MQTT_MANAGER_H_

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <functional>
#include "config.h"

// 消息回调类型：收到订阅主题的消息时触发
using MqttCallback = std::function<void(const String& topic, const String& payload)>;

class MqttManager {
public:
    MqttManager(WiFiClient& wifiClient);

    // 连接到配置文件中定义的 MQTT Broker
    bool connect();

    // 连接指定 Broker
    bool connect(const char* broker, uint16_t port);

    // 断开连接
    void disconnect();

    // 是否已连接
    bool isConnected() const;

    // 循环处理 MQTT 消息（应在主 loop 中定期调用）
    void loop();

    // 发布消息到指定主题
    bool publish(const char* topic, const String& payload, bool retained = false);

    // 订阅主题
    bool subscribe(const char* topic);

    // 取消订阅
    bool unsubscribe(const char* topic);

    // 设置消息回调（收到订阅消息时触发）
    void setCallback(MqttCallback cb);

    // 获取最后错误信息
    String getLastError() const;

private:
    PubSubClient _client;
    MqttCallback _userCallback;
    String _broker;
    uint16_t _port;
    bool _connected;

    // 内部回调转发
    static void _onMessage(char* topic, byte* payload, unsigned int length);
    static MqttManager* _instance;
};

#endif  // MQTT_MANAGER_H_

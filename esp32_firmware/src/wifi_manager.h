// wifi_manager.h — WiFi 连接管理

#ifndef WIFI_MANAGER_H_
#define WIFI_MANAGER_H_

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiManager {
public:
    WiFiManager();

    // 连接到配置文件中定义的 WiFi
    // Returns: true 表示连接成功
    bool connect();

    // 连接到指定的 SSID
    bool connect(const char* ssid, const char* password);

    // 断开连接
    void disconnect();

    // 检查是否已连接
    bool isConnected() const;

    // 获取本机 IP 地址
    String getLocalIP() const;

    // 获取信号强度 (RSSI, dBm)
    int getRSSI() const;

    // 获取连接状态描述
    String getStatusString() const;

private:
    unsigned long _connectStartTime;
    bool _connected;
};

#endif  // WIFI_MANAGER_H_

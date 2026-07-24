// main.cpp — CSL Recognition ESP32-S3 固件入口
//
// 数据流向:
//   ESP32-CAM ──WiFi (MJPEG)──→ ESP32-S3 ──MQTT──→ PC/手机
//                                  ↓
//                            JPEG解码 → 手部关键点提取
//                            → 序列缓冲(30帧) → TFLite 推理
//
// 状态机:
//   INIT → WIFI_CONN → MQTT_CONN → CAM_CONN → RUNNING
//                                               ↑   |
//                                               └───┘ (断线重连)

#include <Arduino.h>

#include "config.h"
#include "inference.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "cam_stream.h"
#include "frame_processor.h"

// ─── 全局实例 ───────────────────────────────────────────────
WiFiClient wifiClient;
InferenceEngine inferenceEngine;
WiFiManager wifiManager;
MqttManager mqttManager(wifiClient);
CamStream camStream;
FrameProcessor frameProcessor;

// ─── 状态枚举 ───────────────────────────────────────────────
enum class SystemState {
    INIT,
    WIFI_CONN,
    MQTT_CONN,
    CAM_CONN,
    RUNNING,
    ERROR
};

SystemState state = SystemState::INIT;
unsigned long stateEnterTime = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastInferencePublish = 0;

// ─── 函数前置声明 ───────────────────────────────────────────
void enterState(SystemState newState);
void publishStatus();
void onMqttCommand(const String& topic, const String& payload);
void onInferenceResult(const InferenceResult& result);
void onCameraFrame(const uint8_t* jpeg, size_t len);

// ═════════════════════════════════════════════════════════════
//  初始化
// ═════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n╔══════════════════════════════════╗");
    Serial.println("║  CSL Recognition — ESP32-S3      ║");
    Serial.println("║  摄像头流 → TFLite 推理 → MQTT    ║");
    Serial.println("╚══════════════════════════════════╝");
    Serial.printf("固件: %s %s\n", __DATE__, __TIME__);
    Serial.printf("CAM: %s\n", CAM_STREAM_URL);
    Serial.printf("MQTT: %s:%d / %s\n", MQTT_BROKER, MQTT_PORT, MQTT_TOPIC_RESULT);

    // ── 初始化帧处理器（含 JPEG 解码 + 推理引擎） ──
    Serial.println("[系统] 初始化帧处理器...");
    if (!frameProcessor.begin()) {
        Serial.println("[系统] ⚠ 帧处理器初始化失败（部分功能不可用）");
    }

    // 注册推理完成回调 → MQTT 发布
    frameProcessor.setInferenceCallback(onInferenceResult);

    // ── 注册 CAM 帧回调 → 帧处理器 ──
    camStream.setFrameCallback(onCameraFrame);

    enterState(SystemState::WIFI_CONN);
}

// ═════════════════════════════════════════════════════════════
//  主循环
// ═════════════════════════════════════════════════════════════
void loop() {
    switch (state) {
        case SystemState::INIT:
            break;

        case SystemState::WIFI_CONN:
            if (wifiManager.connect()) {
                enterState(SystemState::MQTT_CONN);
            } else {
                static unsigned long lastWifiRetry = 0;
                if (millis() - lastWifiRetry > 10000) {
                    lastWifiRetry = millis();
                    enterState(SystemState::WIFI_CONN);
                }
            }
            break;

        case SystemState::MQTT_CONN:
            if (mqttManager.connect()) {
                mqttManager.setCallback(onMqttCommand);
                enterState(SystemState::CAM_CONN);
            } else {
                delay(5000);
                enterState(SystemState::MQTT_CONN);
            }
            break;

        case SystemState::CAM_CONN:
            Serial.printf("[CAM] 正在连接 %s ...\n", CAM_STREAM_URL);
            if (camStream.begin(CAM_STREAM_URL)) {
                enterState(SystemState::RUNNING);
            } else {
                delay(3000);
                enterState(SystemState::CAM_CONN);
            }
            break;

        case SystemState::RUNNING: {
            // ── 连接健康检查 ──
            if (!wifiManager.isConnected()) {
                Serial.println("[系统] WiFi 断开");
                enterState(SystemState::WIFI_CONN);
                return;
            }

            // ── 驱动各模块 ──
            mqttManager.loop();
            camStream.loop();         // 接收 CAM 的 MJPEG 帧 → 回调 onCameraFrame
            frameProcessor.loop();    // 维护帧处理状态 + 定期日志

            // ── 定期发布设备状态 ──
            if (millis() - lastStatusPublish > 30000) {
                publishStatus();
                lastStatusPublish = millis();
            }

            // ── 打印连接状态（调试用，每 10 秒） ──
            static unsigned long lastDebugLog = 0;
            if (millis() - lastDebugLog > 10000) {
                lastDebugLog = millis();
                Serial.printf("[状态] %s | %s | %.1f fps\n",
                              wifiManager.getStatusString().c_str(),
                              camStream.getStatus().c_str(),
                              frameProcessor.getFps());
            }

#ifdef USE_DISPLAY
            updateDisplay();
#endif
            break;
        }

        case SystemState::ERROR:
            Serial.println("[系统] 错误，10 秒后重启...");
            delay(10000);
            ESP.restart();
            break;
    }
}

// ═════════════════════════════════════════════════════════════
//  帧回调 — 每收到一帧 JPEG 数据时触发
// ═════════════════════════════════════════════════════════════
void onCameraFrame(const uint8_t* jpeg, size_t len) {
    frameProcessor.processFrame(jpeg, len);
}

// ═════════════════════════════════════════════════════════════
//  推理完成回调 — 发布到 MQTT
// ═════════════════════════════════════════════════════════════
void onInferenceResult(const InferenceResult& result) {
    if (!mqttManager.isConnected()) return;

    // 避免同一结果重复发布
    unsigned long now = millis();
    if (now - lastInferencePublish < 500 && !result.valid) return;
    lastInferencePublish = now;

    String json = "{";
    json += "\"word\":\"" + String(result.valid ? result.className : "?") + "\",";
    json += "\"confidence\":" + String(result.valid ? result.confidence : 0.0, 4) + ",";
    json += "\"time_ms\":" + String(millis()) + ",";
    json += "\"fps\":" + String(frameProcessor.getFps(), 1);
    json += "}";

    if (mqttManager.publish(MQTT_TOPIC_RESULT, json)) {
        Serial.printf("[MQTT] 已发布: %s\n", json.c_str());
    }
}

// ═════════════════════════════════════════════════════════════
//  状态切换
// ═════════════════════════════════════════════════════════════
void enterState(SystemState newState) {
    state = newState;
    stateEnterTime = millis();

    switch (newState) {
        case SystemState::INIT:     Serial.println("[状态] INIT"); break;
        case SystemState::WIFI_CONN: Serial.println("[状态] WIFI_CONN"); break;
        case SystemState::MQTT_CONN: Serial.println("[状态] MQTT_CONN"); break;
        case SystemState::CAM_CONN:  Serial.println("[状态] CAM_CONN"); break;
        case SystemState::RUNNING:
            Serial.println("[状态] RUNNING — 接收 CAM 视频流中...");
            break;
        case SystemState::ERROR:    Serial.println("[状态] ERROR"); break;
    }
}

// ═════════════════════════════════════════════════════════════
//  MQTT 命令回调
// ═════════════════════════════════════════════════════════════
void onMqttCommand(const String& topic, const String& payload) {
    Serial.printf("[CMD] %s: %s\n", topic.c_str(), payload.c_str());

    // 支持简单命令：
    // {"cmd":"reset"} — 重启设备
    // {"cmd":"reconnect"} — 重连 CAM
    if (payload.indexOf("reset") >= 0) {
        Serial.println("[CMD] 收到重启命令");
        ESP.restart();
    }
    if (payload.indexOf("reconnect") >= 0) {
        Serial.println("[CMD] 重连 CAM...");
        camStream.stop();
        camStream.begin(CAM_STREAM_URL);
    }
}

// ═════════════════════════════════════════════════════════════
//  发布状态
// ═════════════════════════════════════════════════════════════
void publishStatus() {
    if (!mqttManager.isConnected()) return;

    String json = "{";
    json += "\"device\":\"esp32s3\",";
    json += "\"ip\":\"" + wifiManager.getLocalIP() + "\",";
    json += "\"rssi\":" + String(wifiManager.getRSSI()) + ",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"cam\":" + String(camStream.isConnected() ? "\"connected\"" : "\"disconnected\"") + ",";
    json += "\"fps\":" + String(frameProcessor.getFps(), 1) + ",";
    json += "\"buffer\":" + String(frameProcessor.getBufferProgress() * 100, 0);
    json += "}";

    mqttManager.publish(MQTT_TOPIC_STATUS, json);
}

// ═════════════════════════════════════════════════════════════
//  显示屏（可选）
// ═════════════════════════════════════════════════════════════
#ifdef USE_DISPLAY
#include <Wire.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

void setupDisplay() {
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[Display] OLED 初始化失败");
        return;
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("CSL Ready");
    display.display();
}

void updateDisplay() {
    InferenceResult r = frameProcessor.getLastResult();
    display.clearDisplay();
    display.setCursor(0, 0);
    if (r.valid) {
        display.setTextSize(2);
        display.println(r.className);
        display.setTextSize(1);
        display.printf("%.0f%%", r.confidence * 100);
    } else {
        display.println("Waiting...");
    }
    display.display();
}
#endif  // USE_DISPLAY

// main.cpp — ESP32-CAM WiFi 摄像头固件
//
// 功能:
//   1. 初始化 OV2640 摄像头
//   2. 对原始帧做预处理（镜像、对比度、亮度等）
//   3. JPEG 编码后通过 HTTP MJPEG 流发送
//
// 数据流向:
//   ESP32-CAM ──WiFi (MJPEG)──→ ESP32-S3 (解码+推理+MQTT)
//
// 测试: 浏览器打开 http://cam-ip/ 可看到实时画面

#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include "config.h"

// ─── 全局变量 ───────────────────────────────────────────────
AsyncWebServer server(HTTP_PORT);
SemaphoreHandle_t cameraMutex = nullptr;

// ─── 摄像头引脚定义（AI-Thinker ESP32-CAM） ─────────────────
static const camera_config_t cameraConfig = {
    .pin_pwdn       = 32,
    .pin_reset      = -1,
    .pin_xclk       = 0,
    .pin_sscb_sda   = 26,
    .pin_sscb_scl   = 27,
    .pin_d7         = 35,
    .pin_d6         = 34,
    .pin_d5         = 39,
    .pin_d4         = 36,
    .pin_d3         = 21,
    .pin_d2         = 19,
    .pin_d1         = 18,
    .pin_d0         = 5,
    .pin_vsync      = 25,
    .pin_href       = 23,
    .pin_pclk       = 22,
    .xclk_freq_hz   = 20000000,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,
    .pixel_format   = PIXFORMAT_JPEG,       // 硬件直接输出 JPEG
    .frame_size     = CAMERA_FRAME_SIZE,
    .jpeg_quality   = CAMERA_JPEG_QUALITY,
    .fb_count       = CAMERA_FB_COUNT,
    .fb_location    = CAMERA_FB_IN_PSRAM,
    .grab_mode      = CAMERA_GRAB_WHEN_EMPTY,
};

// ═════════════════════════════════════════════════════════════
//  摄像头初始化
// ═════════════════════════════════════════════════════════════
static bool initCamera() {
    esp_err_t err = esp_camera_init(&cameraConfig);
    if (err != ESP_OK) {
        Serial.printf("[CAM] 初始化失败: 0x%x\n", err);
        return false;
    }

    // 应用预处理配置
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, PREPROCESS_BRIGHTNESS);
        s->set_contrast(s, PREPROCESS_CONTRAST);
        s->set_saturation(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_hmirror(s, PREPROCESS_MIRROR);    // 水平镜像
        s->set_vflip(s, PREPROCESS_FLIP);        // 垂直翻转
        s->set_quality(s, CAMERA_JPEG_QUALITY);
        s->set_framesize(s, CAMERA_FRAME_SIZE);

        Serial.printf("[CAM] %dx%d, quality=%d, mirror=%d, flip=%d\n",
                      s->status.width, s->status.height,
                      CAMERA_JPEG_QUALITY,
                      PREPROCESS_MIRROR, PREPROCESS_FLIP);
    }
    return true;
}

// ═════════════════════════════════════════════════════════════
//  帧采集（线程安全）
// ═════════════════════════════════════════════════════════════
// 返回的 fb 使用完后必须调用 freeFrame() 释放
struct CapturedFrame {
    uint8_t* data;
    size_t len;
};

static CapturedFrame captureFrame() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        return {nullptr, 0};
    }

    CapturedFrame frame = {fb->buf, fb->len};

#if PREPROCESS_GRAYSCALE
    // 灰度转换：将 JPEG 解码后转灰度再重新编码（简化版：直接调整传感器）
    // 传感器级别的灰度由 s->set_special_effect(s, 2) 实现
#endif

    return frame;
}

static void freeFrame(CapturedFrame& frame) {
    if (frame.data) {
        esp_camera_fb_return((camera_fb_t*)frame.data);
        frame.data = nullptr;
        frame.len = 0;
    }
}

// ═════════════════════════════════════════════════════════════
//  MJPEG 流处理器
// ═════════════════════════════════════════════════════════════
void handleStream(AsyncWebServerRequest* request) {
    auto* response = request->beginResponseStream(
        "multipart/x-mixed-replace; boundary=frame"
    );
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->setCode(200);

    while (request->client()->connected()) {
        CapturedFrame frame = captureFrame();
        if (frame.data) {
            response->printf("--frame\r\n"
                             "Content-Type: image/jpeg\r\n"
                             "Content-Length: %zu\r\n\r\n",
                             frame.len);
            response->write(frame.data, frame.len);
            freeFrame(frame);
        }
        delay(1000 / 30);  // 目标 ~30fps
    }

    request->send(response);
}

// ═════════════════════════════════════════════════════════════
//  Web 界面
// ═════════════════════════════════════════════════════════════
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>ESP32-CAM 手语识别</title>
    <style>
        body{font-family:Arial;text-align:center;background:#1a1a2e;color:#eee;margin:20px}
        img{max-width:100%;border:2px solid #333;border-radius:8px}
        .info{color:#aaa;margin:16px 0;font-size:14px}
        code{background:#333;padding:2px 8px;border-radius:4px}
    </style>
</head>
<body>
    <h2>📷 CSL Recognition — CAM</h2>
    <img src="/stream" id="video">
    <div class="info">
        <p>ESP32-S3 连接地址:</p>
        <code>http://%s:80/stream</code>
    </div>
    <script>
        setInterval(() => { img.src='/stream?'+Date.now(); }, 60000);
    </script>
</body>
</html>
)rawliteral";

void handleRoot(AsyncWebServerRequest* request) {
    char buf[512];
    snprintf(buf, sizeof(buf), INDEX_HTML, WiFi.localIP().toString().c_str());
    request->send(200, "text/html", buf);
}

// ═════════════════════════════════════════════════════════════
//  WiFi 连接
// ═════════════════════════════════════════════════════════════
static bool connectWiFi() {
    Serial.printf("[WiFi] 正在连接 %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    for (int i = 0; i < 40; i++) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        delay(500);
        Serial.print(".");
    }

    // 连接失败 → 创建热点作为 fallback
    Serial.println("\n[WiFi] 连接失败，创建热点 CSL-CAM");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CSL-CAM", "12345678");
    Serial.printf("[WiFi] 热点 IP: %s\n", WiFi.softAPIP().toString().c_str());
    return false;
}

// ═════════════════════════════════════════════════════════════
//  入口
// ═════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== CSL CAM ===");

    if (!initCamera()) {
        Serial.println("[CAM] 初始化失败，重启中...");
        delay(3000);
        ESP.restart();
    }

    connectWiFi();

    server.on("/", handleRoot);
    server.on("/stream", HTTP_GET, handleStream);
    server.begin();

    Serial.printf("[HTTP] http://%s/stream\n",
                  WiFi.getMode() == WIFI_AP ?
                    WiFi.softAPIP().toString().c_str() :
                    WiFi.localIP().toString().c_str());
    Serial.println("[系统] 就绪，等待 S3 连接...");
}

void loop() {
    delay(10000);
}

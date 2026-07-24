// ===========================================================
//  ESP32-S3 配置模板
// ===========================================================
// 使用: 复制为 config.h 并修改其中的参数
//   cp config.example.h config.h
// config.h 已被 .gitignore 忽略，不会误提交
//
// 上半部分为"用户配置"——初次使用必须修改
// 下半部分为"系统常量"——与训练配置绑定，一般无需修改
// ===========================================================

#ifndef CONFIG_H_
#define CONFIG_H_

// ╔════════════════════════════════════════════════════════════
// ║  用户配置——首次使用请修改
// ╚════════════════════════════════════════════════════════════

// ─── WiFi ───────────────────────────────────────────────────
// 与 ESP32-CAM 同在一个局域网
#define WIFI_SSID         "YourWiFi"
#define WIFI_PASSWORD     "YourPassword"
#define WIFI_TIMEOUT_MS   15000

// ─── ESP32-CAM 视频流 ──────────────────────────────────────
// 烧录 CAM 后从串口获取它的 IP 地址
#define CAM_STREAM_URL    "http://192.168.1.100:80/stream"
#define CAM_RECONNECT_MS  5000

// ─── MQTT ───────────────────────────────────────────────────
#define MQTT_BROKER       "broker.emqx.io"
#define MQTT_PORT         1883
#define MQTT_CLIENT_ID    "csl_esp32s3_01"

#define MQTT_TOPIC_RESULT "csl/result"    // 发布识别结果
#define MQTT_TOPIC_STATUS "csl/status"    // 发布设备状态
#define MQTT_TOPIC_COMMAND "csl/command"  // 订阅控制命令

// ╔════════════════════════════════════════════════════════════
// ║  系统常量——与训练配置绑定，一般无需修改
// ╚════════════════════════════════════════════════════════════

// ─── 模型输入 ───────────────────────────────────────────────
// 以下三个值必须与 src/config.py 一致
#define INPUT_SEQ_LEN      30     // 每手势帧数
#define INPUT_FEATURES     63     // 21关键点 × 3坐标
#define INPUT_FLAT_SIZE    (INPUT_SEQ_LEN * INPUT_FEATURES)  // 1890

// ─── 推理引擎 ───────────────────────────────────────────────
#define TENSOR_ARENA_SIZE  (150 * 1024)   // TFLite Micro 张量内存 (150 KB)

// ─── 帧处理 ─────────────────────────────────────────────────
#define CAM_WIDTH          320
#define CAM_HEIGHT         240
#define FRAME_BUFFER_SIZE  (60 * 1024)    // JPEG 帧缓冲 (60 KB)

// ─── OLED 显示屏（可选） ────────────────────────────────────
// 如需使用，取消注释下行
// #define USE_DISPLAY
#ifdef USE_DISPLAY
  #define OLED_SDA         41
  #define OLED_SCL         42
  #define OLED_WIDTH       128
  #define OLED_HEIGHT      64
  #define OLED_ADDR        0x3C
#endif

#endif  // CONFIG_H_

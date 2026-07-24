// ===========================================================
//  ESP32-CAM 配置模板
// ===========================================================
// 使用: 复制为 config.h 并修改其中的参数
//   cp config.example.h config.h
// config.h 已被 .gitignore 忽略，不会误提交
// ===========================================================

#ifndef CONFIG_H_
#define CONFIG_H_

// ─── WiFi ───────────────────────────────────────────────────
#define WIFI_SSID        "YourWiFi"
#define WIFI_PASSWORD    "YourPassword"

// ─── 摄像头 ─────────────────────────────────────────────────
#define CAMERA_FRAME_SIZE    FRAMESIZE_QVGA   // QVGA(320×240) / VGA(640×480)
#define CAMERA_JPEG_QUALITY  15                // 0~63，越小画质越好
#define CAMERA_FB_COUNT      2                 // 帧缓冲数

// ─── 预处理 ─────────────────────────────────────────────────
#define PREPROCESS_MIRROR     1    // 水平镜像（自拍视角）
#define PREPROCESS_FLIP       0    // 垂直翻转
#define PREPROCESS_CONTRAST   0    // 对比度 -2~2
#define PREPROCESS_BRIGHTNESS 0    // 亮度 -2~2

// ─── HTTP ───────────────────────────────────────────────────
#define HTTP_PORT  80

#endif  // CONFIG_H_

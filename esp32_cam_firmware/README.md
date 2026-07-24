# ESP32-CAM 固件 — 摄像头端

将 ESP32-CAM 配置为 WiFi 摄像头，采集图像并经过预处理后，通过 MJPEG 视频流发送给 ESP32-S3。

## 目录结构

```
esp32_cam_firmware/
├── config/
│   ├── config.example.h      ← 配置模板（提交到 git）
│   └── config.h              ← 你的配置（被 git 忽略，需自行创建）
├── include/                   ← 系统头文件（自动生成）
├── src/
│   └── main.cpp               ← 固件主程序
├── platformio.ini             ← PlatformIO 工程配置
└── README.md                  ← 本文件
```

## 环境要求

- **VSCode** + **PlatformIO IDE** 插件
- 或 **CLI**: `pip install platformio`
- **ESP32-CAM 开发板**（AI-Thinker 模组）
- **USB 转串口模块**（如 FTDI / CH340，ESP32-CAM 通常不自带 USB）

## 编译与烧录

### 第 1 步：创建配置文件

```bash
# 从模板复制出实际配置
cd esp32_cam_firmware
cp config/config.example.h config/config.h
```

### 第 2 步：修改配置

编辑 `config/config.h`，修改以下参数：

```c
// 必改——WiFi 名称和密码
#define WIFI_SSID        "你的WiFi名称"
#define WIFI_PASSWORD    "你的WiFi密码"

// 可选——分辨率、画质等
#define CAMERA_FRAME_SIZE    FRAMESIZE_QVGA   // QVGA(320×240) 或 VGA(640×480)
#define CAMERA_JPEG_QUALITY  15                // 0~63，越小越好
```

### 第 3 步：连接硬件

ESP32-CAM 通常不自带 USB 口，需要通过 **USB 转串口模块** 连接：

| USB-TTL | ESP32-CAM |
|---------|-----------|
| 5V | 5V |
| GND | GND |
| TXD | U0R (GPIO3) |
| RXD | U0T (GPIO1) |
| — | IO0 接 GND（烧录时） |

> ⚠ **烧录时**：GPIO0 必须接 GND，烧录完成后断开即可正常运行。
> ⚠ **供电注意**：建议用 5V 供电，WiFi 开启时电流可能达 200mA+。

### 第 4 步：编译并烧录

**方式一：VSCode + PlatformIO**

1. 打开 `esp32_cam_firmware/` 文件夹
2. 点击左侧 PlatformIO 图标 → **→ (Build & Upload)**
3. 等待编译完成自动烧录

**方式二：CLI**

```bash
pio run -d esp32_cam_firmware -t upload
```

### 第 5 步：查看运行状态

烧录完成后，断开 GPIO0 的 GND 连接，按复位键。

打开串口监视器（PlatformIO 点 🔌 图标，或 CLI）：

```bash
pio run -d esp32_cam_firmware -t monitor
```

正常启动后会看到：

```
=== CSL CAM ===
[CAM] 320x240, quality=15, mirror=1, flip=0
[WiFi] 已连接, IP: 192.168.1.100
[HTTP] http://192.168.1.100/stream
[系统] 就绪，等待 S3 连接...
```

记下 **IP 地址**（上面例子的 `192.168.1.100`），配置 S3 时需要用到。

## 验证

浏览器打开 `http://<CAM的IP>/stream`，如果看到实时画面，说明摄像头工作正常。

## 接线图（烧录时）

```
USB-TTL        ESP32-CAM
┌──────┐       ┌─────────┐
│  5V  │──────→│ 5V      │
│ GND  │──────→│ GND     │
│ TXD  │──────→│ U0R     │
│ RXD  │←──────│ U0T     │
│      │       │ GPIO0 ──╯ GND（烧录时短接）
└──────┘       └─────────┘
```

> 烧录完成后，断开 GPIO0 与 GND 的连接并复位。

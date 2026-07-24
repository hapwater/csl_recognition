# ESP32-S3 固件 — 推理与 MQTT 发布端

接收 ESP32-CAM 的 MJPEG 视频流，解码后提取手部关键点，运行 TFLite Micro INT8 推理，通过 MQTT 发布识别结果。

## 目录结构

```
esp32_firmware/
├── config/
│   ├── config.example.h      ← 配置模板（提交到 git）
│   └── config.h              ← 你的配置（被 git 忽略，需自行创建）
├── include/
│   ├── classes.h              ← 类别列表（由训练自动生成）
│   └── model_data.h           ← INT8 模型数据（由量化自动生成）
├── src/
│   ├── main.cpp               ← 主程序入口 + 状态机
│   ├── cam_stream.cpp/.h      ← CAM 视频流接收（MJPEG 解析）
│   ├── frame_processor.cpp/.h ← JPEG 解码 + 关键点提取 + 缓冲 + 推理
│   ├── inference.cpp/.h       ← TFLite Micro 推理引擎
│   ├── wifi_manager.cpp/.h    ← WiFi 管理
│   └── mqtt_manager.cpp/.h    ← MQTT 客户端
├── platformio.ini             ← PlatformIO 工程配置
└── README.md                  ← 本文件
```

## 编译前的准备工作

S3 固件依赖训练好的模型文件，因此在首次编译前需要先在 PC 上完成训练和量化。

### 第 1 步：安装 PC 端依赖

```bash
# 在项目根目录下
python -m venv venv
venv\Scripts\activate        # Windows
# source venv/bin/activate   # Linux / macOS

pip install -r requirements.txt
```

### 第 2 步：下载 MediaPipe 模型

```bash
# 项目根目录下
# Windows (PowerShell)
curl -o hand_landmarker.task https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task
```

### 第 3 步：采集手势数据

```bash
python collect_data.py --class_name "你好" --num_sequences 30
python collect_data.py --class_name "我" --num_sequences 30
# ……继续添加其他词汇
```

### 第 4 步：训练模型

```bash
python train_model.py --model cnn
```

### 第 5 步：量化并导出到固件

```bash
python scripts/copy_model_to_firmware.py --validate
```

这一步会自动：
1. 将 Keras 模型量化为 INT8 TFLite
2. 生成 `outputs/model_data.h`（C 头文件）
3. 生成 `outputs/classes.h`（类别列表）
4. 复制到 `esp32_firmware/include/`

---

## 编译与烧录

### 第 1 步：创建配置文件

```bash
cd esp32_firmware
cp config/config.example.h config/config.h
```

### 第 2 步：修改配置

编辑 `config/config.h`，必改项：

```c
// ── 必改：WiFi（与 CAM 同一网络） ──
#define WIFI_SSID         "你的WiFi名称"
#define WIFI_PASSWORD     "你的WiFi密码"

// ── 必改：CAM 地址（查看 CAM 串口输出获取） ──
#define CAM_STREAM_URL    "http://192.168.1.100:80/stream"

// ── 可选：MQTT ──
#define MQTT_BROKER       "broker.emqx.io"      // 公共 Broker，无需修改
// #define MQTT_BROKER    "192.168.1.200"       // 或自行搭建的 Broker IP
```

### 第 3 步：确认模型已导出

检查 `include/` 目录下是否存在：

```
include/model_data.h     ← 必须有（约 30 KB）
include/classes.h        ← 必须有（你的词汇列表）
```

如果缺失，回到前面的**第 5 步**运行：

```bash
python scripts/copy_model_to_firmware.py --validate
```

### 第 4 步：连接硬件

| ESP32-S3 开发板 | 连接方式 |
|----------------|----------|
| ESP32-S3-DevKitC | **直接 USB-C** 连接电脑 |
| 其他 S3 板 | 通常自带 USB，直接插即可 |

### 第 5 步：编译并烧录

**方式一：VSCode + PlatformIO**

1. 打开 `esp32_firmware/` 文件夹
2. 点击左侧 PlatformIO 图标 → **→ (Build & Upload)**
3. 等待编译完成自动烧录

**方式二：CLI**

```bash
pio run -d esp32_firmware -t upload
```

### 第 6 步：查看运行状态

打开串口监视器：

```bash
pio run -d esp32_firmware -t monitor
```

正常启动流程：

```
=== CSL Recognition — ESP32-S3 ===
固件: Jul 24 2026 15:00:00
CAM: http://192.168.1.100:80/stream

[系统] 初始化帧处理器...
[FProc] 解码缓冲: 320x240 (150 KB)
[Infer] 模型加载成功 (3 类, arena=150 KB)
[状态] WIFI_CONN → MQTT_CONN → CAM_CONN → RUNNING

[CAM] 已连接 192.168.1.100:80/stream
[FProc] 推理: 你好 (0.92), 耗时 45 ms
[MQTT] 已发布: {"word":"你好","confidence":0.92}
```

---

## 验证

订阅 MQTT 结果，在 PC 或手机上运行：

```bash
python mqtt-client/mqtt_subscriber.py --broker broker.emqx.io
```

对着 CAM 摆手势，终端会实时打印识别结果。

## 常见问题

**Q: 编译报错 `#error "model_data.h 尚未生成"`**
→ 模型还没导出到固件。运行 `python scripts/copy_model_to_firmware.py --validate`

**Q: 烧录后串口显示 `CAM: disconnected`**
→ CAM 还没开机或 IP 地址不对。检查 `config/config.h` 中的 `CAM_STREAM_URL`

**Q: MQTT 收不到结果**
→ 确认 MQTT Broker 地址正确，ESP32-S3 和 PC 在同一个网络

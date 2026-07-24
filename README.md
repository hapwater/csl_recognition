# CSL Recognition — 中文手语识别

基于 **ESP32-CAM + ESP32-S3** 的超轻量级手语识别智能终端。ESP32-CAM 采集图像并通过 WiFi 传输到 ESP32-S3 进行 TFLite Micro 推理，识别结果通过 MQTT 发布到任意订阅设备（电脑/手机）。

## 项目目标

- 支持 ~200 个常用中文手语词汇的识别
- CNN 模型体积经 INT8 量化后仅 **~30 KB**，可在 ESP32-S3 上实时运行
- 提供完整的数据采集 → 训练 → 量化 → 部署流程

## 系统架构

```
┌──────────────┐   WiFi (MJPEG)   ┌──────────────────┐   MQTT   ┌──────────┐
│  ESP32-CAM   │ ───────────────→ │    ESP32-S3      │ ──────→ │ 任何设备  │
│              │   视频流 (320×240)│                  │  "你好"  │          │
│  摄像头采集   │                  │  JPEG解码         │          │ 电脑/手机 │
│  图像预处理   │                  │  手部关键点提取    │          │ 实时显示  │
│  JPEG 编码   │                  │  序列缓冲(30帧)    │          │          │
│              │                  │  TFLite 推理      │          │          │
└──────────────┘                  │  MQTT 发布        │          └──────────┘
                                  └──────────────────┘
```

- **ESP32-CAM**：负责 OV2640 摄像头采集、图像预处理（镜像/对比度/亮度）、JPEG 编码，通过 WiFi 以 MJPEG 流格式发送
- **ESP32-S3**：接收视频流、解码 JPEG 帧、提取手部关键点特征、累积 30 帧后 TFLite Micro 推理、MQTT 发布识别结果
- **MQTT 订阅者**：任意设备（电脑/手机）订阅 `csl/result` 主题，接收并显示识别结果

## 项目结构

```
├── src/                          # Python 训练工具
│   ├── config.py                 # 全局训练配置
│   └── __init__.py
├── esp32_cam_firmware/           # ESP32-CAM 固件 ← 摄像头端
│   ├── platformio.ini
│   ├── config/config.example.h   # 配置模板（复制为 config.h 使用）
│   └── src/main.cpp              # 采集 → 预处理 → MJPEG 流
├── esp32_firmware/               # ESP32-S3 固件 ← 推理端
│   ├── platformio.ini
│   ├── config/config.example.h   # 配置模板
│   │   ├── classes.h             # 类别列表（由训练生成）
│   │   └── model_data.h          # INT8 模型（由量化生成）
│   └── src/
│       ├── main.cpp              # 入口 + 状态机
│       ├── cam_stream.cpp/.h     # MJPEG 流接收 + 解析
│       ├── frame_processor.cpp/.h # JPEG 解码 + 关键点提取
│       ├── inference.cpp/.h      # TFLite Micro 推理
│       ├── wifi_manager.cpp/.h   # WiFi 连接管理
│       └── mqtt_manager.cpp/.h   # MQTT 客户端
├── scripts/
│   └── copy_model_to_firmware.py # 模型 → C 头文件 → 固件工程
├── mqtt-client/                   # MQTT 订阅端（任何设备查看结果）
│   └── mqtt_subscriber.py         # 订阅 csl/result 主题，显示识别结果
├── data/landmarks/               # 手势关键点训练数据
├── outputs/                      # 训练产出（模型、类别文件）
├── collect_data.py               # 数据采集
├── train_model.py                # CNN / GRU 模型训练
├── quantize_model.py             # INT8 量化
├── extract_landmarks_dataset.py  # 视频数据集批量提取
├── test_mediapipe_webcam.py      # PC 端摄像头验证
└── demo_gru_webcam.py            # PC 端本地演示
```

## 命令速查表

| 用途 | 命令 | 说明 |
|------|------|------|
| **环境** | | |
| 创建虚拟环境 | `python -m venv venv` | 只需一次 |
| 激活虚拟环境(Win) | `venv\Scripts\activate` | Windows |
| 激活虚拟环境(Mac) | `source venv/bin/activate` | Linux/macOS |
| 安装依赖 | `pip install -r requirements.txt` | 加 `-i https://pypi.tuna.tsinghua.edu.cn/simple` 换国内源 |
| **训练（PC 端完成）** | | |
| 验证摄像头 | `python test_mediapipe_webcam.py` | 按 ESC 退出 |
| 采集手势 | `python collect_data.py --class_name "你好" --num_sequences 30` | 每个词至少 30 组 |
| 训练 CNN 模型 | `python train_model.py --model cnn` | 🔵 ESP32 兼容 |
| 训练 GRU 模型 | `python train_model.py --model gru` | PC 端演示用 |
| 导出模型到固件 | `python scripts/copy_model_to_firmware.py --validate` | 一键量化+复制 |
| 视频批量提取 | `python extract_landmarks_dataset.py --video_dir data/national_csl` | 批量处理数据集 |
| PC 端实时演示 | `python demo_gru_webcam.py --mode trigger` | 不连 ESP32 时测试 |
| **编译烧录** | | |
| 编译烧录 CAM | `pio run -d esp32_cam_firmware -t upload` | 或用 VSCode 打开文件夹 |
| 编译烧录 S3 | `pio run -d esp32_firmware -t upload` | 同上 |
| 查看串口日志 | `pio run -d esp32_firmware -t monitor` | 查看运行日志 |
| **部署后** | | |
| 订阅识别结果 | `python mqtt-client/mqtt_subscriber.py --broker broker.emqx.io` | 任意设备上运行 |

---

## 环境安装

### 系统要求

- Python 3.9 – 3.12（推荐 3.10/3.11）
- 摄像头（用于 PC 端采集训练数据）
- Windows / Linux / macOS 均可

### 1. 创建虚拟环境

```bash
# Windows
python -m venv venv
venv\Scripts\activate

# Linux / macOS
python3 -m venv venv
source venv/bin/activate
```

### 2. 安装依赖

```bash
pip install -r requirements.txt
```

### 3. 下载 MediaPipe 手部关键点模型

仅在 PC 端采集数据时使用，不烧录到 ESP32：

```bash
# Windows (PowerShell)
curl -o hand_landmarker.task https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task

# Windows (cmd)
certutil -urlcache -split -f https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task hand_landmarker.task

# Linux / macOS
wget https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task
```

### 依赖清单

| 包名 | 用途 |
|------|------|
| `mediapipe` | PC 端手部关键点提取（仅训练数据采集用） |
| `opencv-python` | 摄像头采集与图像显示 |
| `numpy` | 数据处理 |
| `tensorflow` | 模型训练与 INT8 量化 |
| `scikit-learn` | 数据集分割 |
| `tqdm` | 进度条 |
| `paho-mqtt` | PC 端 MQTT 订阅 |

### PlatformIO（编译 ESP32 固件）

1. 在 VSCode 中安装 [PlatformIO IDE](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) 插件
2. 分别打开 `esp32_cam_firmware/` 和 `esp32_firmware/` 两个工程

---

## 训练流程

### 第 0 步：验证摄像头与 MediaPipe

```bash
python test_mediapipe_webcam.py
```

### 第 1 步：采集手势数据

```bash
python collect_data.py --class_name "你好" --num_sequences 30
python collect_data.py --class_name "我" --num_sequences 30
python collect_data.py --class_name "谢谢" --num_sequences 30
```

### 第 2 步：训练模型

```bash
python train_model.py --model cnn
```

### 第 3 步：量化并导出到固件

```bash
# 一键完成
python scripts/copy_model_to_firmware.py --validate
```

---

## ESP32 部署流程

### 1. 烧录 ESP32-CAM（摄像头端）

1. 创建并编辑 `esp32_cam_firmware/config/config.h`（复制模板后填入 WiFi）
2. 打开 `esp32_cam_firmware/` → PlatformIO → 编译并烧录
3. 打开串口监视器，记下 CAM 的 IP 地址

### 2. 烧录 ESP32-S3（推理端）

1. 创建并编辑 `esp32_firmware/config/config.h`（复制模板后配置 WiFi、CAM 地址）
   - 填入 WiFi 信息
   - 将 `CAM_STREAM_URL` 改为上一步得到的 CAM IP 地址
2. 确认模型已导出（`python scripts/copy_model_to_firmware.py`）
3. 打开 `esp32_firmware/` → PlatformIO → 编译并烧录

### 3. MQTT Broker

```bash
# 使用公共 Broker（无需搭建）
#   默认 broker.emqx.io:1883

# 或自行搭建（推荐，局域网更稳定）
docker run -d --name mosquitto -p 1883:1883 eclipse-mosquitto
```

### 4. 启动 MQTT 订阅者（PC/手机）

```bash
python mqtt-client/mqtt_subscriber.py --broker broker.emqx.io
```

连上后即可实时看到 ESP32-S3 发布的识别结果。

---

## ESP32 固件详解

### ESP32-CAM（摄像头端）

| 模块 | 功能 |
|------|------|
| 摄像头初始化 | OV2640 驱动、分辨率/质量设置 |
| 图像预处理 | 镜像翻转、对比度/亮度调节（传感器级） |
| MJPEG 流 | HTTP multipart/x-mixed-replace 实时视频流 |
| WiFi 连接 | 优先 STA 模式，连接失败自动切 AP 热点 |

**配置项**（`esp32_cam_firmware/config/config.h`）：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `CAMERA_FRAME_SIZE` | QVGA | 分辨率 (320×240) |
| `CAMERA_JPEG_QUALITY` | 15 | JPEG 质量 (0-63) |
| `PREPROCESS_MIRROR` | 1 | 水平镜像 |
| `PREPROCESS_CONTRAST` | 0 | 对比度 (-2~2) |

### ESP32-S3（推理端）

| 模块 | 文件 | 职责 |
|------|------|------|
| CAM 流客户端 | `cam_stream.cpp/.h` | HTTP MJPEG 流接收、边界解析、帧提取 |
| 帧处理器 | `frame_processor.cpp/.h` | JPEG 解码 → 手部关键点 → 序列缓冲 → 推理 |
| 推理引擎 | `inference.cpp/.h` | TFLite Micro INT8 量化推理 |
| WiFi 管理 | `wifi_manager.cpp/.h` | 连接管理 + 自动重连 |
| MQTT 管理 | `mqtt_manager.cpp/.h` | 发布/订阅 + 自动重连 |

**状态机**:

```
INIT → WIFI_CONN → MQTT_CONN → CAM_CONN → RUNNING
                                            ↑   |
                                            └───┘ (断线重连)
```

**MQTT 主题**:

| 主题 | 方向 | 格式 |
|------|------|------|
| `csl/result` | S3 → 订阅者 | `{"word":"你好","confidence":0.92}` |
| `csl/status` | S3 → 订阅者 | `{"ip":"192.168.1.x","fps":15}` |
| `csl/command` | 订阅者 → S3 | `{"cmd":"reset"}` (重启/重连) |

---

## 训练配置参数

所有可调参数集中在 `src/config.py`：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `SEQ_LEN` | 30 | 每个手势的采样帧数 |
| `N_LANDMARKS` | 63 | 21 个关键点 × 3 坐标 |
| `CNN_FILTERS` | [64, 64, 64] | 三层 CNN 通道数 |
| `CNN_KERNEL_SIZE` | 3 | 卷积核大小 |
| `GRU_HIDDEN` | 64 | GRU 隐藏层单元数（PC 端用） |
| `DROPOUT` | 0.3 | Dropout 比率 |
| `BATCH_SIZE` | 32 | 训练批次大小 |
| `EPOCHS` | 50 | 最大训练轮数 |
| `LEARNING_RATE` | 0.001 | 学习率 |

ESP32-S3 端配置在 `esp32_firmware/config/config.h`，CAM 端在 `esp32_cam_firmware/config/config.h` 中修改。

## License

MIT

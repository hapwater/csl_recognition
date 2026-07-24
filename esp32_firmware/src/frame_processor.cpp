// frame_processor.cpp — 视频帧处理器实现

#include "frame_processor.h"
#include "config.h"

#include <TJpg_Decoder.h>

// ─── 静态回调函数所需的全局指针 ─────────────────────────────
static uint16_t* s_decodeOutput = nullptr;
static int s_decodeWidth = 0;
static int s_decodeHeight = 0;

// TJpg_Decoder 输出回调：将解码后的像素块写入内存缓冲
static bool tjpeg_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!s_decodeOutput) return false;
    for (int row = 0; row < h; row++) {
        memcpy(s_decodeOutput + (y + row) * s_decodeWidth + x,
               bitmap + row * w,
               w * sizeof(uint16_t));
    }
    return true;
}


// ═════════════════════════════════════════════════════════════
//  构造 / 析构
// ═════════════════════════════════════════════════════════════
FrameProcessor::FrameProcessor()
    : _state(ProcessorState::IDLE)
    , _decodedFrame(nullptr)
    , _decodedWidth(0)
    , _decodedHeight(0)
    , _bufferCount(0)
    , _hasHandRecently(false)
    , _lastFrameTime(0)
    , _frameCount(0)
    , _currentFps(0.0f)
    , _hasResult(false)
    , _frameSkip(2)           // 每 3 帧处理 1 帧
    , _frameSkipCounter(0)
{
    memset(_buffer, 0, sizeof(_buffer));
    memset(&_lastResult, 0, sizeof(_lastResult));
}

bool FrameProcessor::begin() {
    // ── 初始化 JPEG 解码器 ──
    TJpgDec.setCallback(tjpeg_callback);
    TJpgDec.setSwapBytes(true);   // RGB565 字节序

    // ── 分配解码帧缓冲 (PSRAM 优先) ──
    size_t frameSize = CAM_WIDTH * CAM_HEIGHT * sizeof(uint16_t);
    _decodedFrame = (uint16_t*)ps_calloc(1, frameSize);
    if (!_decodedFrame) {
        Serial.printf("[FProc] 解码缓冲分配失败 (%zu KB)\n", frameSize / 1024);
        return false;
    }
    Serial.printf("[FProc] 解码缓冲: %dx%d (%zu KB)\n",
                  CAM_WIDTH, CAM_HEIGHT, frameSize / 1024);

    // ── 初始化推理引擎 ──
    if (!_inference.begin()) {
        Serial.println("[FProc] ⚠ 推理引擎初始化失败（但帧处理器仍可运行）");
    }

    _state = ProcessorState::IDLE;
    return true;
}

// ═════════════════════════════════════════════════════════════
//  帧处理入口
// ═════════════════════════════════════════════════════════════
void FrameProcessor::processFrame(const uint8_t* jpegData, size_t jpegLen) {
    // ── 帧率统计 ──
    _frameCount++;
    unsigned long now = millis();
    if (now - _lastFrameTime > 1000) {
        _currentFps = _frameCount * 1000.0f / (now - _lastFrameTime);
        _frameCount = 0;
        _lastFrameTime = now;
    }

    // ── 跳帧（没必要每帧都处理，节省 CPU） ──
    _frameSkipCounter++;
    if (_frameSkipCounter <= _frameSkip) return;
    _frameSkipCounter = 0;

    _state = ProcessorState::DECODING;

    // ── 1. 解码 JPEG → RGB565 ──
    if (!decodeJpeg(jpegData, jpegLen)) {
        _state = ProcessorState::IDLE;
        return;
    }

    _state = ProcessorState::DETECTING;

    // ── 2. 提取手部关键点 ──
    LandmarkFrame lf = detectHandLandmarks(_decodedFrame, _decodedWidth, _decodedHeight);

    // ── 3. 加入缓冲 ──
    if (lf.hasHand) {
        appendToBuffer(lf);
    }

    _state = ProcessorState::IDLE;
}

// ═════════════════════════════════════════════════════════════
//  JPEG 解码
// ═════════════════════════════════════════════════════════════
bool FrameProcessor::decodeJpeg(const uint8_t* jpegData, size_t jpegLen) {
    // 获取 JPEG 尺寸
    uint16_t w, h;
    // TJpgDec 返回 0=成功, 非0=失败
    if (TJpgDec.getJpgSize(&w, &h, const_cast<uint8_t*>(jpegData), jpegLen) != 0) {
        Serial.println("[FProc] 获取 JPEG 尺寸失败");
        return false;
    }

    // 设置全局指针供回调使用
    s_decodeOutput = _decodedFrame;
    s_decodeWidth = w;
    s_decodeHeight = h;
    _decodedWidth = w;
    _decodedHeight = h;

    // 清空解码缓冲
    memset(_decodedFrame, 0, w * h * sizeof(uint16_t));

    // 解码（1:1 缩放，绘制到 (0,0)）
    if (TJpgDec.drawJpg(0, 0, const_cast<uint8_t*>(jpegData), jpegLen) != 0) {
        Serial.println("[FProc] JPEG 解码失败");
        return false;
    }

    return true;
}

// ═════════════════════════════════════════════════════════════
//  手部关键点提取
// ═════════════════════════════════════════════════════════════
//
//  TODO: 替换为实际的手部关键点检测模型
//  ───────────────────────────────────────────────────────────
//  当前实现：基于肤色分割的简单手部检测。
//  只输出手部 bounding box 的近似关键点（而非 MediaPipe 的 21 点）。
//  如需精确 21 个关键点，需要额外训练轻量级 TFLite 关键点模型。
//
//  备选手部检测方案:
//    1. 训练一个 MobileNet-based 手部关键点 TFLite 模型
//    2. 使用 ESP-DL (Espressif 深度学习库) 的检测模型
//    3. 简单 CNN 分类 + 回归网络
//
LandmarkFrame FrameProcessor::detectHandLandmarks(const uint16_t* rgb565, int w, int h) {
    LandmarkFrame lf;
    memset(&lf, 0, sizeof(lf));
    lf.hasHand = false;

    // ── 肤色分割（YCbCr 色彩空间） ──
    // 典型肤色范围: Y: 0-255, Cb: 77-127, Cr: 133-173
    int minX = w, minY = h, maxX = 0, maxY = 0;
    int skinPixelCount = 0;

    for (int y = 0; y < h; y += 4) {       // 步长 4 加速
        for (int x = 0; x < w; x += 4) {
            uint16_t pixel = rgb565[y * w + x];

            // RGB565 → (R5,G6,B5) → YCbCr 近似
            int r = (pixel >> 11) & 0x1F;
            int g = (pixel >> 5) & 0x3F;
            int b = pixel & 0x1F;
            r = (r * 255) / 31;
            g = (g * 255) / 63;
            b = (b * 255) / 31;

            // 简化的 YCbCr 转换
            int cb = -0.1687 * r - 0.3313 * g + 0.5 * b + 128;
            int cr = 0.5 * r - 0.4187 * g - 0.0813 * b + 128;

            if (cb >= 77 && cb <= 127 && cr >= 133 && cr <= 173) {
                skinPixelCount++;
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }

    // ── 判断是否有手 ──
    int regionW = maxX - minX;
    int regionH = maxY - minY;
    float skinRatio = (float)skinPixelCount / ((w / 4) * (h / 4));

    if (skinRatio > 0.01f && regionW > 20 && regionH > 20) {
        lf.hasHand = true;
        _hasHandRecently = true;

        // ── 生成 21 个近似关键点 ──
        // 将 hand region 划分成 7×3 网格，每个格子中心作为关键点
        // 原始 MediaPipe 的 21 点无法从肤色分割还原，
        // 这里用网格近似填充，真实场景应替换为关键点检测模型。
        float cx = (minX + maxX) / 2.0f / w;
        float cy = (minY + maxY) / 2.0f / h;
        float bw = (maxX - minX) / (float)w;
        float bh = (maxY - minY) / (float)h;

        // Palm center 和 fingers 近似散布
        float palmCx = cx;
        float palmCy = cy + bh * 0.1f;   // 手掌中心偏下

        int idx = 0;
        // 手腕 (landmark 0)
        lf.data[idx++] = cx;
        lf.data[idx++] = cy + bh * 0.45f;
        lf.data[idx++] = 0;

        // 拇指 (landmark 1-4)
        lf.data[idx++] = cx - bw * 0.3f; lf.data[idx++] = cy + bh * 0.2f; lf.data[idx++] = 0;
        lf.data[idx++] = cx - bw * 0.35f; lf.data[idx++] = cy + bh * 0.05f; lf.data[idx++] = 0;
        lf.data[idx++] = cx - bw * 0.3f;  lf.data[idx++] = cy - bh * 0.1f; lf.data[idx++] = 0;
        lf.data[idx++] = cx - bw * 0.25f; lf.data[idx++] = cy - bh * 0.2f; lf.data[idx++] = 0;

        // 食指 (landmark 5-8)
        lf.data[idx++] = cx - bw * 0.1f;  lf.data[idx++] = cy + bh * 0.2f; lf.data[idx++] = 0;
        lf.data[idx++] = cx - bw * 0.05f; lf.data[idx++] = cy - bh * 0.05f; lf.data[idx++] = 0;
        lf.data[idx++] = cx;              lf.data[idx++] = cy - bh * 0.25f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.05f; lf.data[idx++] = cy - bh * 0.35f; lf.data[idx++] = 0;

        // 中指 (landmark 9-12)
        lf.data[idx++] = cx + bw * 0.1f;  lf.data[idx++] = cy + bh * 0.2f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.1f;  lf.data[idx++] = cy - bh * 0.05f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.12f; lf.data[idx++] = cy - bh * 0.25f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.15f; lf.data[idx++] = cy - bh * 0.4f; lf.data[idx++] = 0;

        // 无名指 (landmark 13-16)
        lf.data[idx++] = cx + bw * 0.25f; lf.data[idx++] = cy + bh * 0.2f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.25f; lf.data[idx++] = cy - bh * 0.05f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.28f; lf.data[idx++] = cy - bh * 0.2f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.3f;  lf.data[idx++] = cy - bh * 0.3f; lf.data[idx++] = 0;

        // 小指 (landmark 17-20)
        lf.data[idx++] = cx + bw * 0.35f; lf.data[idx++] = cy + bh * 0.2f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.35f; lf.data[idx++] = cy;             lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.38f; lf.data[idx++] = cy - bh * 0.15f; lf.data[idx++] = 0;
        lf.data[idx++] = cx + bw * 0.4f;  lf.data[idx++] = cy - bh * 0.25f; lf.data[idx++] = 0;

    } else {
        _hasHandRecently = false;
    }

    return lf;
}

// ═════════════════════════════════════════════════════════════
//  缓冲管理
// ═════════════════════════════════════════════════════════════
void FrameProcessor::appendToBuffer(const LandmarkFrame& frame) {
    if (_bufferCount < INPUT_SEQ_LEN) {
        _buffer[_bufferCount++] = frame;
    }

    if (_bufferCount >= INPUT_SEQ_LEN) {
        runInference();
        _bufferCount = 0;
        memset(_buffer, 0, sizeof(_buffer));
    }
}

void FrameProcessor::runInference() {
    _state = ProcessorState::INFERENCE;

    if (!_inference.isReady()) {
        Serial.println("[FProc] 推理引擎未就绪");
        _state = ProcessorState::IDLE;
        return;
    }

    // 将缓冲的 LandmarkFrame 展平为 float 数组
    float flatInput[INPUT_FLAT_SIZE];
    for (int i = 0; i < INPUT_SEQ_LEN; i++) {
        memcpy(&flatInput[i * INPUT_FEATURES],
               _buffer[i].data,
               INPUT_FEATURES * sizeof(float));
    }

    // 执行推理
    _lastResult = _inference.run(flatInput);
    _hasResult = true;

    Serial.printf("[FProc] 推理: %s (%.2f), 耗时 %lu ms\n",
                  _lastResult.className,
                  _lastResult.confidence,
                  _inference.getLastInferenceTime());

    // 通知回调
    if (_inferenceCallback) {
        _inferenceCallback(_lastResult);
    }

    _state = ProcessorState::BUFFERING;
}

void FrameProcessor::loop() {
    // 定期打印状态
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 5000) {
        lastStatus = millis();
        if (_hasResult) {
            Serial.printf("[FProc] %.1f fps | buff: %d/%d | last: %s\n",
                          _currentFps, _bufferCount, INPUT_SEQ_LEN,
                          _hasResult ? _lastResult.className : "-");
        }
    }
}

// ═════════════════════════════════════════════════════════════
//  Getter / Setter
// ═════════════════════════════════════════════════════════════
ProcessorState FrameProcessor::getState() const {
    return _state;
}

float FrameProcessor::getBufferProgress() const {
    return (float)_bufferCount / INPUT_SEQ_LEN;
}

float FrameProcessor::getFps() const {
    return _currentFps;
}

InferenceResult FrameProcessor::getLastResult() const {
    return _lastResult;
}

void FrameProcessor::resetBuffer() {
    _bufferCount = 0;
    memset(_buffer, 0, sizeof(_buffer));
}

void FrameProcessor::setInferenceCallback(std::function<void(const InferenceResult&)> cb) {
    _inferenceCallback = cb;
}

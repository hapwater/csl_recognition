// frame_processor.h — 视频帧处理器
//
// 接收来自 CAM 的 JPEG 帧，完成流水线:
//   JPEG 解码 → 手部关键点提取 → 序列缓冲 → 推理触发
//
// 手部关键点检测使用轻量级模型或传统 CV 方法，
// 因为 MediaPipe Hand Landmarker 无法直接在 ESP32-S3 上运行。
// ─────────────────────────────────────────────────────────────
// ⚠  此模块中的手部关键点提取需要根据实际采用的检测方案实现。
//    以下提供两种方式的结构:
//    1. TFLite Micro 关键点模型（推荐，需额外训练）
//    2. 传统视觉方法（肤色检测 + 轮廓 -> 关键点近似）
// ─────────────────────────────────────────────────────────────

#ifndef FRAME_PROCESSOR_H_
#define FRAME_PROCESSOR_H_

#include <Arduino.h>
#include "inference.h"
#include "config.h"

// ─── 关键点结果 ────────────────────────────────────────────
struct LandmarkFrame {
    float data[INPUT_FEATURES];   // 63 维关键点 (21点 × 3坐标)
    bool hasHand;                  // 是否检测到手
};

// ─── 处理器状态 ─────────────────────────────────────────────
enum class ProcessorState {
    IDLE,               // 等待帧
    DECODING,           // 解码 JPEG
    DETECTING,          // 提取手部关键点
    BUFFERING,          // 缓冲中
    INFERENCE,          // 缓冲满 → 推理中
};

class FrameProcessor {
public:
    FrameProcessor();

    // 初始化解码器和关键点检测器
    bool begin();

    // 处理一帧 JPEG 数据（由 cam_stream 回调调用）
    void processFrame(const uint8_t* jpegData, size_t jpegLen);

    // 获取当前状态
    ProcessorState getState() const;

    // 获取缓冲进度 (0.0 ~ 1.0)
    float getBufferProgress() const;

    // 获取当前帧率统计
    float getFps() const;

    // 获取最后推理结果
    InferenceResult getLastResult() const;

    // 重置缓冲
    void resetBuffer();

    // 主循环（驱动推理结果回调）
    void loop();

    // 设置推理完成回调
    void setInferenceCallback(std::function<void(const InferenceResult&)> cb);

private:
    // ═══════════════════════════════════════════════════════
    //  内部方法
    // ═══════════════════════════════════════════════════════

    // 解码 JPEG → RGB565 到内部缓冲
    bool decodeJpeg(const uint8_t* jpegData, size_t jpegLen);

    // 从 RGB565 图像提取手部关键点
    // TODO: 根据实际采用的手部检测方案实现
    LandmarkFrame detectHandLandmarks(const uint16_t* rgb565, int w, int h);

    // 将 LandmarkFrame 加入序列缓冲，缓冲满时触发推理
    void appendToBuffer(const LandmarkFrame& frame);

    // 触发推理
    void runInference();

    // ═══════════════════════════════════════════════════════
    //  状态
    // ═══════════════════════════════════════════════════════

    ProcessorState _state;

    // 解码帧缓冲（解码后的 RGB565 图像）
    uint16_t* _decodedFrame;
    int _decodedWidth;
    int _decodedHeight;

    // 关键点序列缓冲
    LandmarkFrame _buffer[INPUT_SEQ_LEN];
    int _bufferCount;
    bool _hasHandRecently;     // 最近几帧是否检测到手

    // 推理引擎
    InferenceEngine _inference;

    // 统计
    unsigned long _lastFrameTime;
    unsigned long _frameCount;
    float _currentFps;

    // 最后结果
    InferenceResult _lastResult;
    bool _hasResult;

    // 推理完成回调
    std::function<void(const InferenceResult&)> _inferenceCallback;

    // 跳过帧计数器（降低处理帧率以节省算力）
    int _frameSkip;
    int _frameSkipCounter;
};

#endif  // FRAME_PROCESSOR_H_

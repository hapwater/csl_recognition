// inference.h — TFLite Micro 推理引擎封装
//
// 将 63 维手部关键点序列送入 INT8 量化 CNN 模型推理，
// 返回类别 argmax 和置信度。

#ifndef INFERENCE_H_
#define INFERENCE_H_

#include <Arduino.h>

// 推理结果
struct InferenceResult {
    int classIndex;        // 类别索引（argmax）
    float confidence;      // softmax 最大值（置信度）
    const char* className; // 类别名称（指向 classes.h 中的字符串）
    bool valid;            // 推理是否成功
};

class InferenceEngine {
public:
    InferenceEngine();

    // 初始化 TFLite Micro 解释器 + 加载模型
    // Returns: true 表示初始化成功
    bool begin();

    // 执行一次推理
    // input: 长度为 INPUT_FLAT_SIZE 的 float 数组（30帧×63关键点）
    // Returns: 推理结果
    InferenceResult run(const float* input);

    // 是否已初始化
    bool isReady() const;

    // 获取上次推理耗时（毫秒）
    unsigned long getLastInferenceTime() const;

private:
    bool _ready;
    unsigned long _lastInferenceTime;

    // 量化/反量化
    float _inputScale;
    int _inputZeroPoint;
    float _outputScale;
    int _outputZeroPoint;

    // TFLite Micro 对象（实现文件中定义）
    class Impl;
    Impl* _impl;
};

#endif  // INFERENCE_H_

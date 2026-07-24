// inference.cpp — TFLite Micro 推理引擎实现

#include "inference.h"
#include "model_data.h"     // g_model_data, g_model_data_len
#include "classes.h"        // CLASS_NAMES, NUM_CLASSES
#include "config.h"         // TENSOR_ARENA_SIZE, INPUT_FLAT_SIZE

// ── TFLite Micro 头文件 ─────────────────────────────────────
#include <TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

// ── TFLite Micro 内部实现（PIMPL 模式隐藏细节） ─────────────
struct InferenceEngine::Impl {
    tflite::AllOpsResolver resolver;
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;

    // 张量内存池（置于 PSRAM，如果可用）
    __attribute__((aligned(16))) uint8_t tensorArena[TENSOR_ARENA_SIZE];

    ~Impl() {
        if (interpreter) {
            delete interpreter;
            interpreter = nullptr;
        }
    }
};

// ── 实现 ─────────────────────────────────────────────────────
InferenceEngine::InferenceEngine()
    : _ready(false)
    , _lastInferenceTime(0)
    , _inputScale(1.0f)
    , _inputZeroPoint(0)
    , _outputScale(1.0f)
    , _outputZeroPoint(0)
    , _impl(nullptr)
{
}

bool InferenceEngine::begin() {
    _impl = new Impl();
    if (!_impl) {
        Serial.println("[Infer] 内存分配失败");
        return false;
    }

    // 加载模型
    _impl->model = tflite::GetModel(g_model_data);
    if (_impl->model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[Infer] 模型版本不匹配 (schema %d, 预期 %d)\n",
                      _impl->model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    // 创建解释器
    _impl->interpreter = new tflite::MicroInterpreter(
        _impl->model,
        _impl->resolver,
        _impl->tensorArena,
        TENSOR_ARENA_SIZE
    );

    if (!_impl->interpreter) {
        Serial.println("[Infer] 解释器创建失败");
        return false;
    }

    // 分配张量
    TfLiteStatus status = _impl->interpreter->AllocateTensors();
    if (status != kTfLiteOk) {
        Serial.println("[Infer] 张量分配失败");
        return false;
    }

    // 读取输入张量的量化参数
    TfLiteTensor* input = _impl->interpreter->input(0);
    _inputScale = input->params.scale;
    _inputZeroPoint = input->params.zero_point;

    // 读取输出张量的量化参数
    TfLiteTensor* output = _impl->interpreter->output(0);
    _outputScale = output->params.scale;
    _outputZeroPoint = output->params.zero_point;

    // 验证输入形状
    if (input->dims->size != 2 ||        // [1, 1890]
        input->dims->data[1] != INPUT_FLAT_SIZE) {
        Serial.printf("[Infer] 输入形状不匹配: 期望 [1,%d], 实际 [",
                      INPUT_FLAT_SIZE);
        for (int i = 0; i < input->dims->size; i++) {
            Serial.printf("%d ", input->dims->data[i]);
        }
        Serial.println("]");
        return false;
    }

    // 验证输出形状
    if (output->dims->size != 2 ||
        output->dims->data[1] != NUM_CLASSES) {
        Serial.printf("[Infer] 输出形状不匹配: 期望 [1,%d]\n", NUM_CLASSES);
        return false;
    }

    Serial.printf("[Infer] 模型加载成功 (%d 类, arena=%d KB)\n",
                  NUM_CLASSES, TENSOR_ARENA_SIZE / 1024);
    Serial.printf("  Input:  scale=%f, zero_point=%d\n", _inputScale, _inputZeroPoint);
    Serial.printf("  Output: scale=%f, zero_point=%d\n", _outputScale, _outputZeroPoint);

    _ready = true;
    return true;
}

InferenceResult InferenceEngine::run(const float* input) {
    InferenceResult result;
    result.classIndex = -1;
    result.confidence = 0.0f;
    result.className = "?";
    result.valid = false;

    if (!_ready || !_impl || !_impl->interpreter) {
        Serial.println("[Infer] 引擎未就绪");
        return result;
    }

    unsigned long startTime = millis();

    // ── 设置输入张量 ──
    TfLiteTensor* inputTensor = _impl->interpreter->input(0);
    int8_t* inputData = inputTensor->data.int8;

    // 浮点数 → INT8 量化
    for (int i = 0; i < INPUT_FLAT_SIZE; i++) {
        float q_val = input[i] / _inputScale + _inputZeroPoint;
        inputData[i] = static_cast<int8_t>(constrain(q_val, -128.0f, 127.0f));
    }

    // ── 执行推理 ──
    TfLiteStatus status = _impl->interpreter->Invoke();
    if (status != kTfLiteOk) {
        Serial.println("[Infer] 推理失败");
        return result;
    }

    // ── 读取输出 ──
    TfLiteTensor* outputTensor = _impl->interpreter->output(0);
    int8_t* outputData = outputTensor->data.int8;

    // INT8 → 浮点反量化 + softmax 等效 argmax
    float maxVal = -INFINITY;
    int maxIdx = -1;

    for (int i = 0; i < NUM_CLASSES; i++) {
        float deqVal = (outputData[i] - _outputZeroPoint) * _outputScale;
        if (deqVal > maxVal) {
            maxVal = deqVal;
            maxIdx = i;
        }
    }

    // 人为约束置信度到 [0, 1]
    float confidence = constrain(maxVal, 0.0f, 1.0f);
    if (confidence < 0.3f) {
        maxIdx = -1;
    }

    _lastInferenceTime = millis() - startTime;

    result.classIndex = maxIdx;
    result.confidence = confidence;
    result.className = (maxIdx >= 0 && maxIdx < NUM_CLASSES) ? CLASS_NAMES[maxIdx] : "?";
    result.valid = (maxIdx >= 0);

    Serial.printf("[Infer] %s (%.2f) — %lu ms\n",
                  result.className, result.confidence, _lastInferenceTime);

    return result;
}

bool InferenceEngine::isReady() const {
    return _ready;
}

unsigned long InferenceEngine::getLastInferenceTime() const {
    return _lastInferenceTime;
}

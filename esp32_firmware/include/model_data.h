// model_data.h — INT8 量化 CNN 模型数据
//
// 本文件由 quantize_model.py 自动生成。
// 使用前请先训练模型并运行量化脚本：
//
//   python train_model.py --model cnn
//   python quantize_model.py
//   python scripts/copy_model_to_firmware.py
//
// ────────────────────────────────────────────────────────────
// ⚠  警告：这个占位文件无法用于实际推理！
//    编译前请先运行上面的命令生成真正的模型数据。
//    看到此错误说明模型尚未生成，这是正常的。
// ────────────────────────────────────────────────────────────

#ifndef MODEL_DATA_H_
#define MODEL_DATA_H_

#include <cstdint>

// 取消下面这行的注释来抑制编译错误 —— 但别忘了生成真实的模型！
#error "model_data.h 尚未生成！请先运行: python scripts/copy_model_to_firmware.py"

// 生成后的模型数据会长这样（示例，不会实际编译通过）：
//
// const unsigned char g_model_data[] = {
//   0x08, 0x00, 0x00, 0x00, ...
// };
//
// const unsigned int g_model_data_len = 12345;

#endif  // MODEL_DATA_H_

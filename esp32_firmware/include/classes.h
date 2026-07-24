// classes.h — 手语类别名称列表
//
// 自动生成方式:
//   python scripts/copy_model_to_firmware.py
// 它会将 outputs/classes.txt 转换为本文件。
//
// 如果手动修改，请保持与训练时 classes.txt 的顺序一致。

#ifndef CLASSES_H_
#define CLASSES_H_

// 类别数量（必须与训练时一致）
constexpr int NUM_CLASSES = 3;

// 类别名称（UTF-8 编码，索引与模型输出 argmax 对应）
const char* CLASS_NAMES[NUM_CLASSES] = {
    "你好",
    "我",
    "谢谢"
};

#endif  // CLASSES_H_

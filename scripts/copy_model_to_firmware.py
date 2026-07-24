"""
将训练好的模型复制到 ESP32 固件工程中。

流程:
  1. 运行 quantize_model.py 生成 INT8 TFLite 模型和 C 头文件
  2. 将 model_data.h 复制到 esp32_firmware/include/
  3. 将 classes.txt 转换为 classes.h 并复制到 esp32_firmware/include/

用法:
  python scripts/copy_model_to_firmware.py
  python scripts/copy_model_to_firmware.py --model outputs/model.h5 --validate

依赖:
  - 已训练好的模型文件 (outputs/model.h5)
  - TensorFlow 已安装
"""

import argparse
import os
import sys
from pathlib import Path

# ── 项目根目录 ───────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).resolve().parent.parent
os.chdir(str(PROJECT_ROOT))


def generate_model_header(model_path: Path, validate: bool = False) -> Path:
    """运行 quantize_model.py 生成模型 C 头文件，返回路径。"""
    sys.path.insert(0, str(PROJECT_ROOT))

    # 动态导入并执行量化脚本
    import importlib.util
    spec = importlib.util.spec_from_file_location("quantize_model",
                                                   str(PROJECT_ROOT / "quantize_model.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    # 调用量化流程
    from quantize_model import main as quantize_main
    # 模拟命令行参数
    sys.argv = ["quantize_model.py", "--model", str(model_path)]
    if validate:
        sys.argv.append("--validate")
    quantize_main()

    return PROJECT_ROOT / "outputs" / "model_data.h"


def generate_classes_header() -> Path:
    """从 outputs/classes.txt 生成 classes.h。"""
    classes_txt = PROJECT_ROOT / "outputs" / "classes.txt"
    if not classes_txt.exists():
        raise FileNotFoundError(f"未找到类别文件: {classes_txt}")

    with open(classes_txt, "r", encoding="utf-8") as f:
        class_names = [line.strip() for line in f if line.strip()]

    out_path = PROJECT_ROOT / "outputs" / "classes.h"
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("// classes.h — 自动生成，请勿手动修改\n")
        f.write(f"// 来源: {classes_txt}\n\n")
        f.write("#ifndef CLASSES_H_\n#define CLASSES_H_\n\n")
        f.write(f"constexpr int NUM_CLASSES = {len(class_names)};\n\n")
        f.write("const char* CLASS_NAMES[NUM_CLASSES] = {\n")
        for name in class_names:
            f.write(f'    "{name}",\n')
        f.write("};\n\n")
        f.write("#endif  // CLASSES_H_\n")

    print(f"✅ 已生成 classes.h ({len(class_names)} 类)")
    return out_path


def copy_to_firmware(src: Path, filename: str):
    """复制文件到 esp32_firmware/include/。"""
    dst = PROJECT_ROOT / "esp32_firmware" / "include" / filename
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(src.read_bytes())
    print(f"✅ 已复制: {src.name} → {dst.relative_to(PROJECT_ROOT)}")


def main():
    parser = argparse.ArgumentParser(description="复制模型到 ESP32 固件")
    parser.add_argument("--model", default="outputs/model.h5",
                        help="训练好的模型路径 (默认: outputs/model.h5)")
    parser.add_argument("--validate", action="store_true",
                        help="量化时验证 INT8 精度")
    args = parser.parse_args()

    model_path = Path(args.model)
    if not model_path.exists():
        print(f"❌ 未找到模型: {model_path}")
        print("   请先运行: python train_model.py --model cnn")
        sys.exit(1)

    print("=" * 50)
    print("  复制模型到 ESP32 固件")
    print("=" * 50)

    # Step 1: 量化模型并生成 C 头文件
    print("\n[1/3] 量化模型...")
    model_header = generate_model_header(model_path, args.validate)

    # Step 2: 生成类别头文件
    print("\n[2/3] 生成类别列表...")
    classes_header = generate_classes_header()

    # Step 3: 复制到固件工程
    print("\n[3/3] 复制到固件目录...")
    copy_to_firmware(model_header, "model_data.h")
    copy_to_firmware(classes_header, "classes.h")

    print("\n" + "=" * 50)
    print("  ✅ 完成！现在可以编译 ESP32 固件了")
    print("  ➡  PlatformIO: 打开 esp32_firmware/ 并点击 → 编译")
    print("=" * 50)


if __name__ == "__main__":
    main()

"""
MQTT 订阅者 — 接收 ESP32 发布的识别结果。

用法:
  python pc/mqtt_subscriber.py
  python pc/mqtt_subscriber.py --broker 192.168.1.100
  python pc/mqtt_subscriber.py --broker broker.emqx.io --topic csl/result

功能:
  - 订阅 MQTT topic，实时显示识别结果
  - 带时间戳的结果历史记录
  - 可选的 GUI 窗口显示
"""

import argparse
import json
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Optional

# ── 确保项目根目录在 sys.path 中 ──────────────────────────────
PROJECT_ROOT = Path(__file__).resolve().parent.parent
os.chdir(str(PROJECT_ROOT))
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))


def on_message(client, userdata, msg):
    """MQTT 消息回调 — 解析并显示识别结果。"""
    try:
        payload = msg.payload.decode("utf-8")
        data = json.loads(payload)
        word = data.get("word", "?")
        conf = data.get("confidence", 0.0)
        timestamp = data.get("timestamp", datetime.now().isoformat())
        print(f"[{timestamp}] 识别结果: {word} (置信度: {conf:.2f})")
    except json.JSONDecodeError:
        # 如果不是 JSON，直接显示文本
        print(f"[{datetime.now():%H:%M:%S}] 收到: {msg.payload.decode('utf-8', errors='replace')}")


def main():
    parser = argparse.ArgumentParser(description="CSL Recognition — MQTT 订阅者")
    parser.add_argument("--broker", default="broker.emqx.io",
                        help="MQTT 服务器地址 (默认: broker.emqx.io)")
    parser.add_argument("--port", type=int, default=1883, help="MQTT 端口 (默认: 1883)")
    parser.add_argument("--topic", default="csl/result", help="订阅主题 (默认: csl/result)")
    parser.add_argument("--client-id", default=None, help="客户端 ID (默认自动生成)")
    args = parser.parse_args()

    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        print("[错误] 需要 paho-mqtt 库: pip install paho-mqtt")
        sys.exit(1)

    client_id = args.client_id or f"csl_subscriber_{os.urandom(4).hex()}"

    client = mqtt.Client(client_id=client_id)
    client.on_message = on_message

    def on_connect(client, userdata, flags, rc, reason=None):
        if rc == 0:
            print(f"[MQTT] 已连接到 {args.broker}:{args.port}")
            client.subscribe(args.topic)
            print(f"[MQTT] 订阅主题: {args.topic}")
            print("─" * 50)
        else:
            print(f"[MQTT] 连接失败，返回码: {rc}")

    client.on_connect = on_connect

    print(f"[MQTT] 正在连接 {args.broker}:{args.port} ...")
    try:
        client.connect(args.broker, args.port, keepalive=60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[MQTT] 用户中断，退出")
        client.disconnect()
    except Exception as e:
        print(f"[MQTT] 连接错误: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()

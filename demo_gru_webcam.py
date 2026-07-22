"""
Stage 0/3: Full-pipeline webcam demo - MediaPipe HandLandmarker + GRU inference.

Two modes:
  CONTINUOUS: rolling window predicts every frame
  TRIGGER: press SPACE to toggle recording, auto-classifies when buffer full

Usage: python demo_gru_webcam.py [--mode trigger|continuous]
"""

import argparse
import os, sys
os.chdir(os.path.dirname(os.path.abspath(__file__)))

import cv2
import numpy as np
import mediapipe as mp
import tensorflow as tf
from collections import deque
from pathlib import Path
from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python.vision import (
    HandLandmarker, HandLandmarkerOptions, RunningMode,
)
from src.config import SEQ_LEN, N_LANDMARKS

HAND_CONNECTIONS = [
    (0, 1), (1, 2), (2, 3), (3, 4),
    (0, 5), (5, 6), (6, 7), (7, 8),
    (0, 9), (9, 10), (10, 11), (11, 12),
    (0, 13), (13, 14), (14, 15), (15, 16),
    (0, 17), (17, 18), (18, 19), (19, 20),
    (5, 9), (9, 13), (13, 17),
]


def draw_landmarks(frame, hand_lms):
    h, w = frame.shape[:2]
    points = [(int(lm.x * w), int(lm.y * h)) for lm in hand_lms]
    for conn in HAND_CONNECTIONS:
        cv2.line(frame, points[conn[0]], points[conn[1]], (0, 255, 0), 2)
    for px, py in points:
        cv2.circle(frame, (px, py), 3, (0, 0, 255), -1)


def extract_landmarks(hand_lms):
    coords = []
    for lm in hand_lms:
        coords.extend([lm.x, lm.y, lm.z])
    return np.array(coords, dtype=np.float32)


def load_model_and_classes():
    model_path = Path("outputs/model.h5")
    classes_path = Path("outputs/classes.txt")
    if not model_path.exists():
        raise FileNotFoundError(f"No model at {model_path}. Run train_model.py first.")
    model = tf.keras.models.load_model(model_path)
    with open(classes_path, "r", encoding="utf-8") as f:
        classes = [line.strip() for line in f]
    return model, classes


def run_continuous(model, classes):
    options = HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path="hand_landmarker.task"),
        running_mode=RunningMode.VIDEO,
        num_hands=1,
    )
    cap = cv2.VideoCapture(0)
    buffer = deque(maxlen=SEQ_LEN)
    print("CONTINUOUS mode. ESC to exit.")

    with HandLandmarker.create_from_options(options) as landmarker:
        timestamp = 0
        while cap.isOpened():
            ok, frame = cap.read()
            if not ok:
                break

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
            result = landmarker.detect_for_video(mp_image, timestamp)
            timestamp += 33

            if result.hand_landmarks:
                draw_landmarks(frame, result.hand_landmarks[0])
                buffer.append(extract_landmarks(result.hand_landmarks[0]))
            else:
                buffer.append(np.zeros(N_LANDMARKS, dtype=np.float32))

            if len(buffer) == SEQ_LEN:
                seq = np.stack(list(buffer), axis=0)[None, ...]
                pred = model.predict(seq, verbose=0)[0]
                idx = np.argmax(pred)
                conf = pred[idx]
                label = classes[idx] if conf > 0.5 else "?"
                cv2.putText(frame, f"{label} ({conf:.2f})", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

            cv2.imshow("GRU Sign Demo (Continuous)", frame)
            if cv2.waitKey(1) & 0xFF == 27:
                break

    cap.release()
    cv2.destroyAllWindows()


def run_trigger(model, classes):
    options = HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path="hand_landmarker.task"),
        running_mode=RunningMode.VIDEO,
        num_hands=1,
    )
    cap = cv2.VideoCapture(0)
    buffer = []
    recording = False
    prediction = None
    print("TRIGGER mode. SPACE to toggle recording. ESC to exit.")

    with HandLandmarker.create_from_options(options) as landmarker:
        timestamp = 0
        while cap.isOpened():
            ok, frame = cap.read()
            if not ok:
                break

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
            result = landmarker.detect_for_video(mp_image, timestamp)
            timestamp += 33

            if result.hand_landmarks:
                draw_landmarks(frame, result.hand_landmarks[0])
                coords = extract_landmarks(result.hand_landmarks[0])
            else:
                coords = np.zeros(N_LANDMARKS, dtype=np.float32)

            if recording:
                buffer.append(coords)
                cv2.putText(frame, f"REC {len(buffer)}/{SEQ_LEN}", (10, 60),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

            if prediction:
                cv2.putText(frame, prediction, (10, 100),
                            cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 255, 0), 2)

            cv2.imshow("GRU Sign Demo (Trigger)", frame)
            key = cv2.waitKey(10) & 0xFF

            if key == 27:
                break
            elif key == 32:
                recording = not recording
                if recording:
                    buffer = []
                    prediction = None
                elif len(buffer) >= SEQ_LEN:
                    seq = np.stack(buffer[:SEQ_LEN], axis=0)[None, ...]
                    pred = model.predict(seq, verbose=0)[0]
                    idx = np.argmax(pred)
                    conf = pred[idx]
                    prediction = f"{classes[idx]} ({conf:.2f})" if conf > 0.3 else "Unknown"
                    print(prediction)
                else:
                    prediction = f"Too short: {len(buffer)}/{SEQ_LEN}"

    cap.release()
    cv2.destroyAllWindows()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["trigger", "continuous"], default="trigger")
    args = parser.parse_args()

    model, classes = load_model_and_classes()
    print(f"Model loaded: {len(classes)} classes: {classes}")

    if args.mode == "trigger":
        run_trigger(model, classes)
    else:
        run_continuous(model, classes)


if __name__ == "__main__":
    main()

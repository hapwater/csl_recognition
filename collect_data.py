"""
Stage 0/1: Collect hand landmark sequences from webcam for training.
Usage: python collect_data.py --class_name "你好" --num_sequences 20

For each sequence, hold the sign gesture. Press SPACE to start/stop recording.
Data saved to data/landmarks/CLASS_NAME/*.npy (each = (SEQ_LEN, 63)).
"""

import argparse
import os, sys
os.chdir(os.path.dirname(os.path.abspath(__file__)))

import cv2
import numpy as np
import mediapipe as mp
from pathlib import Path
from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python.vision import (
    HandLandmarker, HandLandmarkerOptions, RunningMode,
)
from src.config import SEQ_LEN, N_LANDMARKS


def extract_landmarks(hand_lms):
    coords = []
    for lm in hand_lms:
        coords.extend([lm.x, lm.y, lm.z])
    return np.array(coords, dtype=np.float32)


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--class_name", required=True)
    parser.add_argument("--num_sequences", type=int, default=20)
    parser.add_argument("--fps", type=int, default=15)
    args = parser.parse_args()

    out_dir = Path("data/landmarks") / args.class_name
    out_dir.mkdir(parents=True, exist_ok=True)
    existing = len(list(out_dir.glob("*.npy")))

    options = HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path="hand_landmarker.task"),
        running_mode=RunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.5,
        min_hand_presence_confidence=0.5,
        min_tracking_confidence=0.5,
    )

    cap = cv2.VideoCapture(0)
    print(f"Collecting '{args.class_name}': {args.num_sequences} sequences")
    print("Press SPACE to start/stop recording, ESC to quit.")

    seq_idx = existing
    recording = False
    buffer = []

    with HandLandmarker.create_from_options(options) as landmarker:
        timestamp = 0
        while cap.isOpened() and seq_idx < existing + args.num_sequences:
            ok, frame = cap.read()
            if not ok:
                break

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
            result = landmarker.detect_for_video(mp_image, timestamp)
            timestamp += max(1, int(1000 / args.fps))

            if result.hand_landmarks:
                coords = extract_landmarks(result.hand_landmarks[0])
                draw_landmarks(frame, result.hand_landmarks[0])
            else:
                coords = np.zeros(N_LANDMARKS, dtype=np.float32)

            if recording:
                buffer.append(coords)
                cv2.putText(frame, f"REC {len(buffer)}/{SEQ_LEN}", (10, 60),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

                if len(buffer) >= SEQ_LEN:
                    arr = np.stack(buffer[:SEQ_LEN], axis=0)
                    np.save(out_dir / f"seq_{seq_idx:04d}.npy", arr)
                    print(f"Saved seq {seq_idx} ({arr.shape})")
                    seq_idx += 1
                    buffer = []
                    recording = False
            else:
                status = "Hand detected" if result.hand_landmarks else "No hand"
                cv2.putText(frame, f"Class: {args.class_name} | {status}", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.putText(frame, f"Collected: {seq_idx - existing}/{args.num_sequences}",
                            (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.putText(frame, "SPACE: toggle record | ESC: quit", (10, 90),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

            cv2.imshow("Data Collection", frame)
            key = cv2.waitKey(1) & 0xFF
            if key == 27:
                break
            elif key == 32:
                recording = not recording
                if recording:
                    buffer = []

    cap.release()
    cv2.destroyAllWindows()
    total = len(list(out_dir.glob("*.npy")))
    print(f"Done. '{args.class_name}' has {total} sequences in {out_dir}")


if __name__ == "__main__":
    main()

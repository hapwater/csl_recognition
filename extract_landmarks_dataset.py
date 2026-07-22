"""
Stage 1: Batch extract MediaPipe hand landmarks from NationalCSL video dataset.

Input: NationalCSL videos organized as data/national_csl/WORD_NAME/*.mp4
Output: data/landmarks/WORD_NAME/seq_XXXX.npy (each = (SEQ_LEN, 63))

Usage: python extract_landmarks_dataset.py --video_dir data/national_csl
"""

import argparse
import os, sys
os.chdir(os.path.dirname(os.path.abspath(__file__)))

import cv2
import numpy as np
import mediapipe as mp
from pathlib import Path
from tqdm import tqdm
from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python.vision import (
    HandLandmarker, HandLandmarkerOptions, RunningMode,
)
from src.config import SEQ_LEN, N_LANDMARKS


def extract_sequence(video_path, landmarker):
    """Extract a fixed-length landmark sequence via uniform frame sampling."""
    cap = cv2.VideoCapture(str(video_path))
    frames = []
    while True:
        ok, frame = cap.read()
        if not ok:
            break
        frames.append(frame)
    cap.release()

    if len(frames) < 5:
        return None

    n_frames = len(frames)
    indices = np.linspace(0, n_frames - 1, SEQ_LEN, dtype=int)

    sequence = []
    for idx in indices:
        rgb = cv2.cvtColor(frames[idx], cv2.COLOR_BGR2RGB)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
        result = landmarker.detect(mp_image)

        if result.hand_landmarks:
            lm = result.hand_landmarks[0]
            coords = []
            for pt in lm:
                coords.extend([pt.x, pt.y, pt.z])
            sequence.append(np.array(coords, dtype=np.float32))
        else:
            sequence.append(np.zeros(N_LANDMARKS, dtype=np.float32))

    return np.stack(sequence, axis=0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--video_dir", required=True)
    parser.add_argument("--output_dir", default="data/landmarks")
    args = parser.parse_args()

    video_root = Path(args.video_dir)
    out_root = Path(args.output_dir)
    out_root.mkdir(parents=True, exist_ok=True)

    options = HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path="hand_landmarker.task"),
        running_mode=RunningMode.IMAGE,
        num_hands=1,
        min_hand_detection_confidence=0.3,
        min_hand_presence_confidence=0.3,
        min_tracking_confidence=0.3,
    )

    word_dirs = sorted([d for d in video_root.iterdir() if d.is_dir()])
    print(f"Found {len(word_dirs)} word directories")

    total_sequences = 0
    with HandLandmarker.create_from_options(options) as landmarker:
        for word_dir in tqdm(word_dirs):
            word_name = word_dir.name
            out_class_dir = out_root / word_name
            out_class_dir.mkdir(exist_ok=True)

            videos = list(word_dir.glob("*.mp4")) + list(word_dir.glob("*.avi"))
            for vi, video_path in enumerate(videos):
                seq = extract_sequence(video_path, landmarker)
                if seq is not None:
                    np.save(out_class_dir / f"seq_{vi:04d}.npy", seq)
                    total_sequences += 1

    print(f"Done. Extracted {total_sequences} sequences to {out_root}")


if __name__ == "__main__":
    main()

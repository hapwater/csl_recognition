"""
Stage 0: Verify MediaPipe hand landmark extraction works with webcam.
Displays 21 hand landmarks in real-time and prints the 63-dim vector.
Exit: press ESC.
"""

import os, sys
# Ensure CWD is the script's directory so relative paths work
os.chdir(os.path.dirname(os.path.abspath(__file__)))

import cv2
import numpy as np
import mediapipe as mp
from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python.vision import (
    HandLandmarker,
    HandLandmarkerOptions,
    HandLandmarkerResult,
    RunningMode,
)

HAND_CONNECTIONS = [
    (0, 1), (1, 2), (2, 3), (3, 4),       # thumb
    (0, 5), (5, 6), (6, 7), (7, 8),        # index
    (0, 9), (9, 10), (10, 11), (11, 12),   # middle
    (0, 13), (13, 14), (14, 15), (15, 16), # ring
    (0, 17), (17, 18), (18, 19), (19, 20), # pinky
    (5, 9), (9, 13), (13, 17),              # palm
]

options = HandLandmarkerOptions(
    base_options=BaseOptions(model_asset_path="hand_landmarker.task"),
    running_mode=RunningMode.VIDEO,
    num_hands=1,
    min_hand_detection_confidence=0.5,
    min_hand_presence_confidence=0.5,
    min_tracking_confidence=0.5,
)

cap = cv2.VideoCapture(0)
print("Press ESC to exit.")

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
            for hand_lms in result.hand_landmarks:
                h, w = frame.shape[:2]
                points = [(int(lm.x * w), int(lm.y * h)) for lm in hand_lms]

                for conn in HAND_CONNECTIONS:
                    cv2.line(frame, points[conn[0]], points[conn[1]], (0, 255, 0), 2)
                for px, py in points:
                    cv2.circle(frame, (px, py), 4, (0, 0, 255), -1)

                coords = []
                for lm in hand_lms:
                    coords.extend([lm.x, lm.y, lm.z])
                arr = np.array(coords, dtype=np.float32)
                cv2.putText(frame, f"vector: (63,) -> [{arr[0]:.3f}, {arr[1]:.3f}, {arr[2]:.3f}...]",
                            (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        else:
            cv2.putText(frame, "No hand detected", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        cv2.imshow("MediaPipe Hand Landmarks", frame)
        if cv2.waitKey(1) & 0xFF == 27:
            break

cap.release()
cv2.destroyAllWindows()

"""
Project-wide configuration constants for CSL Recognition.

All tunable hyperparameters are centralized here for easy experimentation.
"""

# ──────────────────────────────────────────────
# Data Shape
# ──────────────────────────────────────────────

SEQ_LEN = 30          # Number of frames sampled per gesture sequence
                      #   - 30 frames @ ~15 fps ≈ 2 seconds per gesture
                      #   - Adjust based on gesture speed; longer = more temporal context
N_LANDMARKS = 63      # Feature dimension per frame: 21 landmarks × 3 coords (x, y, z)
                      #   - Each hand has 21 MediaPipe landmarks
                      #   - Each landmark contributes (x, y, z) → 63 floats per frame
N_CLASSES = 200       # Target vocabulary size (max number of CSL words to support)
                      #   - Actual classes depend on collected data directories

# ──────────────────────────────────────────────
# 1D CNN Architecture (ESP32-Compatible)
# ──────────────────────────────────────────────
# Recommended for ESP32 TFLite Micro deployment.
# Architecture: Conv1D × 3 → GlobalAvgPool → Dense(softmax)
# - No GRU/LSTM → no unroll issues, smaller binary
# - ~30 KB after INT8 quantization

CNN_FILTERS = [64, 64, 64]   # Number of filters for each Conv1D layer
                              #   - 3 layers: 64 → 64 → 64 channels
                              #   - Increase for more capacity, decrease for smaller model
CNN_KERNEL_SIZE = 3           # Convolution kernel size (temporal window)
                              #   - 3 means each output looks at 3 consecutive frames

# ──────────────────────────────────────────────
# GRU Architecture (PC Testing Only)
# ──────────────────────────────────────────────
# May not convert cleanly to TFLite Micro for ESP32.
# Use for higher-accuracy PC/laptop demos.

GRU_HIDDEN = 64      # GRU hidden state size
                     #   - Larger = more capacity but more parameters
GRU_UNROLL = True    # Unroll the GRU for TFLite conversion
                     #   - Required when converting GRU → TFLite
                     #   - Increases graph size but enables static shape inference

# ──────────────────────────────────────────────
# Training Hyperparameters
# ──────────────────────────────────────────────

DROPOUT = 0.3        # Dropout rate (applied after feature extraction)
                     #   - 0.3 = keep 70% of neurons; range [0.0, 1.0)
                     #   - Higher = stronger regularization
BATCH_SIZE = 32      # Training batch size
                     #   - Reduce if GPU OOM (e.g., 16 or 8)
                     #   - Increase for faster training on large datasets
EPOCHS = 50          # Maximum training epochs
                     #   - EarlyStopping will halt earlier if val_accuracy plateaus
LEARNING_RATE = 0.001  # Adam optimizer learning rate
                       #   - 0.001 is the Adam default; try 0.0005 for finer tuning

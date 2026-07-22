"""Project-wide configuration constants."""

SEQ_LEN = 30
N_LANDMARKS = 63
N_CLASSES = 200

# 1D CNN architecture (ESP32-compatible)
CNN_FILTERS = [64, 64, 64]
CNN_KERNEL_SIZE = 3

# GRU architecture (PC testing only, may not convert to TFLite Micro)
GRU_HIDDEN = 64
GRU_UNROLL = True  # required for TFLite conversion

DROPOUT = 0.3
BATCH_SIZE = 32
EPOCHS = 50
LEARNING_RATE = 0.001

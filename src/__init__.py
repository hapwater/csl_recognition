"""
src — CSL Recognition 核心模块

提供训练、量化、推理等各阶段共享的配置与工具。
"""

from src.config import (
    SEQ_LEN,
    N_LANDMARKS,
    N_CLASSES,
    CNN_FILTERS,
    CNN_KERNEL_SIZE,
    GRU_HIDDEN,
    GRU_UNROLL,
    DROPOUT,
    BATCH_SIZE,
    EPOCHS,
    LEARNING_RATE,
)

__all__ = [
    "SEQ_LEN",
    "N_LANDMARKS",
    "N_CLASSES",
    "CNN_FILTERS",
    "CNN_KERNEL_SIZE",
    "GRU_HIDDEN",
    "GRU_UNROLL",
    "DROPOUT",
    "BATCH_SIZE",
    "EPOCHS",
    "LEARNING_RATE",
]

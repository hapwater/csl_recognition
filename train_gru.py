"""
Stage 0/1: Train GRU classifier on MediaPipe hand landmark sequences.

Input: data/landmarks/CLASS_NAME/*.npy files, each shape (SEQ_LEN, 63)
Output: outputs/gru_model.h5

Usage: python train_gru.py
"""

from pathlib import Path
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models
from sklearn.model_selection import train_test_split
from src.config import SEQ_LEN, N_LANDMARKS, GRU_HIDDEN, DROPOUT, BATCH_SIZE, EPOCHS, LEARNING_RATE


def load_data(data_dir="data/landmarks"):
    """Load landmark sequences from disk. Returns X, y, classes."""
    data_root = Path(data_dir)
    if not data_root.exists():
        raise FileNotFoundError(f"No data directory: {data_root}. Run collect_data.py first.")

    classes = sorted([d.name for d in data_root.iterdir() if d.is_dir()])
    if not classes:
        raise ValueError(f"No class directories found in {data_root}")

    X, y = [], []
    for idx, cls in enumerate(classes):
        for npy_path in data_root.glob(f"{cls}/*.npy"):
            arr = np.load(npy_path)
            if arr.shape != (SEQ_LEN, N_LANDMARKS):
                print(f"WARNING: {npy_path} has shape {arr.shape}, expected ({SEQ_LEN}, {N_LANDMARKS}). Skipping.")
                continue
            X.append(arr)
            y.append(idx)

    X = np.array(X, dtype=np.float32)
    y = np.array(y, dtype=np.int32)
    print(f"Loaded {len(X)} sequences across {len(classes)} classes: {classes}")
    return X, y, classes


def build_model(n_classes):
    """Build GRU model matching Sign-Language-Advance architecture."""
    inp = layers.Input(shape=(SEQ_LEN, N_LANDMARKS))
    x = layers.Masking(mask_value=0.0)(inp)
    x = layers.GRU(GRU_HIDDEN, return_sequences=False)(x)
    x = layers.Dropout(DROPOUT)(x)
    out = layers.Dense(n_classes, activation="softmax")(x)

    model = models.Model(inp, out)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(LEARNING_RATE),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    model.summary()
    return model


def main():
    X, y, classes = load_data()
    n_classes = len(classes)

    X_train, X_val, y_train, y_val = train_test_split(
        X, y, test_size=0.2, stratify=y, random_state=42
    )

    model = build_model(n_classes)

    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_accuracy", patience=10, restore_best_weights=True
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss", factor=0.5, patience=5, min_lr=1e-6
        ),
    ]

    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=callbacks,
        verbose=1,
    )

    out_dir = Path("outputs")
    out_dir.mkdir(exist_ok=True)
    model.save(out_dir / "gru_model.h5")
    model.save(out_dir / "gru_model.keras")

    with open(out_dir / "classes.txt", "w", encoding="utf-8") as f:
        for cls in classes:
            f.write(cls + "\n")

    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"Validation accuracy: {val_acc:.4f}")
    print(f"Model saved to {out_dir}/gru_model.h5")


if __name__ == "__main__":
    main()

# ============================================================
# train_bp_model.py
# Huan luyen mo hinh du doan huyet ap (SBP/DBP) tu PPG Waveform (Red & IR)
#
# Dinh dang du lieu vao (tu ppg_sequence_data.npz):
#   red_z, ir_z : (N, 800)  - waveform da qua EMA+bandpass+z-score
# Dau ra:
#   sbp, dbp    : (N,)      - nhan gia tri huyet ap (mmHg)
#
# Kien truc: Single-Input CNN
#   Input: waveform [800, 2] (Red_Z & IR_Z)
#   Output: [SBP, DBP]
# ============================================================

import numpy as np
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, mean_squared_error, r2_score
import tensorflow as tf
from tensorflow.keras import layers, models, callbacks

# -------------------- 1. Cau hinh --------------------
DATA_PATH = './data_seq/ppg_sequence_data_1_input.npz'
MODEL_DIR = './models'
os.makedirs(MODEL_DIR, exist_ok=True)

N_POINTS = 800
RANDOM_SEED = 42
TEST_SIZE = 0.15
VAL_SIZE = 0.15   # Tinh tren phan con lai sau khi tach test

# -------------------- 2. Tai du lieu --------------------
print("Loading data...")
data = np.load(DATA_PATH)
red_z = data['red_z']    # (N, 800)
ir_z  = data['ir_z']     # (N, 800)
y_sbp = data['SBP']    # (N,)
y_dbp = data['DBP']    # (N,)

N = red_z.shape[0]
print(f"Total samples: {N}")

# Ghep 2 kenh Red & IR vao Waveform Input -> (N, 800, 2)
X_wave = np.stack([red_z, ir_z], axis=-1).astype(np.float32)

# Head Output: [SBP, DBP] -> (N, 2)
y = np.stack([y_sbp, y_dbp], axis=-1).astype(np.float32)

# -------------------- 3. Chia train/val/test --------------------
idx = np.arange(N)
idx_temp, idx_test = train_test_split(idx, test_size=TEST_SIZE, random_state=RANDOM_SEED)
idx_train, idx_val = train_test_split(idx_temp, test_size=VAL_SIZE / (1 - TEST_SIZE),
                                       random_state=RANDOM_SEED)

Xw_train, y_train = X_wave[idx_train], y[idx_train]
Xw_val,   y_val   = X_wave[idx_val],   y[idx_val]
Xw_test,  y_test  = X_wave[idx_test],  y[idx_test]

print(f"Train: {len(idx_train)}, Val: {len(idx_val)}, Test: {len(idx_test)}")

# -------------------- 4. Xay dung mo hinh Single-Input CNN --------------------
def build_model(wave_shape=(N_POINTS, 2)):
    wave_in = layers.Input(shape=wave_shape, name='waveform')
    
    x = layers.Conv1D(8, 3, activation='relu', padding='same')(wave_in)
    x = layers.MaxPooling1D(2)(x)
    
    x = layers.Conv1D(16, 3, activation='relu', padding='same')(x)
    x = layers.MaxPooling1D(2)(x)
    
    x = layers.Conv1D(32, 3, activation='relu', padding='same')(x)
    x = layers.MaxPooling1D(2)(x)
    
    x = layers.Conv1D(64, 3, activation='relu', padding='same')(x)
    x = layers.MaxPooling1D(2)(x) 
    
    x = layers.Conv1D(64, 3, activation='relu', padding='same')(x)
    x = layers.GlobalAveragePooling1D()(x)

    out = layers.Dense(32, activation='relu')(x)
    out = layers.Dense(2, name='sbp_dbp')(out)  # [sbp, dbp]

    return models.Model(inputs=wave_in, outputs=out)

model = build_model()
model.summary()

model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=1e-3),
              loss='mse',
              metrics=['mae'])

# -------------------- 5. Callbacks --------------------
callbacks_list = [
    callbacks.EarlyStopping(monitor='val_loss', patience=20, restore_best_weights=True),
    callbacks.ModelCheckpoint(filepath=os.path.join(MODEL_DIR, 'best_model.keras'),
                              monitor='val_loss', save_best_only=True),
    callbacks.ReduceLROnPlateau(monitor='val_loss', factor=0.5, patience=10, min_lr=1e-6)
]

# -------------------- 6. Huan luyen --------------------
history = model.fit(
    Xw_train, y_train,
    validation_data=(Xw_val, y_val),
    epochs=100,
    batch_size=32,
    callbacks=callbacks_list,
    verbose=1
)

# -------------------- 7. Danh gia tren tap test --------------------
best_model = tf.keras.models.load_model(os.path.join(MODEL_DIR, 'best_model.keras'))
y_pred = best_model.predict(Xw_test)

mae_sbp = mean_absolute_error(y_test[:, 0], y_pred[:, 0])
mae_dbp = mean_absolute_error(y_test[:, 1], y_pred[:, 1])
rmse_sbp = np.sqrt(mean_squared_error(y_test[:, 0], y_pred[:, 0]))
rmse_dbp = np.sqrt(mean_squared_error(y_test[:, 1], y_pred[:, 1]))
r2_sbp = r2_score(y_test[:, 0], y_pred[:, 0])
r2_dbp = r2_score(y_test[:, 1], y_pred[:, 1])

print("\n======= Test Evaluation =======")
print(f"SBP - MAE: {mae_sbp:.2f} mmHg, RMSE: {rmse_sbp:.2f} mmHg, R2: {r2_sbp:.3f}")
print(f"DBP - MAE: {mae_dbp:.2f} mmHg, RMSE: {rmse_dbp:.2f} mmHg, R2: {r2_dbp:.3f}")

# Ve do thi Loss
plt.figure(figsize=(10, 4))
plt.plot(history.history['loss'], label='Train Loss')
plt.plot(history.history['val_loss'], label='Val Loss')
plt.xlabel('Epoch')
plt.ylabel('MSE Loss')
plt.legend()
plt.title('Training History')
plt.savefig(os.path.join(MODEL_DIR, 'training_loss.png'))
plt.close()

# -------------------- 8. Xuat TFLite (1 Input) --------------------
def representative_dataset():
    num_samples = min(100, len(Xw_train))
    indices = np.random.choice(len(Xw_train), num_samples, replace=False)
    for i in indices:
        # Lay 1 sample shape (1, 800, 2)
        yield [Xw_train[i:i+1].astype(np.float32)]

converter = tf.lite.TFLiteConverter.from_keras_model(best_model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

try:
    tflite_model = converter.convert()
    tflite_path = os.path.join(MODEL_DIR, 'bp_model_int8.tflite')
    with open(tflite_path, 'wb') as f:
        f.write(tflite_model)
    print(f"\nDa xuat TFLite int8: {tflite_path}")
except Exception as e:
    print(f"\nLoi khi luong tu hoa int8 ({e}), thu float16...")
    converter = tf.lite.TFLiteConverter.from_keras_model(best_model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.target_spec.supported_types = [tf.float16]
    tflite_model = converter.convert()
    tflite_path = os.path.join(MODEL_DIR, 'bp_model_float16.tflite')
    with open(tflite_path, 'wb') as f:
        f.write(tflite_model)
    print(f"Da xuat TFLite float16: {tflite_path}")

# Ban float32 (khong luong tu hoa) - de doi chieu debug
converter = tf.lite.TFLiteConverter.from_keras_model(best_model)
tflite_fp32 = converter.convert()
tflite_fp32_path = os.path.join(MODEL_DIR, 'bp_model_fp32.tflite')
with open(tflite_fp32_path, 'wb') as f:
    f.write(tflite_fp32)
print(f"Da xuat TFLite float32: {tflite_fp32_path}")

# In dung luong file
for fname in ['bp_model_int8.tflite', 'bp_model_float16.tflite', 'bp_model_fp32.tflite']:
    path = os.path.join(MODEL_DIR, fname)
    if os.path.exists(path):
        size = os.path.getsize(path) / 1024
        print(f"{fname}: {size:.2f} KB")

# In Quantization parameters de dung cho C/C++ Firmware
int8_path = os.path.join(MODEL_DIR, 'bp_model_int8.tflite')
if os.path.exists(int8_path):
    interp = tf.lite.Interpreter(model_path=int8_path)
    interp.allocate_tensors()
    print("\n======= Quantization params (Dung cho Firmware C/C++) =======")
    for t in interp.get_input_details():
        print(f"INPUT  '{t['name']}': shape={t['shape']}, scale={t['quantization'][0]:.8f}, "
              f"zero_point={t['quantization'][1]}")
    for t in interp.get_output_details():
        print(f"OUTPUT '{t['name']}': shape={t['shape']}, scale={t['quantization'][0]:.8f}, "
              f"zero_point={t['quantization'][1]}")

print("\nHoan tat huan luyen va xuat mo hinh.")
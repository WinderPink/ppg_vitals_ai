import pandas as pd
import numpy as np
from pathlib import Path

# 1. Định nghĩa thư mục chứa dữ liệu (vì script đang đặt ở thư mục gốc)
data_dir = Path('data_seq')

# 2. Đọc file CSV từ thư mục data_seq
csv_file = data_dir / 'ppg_sequence_data_1_input.csv'
df = pd.read_csv(csv_file)

# 3. Tách các cột (dùng lọc chuỗi để linh hoạt)
red_data = df[[col for col in df.columns if col.startswith('red_z_')]].values
ir_data  = df[[col for col in df.columns if col.startswith('ir_z_')]].values
sbp_data = df['SBP'].values
dbp_data = df['DBP'].values

# 4. Lưu file .npz vào thư mục data_seq
npz_file = data_dir / 'ppg_sequence_data_1_input.npz'
np.savez_compressed(npz_file, red_z=red_data, ir_z=ir_data, SBP=sbp_data, DBP=dbp_data)

print(f"Đã lưu file {npz_file} thành công!")
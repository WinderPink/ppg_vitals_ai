# Thiết bị đeo tay theo dõi sức khỏe (Mini Wearable Health Device)

Thiết bị đeo tay đo và theo dõi các chỉ số sức khỏe (nhịp tim, SpO2, huyết áp) theo thời gian thực, sử dụng cảm biến quang học PPG kết hợp mô hình AI nhúng (Edge AI) để ước lượng huyết áp, truyền dữ liệu qua Bluetooth Low Energy (BLE) tới gateway để hiển thị trên màn hình OLED.

## Demo & Kết quả thực tế

### Video demo:

https://github.com/user-attachments/assets/2fe42cf8-3665-4bd7-a83b-bc7978a10b6e

### Hình ảnh kết quả:

<p align="center">
  <img src="https://github.com/user-attachments/assets/7a502d52-63d0-4116-ae8b-c4cc1d9189ab" width="45%" alt="Gateway chờ kết nối BLE" />
  &nbsp;&nbsp;
  <img src="https://github.com/user-attachments/assets/9a0386de-f31d-40cf-a4e0-90d6f73eef1b" width="45%" alt="Gateway kết nối BLE thành công" />
</p>
<p align="center">
  <em>Hình 1: Gateway ở trạng thái chờ kết nối BLE (trái) và sau khi kết nối thành công với Node (phải)</em>
</p>

<br />

<p align="center">
  <img src="https://github.com/user-attachments/assets/493afb6e-d983-4ec0-bfaa-410458acb905" width="85%" alt="Dữ liệu thu được từ Node - Ảnh 1" />
</p>
<p align="center">
  <em>Hình 2: Gateway hiển thị dữ liệu thu nhận được từ Node trên màn OLED</em>
</p>

<br />

<p align="center">
  <img src="https://github.com/user-attachments/assets/65f89f24-299f-4787-a3fd-0ec95a928429" width="85%" alt="Dữ liệu thu được từ Node - Ảnh 2" />
</p>
<p align="center">
  <em>Hình 3: Chi tiết chuỗi dữ liệu tín hiệu truyền từ Node về Gateway hiển thị trên terminal</em>
</p>

<br />

<p align="center">
  <img src="https://github.com/user-attachments/assets/64108960-a999-4479-8c9c-081a2469f6bc" width="85%" alt="Dữ liệu thu được từ Node - Ảnh 3" />
</p>
<p align="center">
  <em>Hình 4: Kết quả xử lý và hiển thị thông số trên terminal của Gateway</em>
</p>

## Mục lục

- [Tổng quan](#tổng-quan)
- [Kiến trúc hệ thống](#kiến-trúc-hệ-thống)
- [Phần cứng sử dụng](#phần-cứng-sử-dụng)
- [Công nghệ / Công cụ](#công-nghệ--công-cụ)
- [Tính năng chính](#tính-năng-chính)
- [Kết quả đánh giá](#kết-quả-đánh-giá)
- [Hướng dẫn build & chạy](#hướng-dẫn-build--chạy)
- [Thành viên nhóm](#thành-viên-nhóm)
- [Hướng phát triển](#hướng-phát-triển)

## Tổng quan

Dự án xây dựng một hệ thống thiết bị đeo tay (wearable) gồm hai khối chính:

- **Node cảm biến (EFR32xG26)**: thu tín hiệu PPG từ cảm biến MAX30102 qua giao tiếp I2C, lọc/chuẩn hóa tín hiệu, chạy mô hình AI ước lượng huyết áp (SBP/DBP) ngay trên chip (TinyML), sau đó truyền kết quả qua BLE.
- **Gateway (BGM220P)**: nhận dữ liệu qua BLE từ node và hiển thị kết quả trực tiếp lên màn hình OLED SSD1306.

## Kiến trúc hệ thống

<img width="933" height="612" alt="image" src="https://github.com/user-attachments/assets/520bc814-92f6-4cb6-9f1a-ff94d84faa3e" />

Node được tổ chức theo kiến trúc FreeRTOS với 4 tác vụ (task) hoạt động song song, giao tiếp qua semaphore/queue/priority:

1. **PPG task** — đọc dữ liệu thô từ MAX30102 (ưu tiên cao nhất)
2. **Filter task** — lọc EMA, bandpass, chuẩn hóa z-score
3. **AI task** — tính toán HR/SpO2, chạy mô hình TFLite Micro ước lượng huyết áp (SYS/DIA)
4. **BLE task** — đóng gói và truyền dữ liệu qua Bluetooth Low Energy

## Phần cứng sử dụng

| Thành phần | Vai trò |
|---|---|
| MAX30102 | Cảm biến PPG (đo tín hiệu quang để tính HR, SpO2, huyết áp) |
| EFR32xG26 | Vi điều khiển node, chạy AI on-device (TinyML) |
| BGM220P | Vi điều khiển gateway, nhận dữ liệu BLE |
| OLED SSD1306 | Hiển thị kết quả đo trên gateway |

## Công nghệ / Công cụ

- **Simplicity Studio 6** — phát triển firmware cho EFR32/BGM220 (FreeRTOS, BGAPI/BLE, ML Model component)
- **Python** — chuyển đổi dữ liệu sang định dạng `.npz`, xây dựng và huấn luyện mô hình AI (TensorFlow/Keras)
- **MATLAB** — tổng hợp, định dạng dữ liệu công khai thành file `.csv` theo chuẩn thống nhất
- **TensorFlow Lite Micro** — lượng tử hóa (int8) và triển khai mô hình AI trên vi điều khiển

## Tính năng chính

- Đo và hiển thị nhịp tim (HR), SpO2, huyết áp (SYS/DIA) theo thời gian thực
- Xử lý tín hiệu PPG on-device: lọc EMA, bandpass, chuẩn hóa z-score
- Mô hình 1D CNN single-input xử lý chuỗi tín hiệu PPG (800 mẫu) ước lượng trực tiếp huyết áp (SBP/DBP), xuất mô hình TFLite INT8 tối ưu tài nguyên chạy trực tiếp trên vi điều khiển.
- Truyền dữ liệu qua BLE 5.4 có bảo mật tới gateway, hiển thị kết quả lên màn hình OLED

## Kết quả đánh giá

Đánh giá trên bộ dữ liệu 40 lần đo, so sánh với thiết bị chuẩn:

| Tham số | MAE | MRE |
|---|---|---|
| HR (nhịp tim) | 1.60 bpm | 1.94% |
| SYS (huyết áp tâm thu) | 8.28 mmHg | 7.70% |
| DIA (huyết áp tâm trương) | 3.48 mmHg | 5.17% |

> HR và DIA đạt độ chính xác tốt, phù hợp với thiết bị theo dõi sức khỏe cá nhân. SYS còn sai số cao hơn ngưỡng khuyến nghị và là hướng cần cải thiện.

## Hướng dẫn build & chạy

1. Tải và cài đặt [Simplicity Studio 6](https://www.silabs.com/developers/simplicity-studio).
2. Clone repo về máy:
   ```bash
   git clone https://github.com/WinderPink/ppg_vitals_ai.git
   ```
3. Mở Simplicity Studio, import project từ thư mục `node/` (firmware cho EFR32xG26) và thư mục `gateway/` (firmware cho BGM220P).
4. Với mỗi project, chạy **Force Generation** để sinh lại mã nguồn cấu hình (auto-generated code).
5. Mở project trong VS Code.
6. Build và nạp firmware:
   - Thư mục `node/` → nạp xuống board **node (EFR32xG26)**
   - Thư mục `gateway/` → nạp xuống board **gateway (BGM220P)**

## Thành viên nhóm

| Thành viên | Vai trò |
|---|---|
| Phong | Thiết kế phần cứng, lập trình I2C (node & gateway), thiết kế bộ lọc tín hiệu (EMA, bandpass, chuẩn hóa z-score) |
| Quang | FreeRTOS và tích hợp tác vụ (PPG, Filter, AI, BLE) |
| Tuấn | Huấn luyện và tích hợp mô hình AI |
| Trung | Lập trình Bluetooth Low Energy |

## Hướng phát triển

- Kết nối gateway lên PC/điện thoại/nền tảng Cloud để lưu trữ và theo dõi dữ liệu dài hạn
- Mở rộng và làm giàu tập dữ liệu huấn luyện để cải thiện độ chính xác ước lượng huyết áp (đặc biệt SYS)
- Tối ưu tiêu thụ năng lượng để kéo dài thời gian sử dụng pin
- Chuyển từ mạch hàn thủ công (perfboard) sang thiết kế PCB hoàn chỉnh

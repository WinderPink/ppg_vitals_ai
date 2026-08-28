/***************************************************************************//**
 * @file app_ead_scanner.h
 * @brief Module quét BLE advertisement và giải mã Encrypted Advertising Data
 *        (EAD), kèm quy trình bonding/pairing với thiết bị advertiser.
 *
 * Tách ra từ app.c gốc (phần trước đây thuộc app_encryption_advertising.c
 * đã được gộp vào). Bao gồm cả các FIX cho lỗi treo bonding (xem comment
 * trong app_ead_scanner.c).
 ******************************************************************************/
#ifndef APP_EAD_SCANNER_H
#define APP_EAD_SCANNER_H

/**************************************************************************//**
 * Khởi tạo module quét/giải mã EAD: khởi động timer đọc advertisement định
 * kỳ. Gọi một lần trong app_init().
 *
 * Việc cấu hình ngăn xếp Bluetooth (scanner, security manager...) được thực
 * hiện trong sl_bt_on_event() khi nhận sự kiện sl_bt_evt_system_boot_id,
 * đúng như hành vi gốc.
 *****************************************************************************/
void app_ead_scanner_init(void);

#endif // APP_EAD_SCANNER_H

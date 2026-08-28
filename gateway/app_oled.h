/***************************************************************************//**
 * @file app_oled.h
 * @brief Module điều khiển màn hình OLED SSD1306 (I2C0, SCL=PD02, SDA=PD03).
 *
 * Tách ra từ app.c gốc (phần trước đây thuộc app_i2c.c đã được gộp vào).
 ******************************************************************************/
#ifndef APP_OLED_H
#define APP_OLED_H

#include "app_health_data.h"

/**************************************************************************//**
 * Khởi tạo màn hình OLED. Gọi một lần trong app_init().
 * Nếu init thất bại, mọi lệnh ghi OLED sau đó sẽ tự động bị bỏ qua.
 *****************************************************************************/
void app_oled_init(void);

/**************************************************************************//**
 * Xử lý định kỳ cho OLED (heartbeat counter). Gọi trong app_process_action().
 *****************************************************************************/
void app_oled_process(void);

/**************************************************************************//**
 * Hiển thị chuỗi trạng thái lên dòng STATUS của OLED.
 * @param status Chuỗi trạng thái (PHẢI viết HOA, xem ghi chú trong app_oled.c).
 *****************************************************************************/
void oled_show_status(const char *status);

/**************************************************************************//**
 * Hiển thị dữ liệu sức khỏe (nhịp tim, huyết áp, SpO2) đã giải mã lên OLED.
 * @param data Con trỏ tới dữ liệu sức khỏe cần hiển thị.
 *****************************************************************************/
void oled_display_health_data(health_data_t *data);

#endif // APP_OLED_H

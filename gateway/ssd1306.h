#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_i2cspm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_I2C_ADDR   0x3C   // địa chỉ mặc định module
#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT     64
#define SSD1306_PAGES      (SSD1306_HEIGHT / 8)   // = 8 page, mỗi page cao 8 pixel

/**
 * @brief Khởi tạo màn hình (chạy chuỗi lệnh init chuẩn của SSD1306)
 */
bool ssd1306_init(sl_i2cspm_t *i2cspm);

/**
 * @brief Xoá toàn bộ màn hình về màu đen
 */
void ssd1306_clear(sl_i2cspm_t *i2cspm);

/**
 * @brief Di chuyển con trỏ ghi tới 1 vị trí cụ thể
 * @param page  0..7 (mỗi page cao 8px, tương đương 1 "dòng" text với font 5x7)
 * @param col   0..127 (cột pixel bắt đầu)
 */
void ssd1306_set_cursor(sl_i2cspm_t *i2cspm, uint8_t page, uint8_t col);

/**
 * @brief In 1 chuỗi ký tự tại vị trí con trỏ hiện tại (font 5x7, hỗ trợ tập
 *        ký tự giới hạn — xem danh sách trong ssd1306_font.c). Ký tự không
 *        hỗ trợ sẽ được vẽ thành 1 ô trắng.
 */
void ssd1306_write_string(sl_i2cspm_t *i2cspm, const char *str);

/**
 * @brief Tiện ích: xoá 1 dòng (1 page) rồi in chuỗi mới vào đó — dùng để
 *        cập nhật số liệu liên tục mà không bị chồng chữ cũ/mới.
 */
void ssd1306_print_line(sl_i2cspm_t *i2cspm, uint8_t page, const char *str);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H
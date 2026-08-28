#ifndef SSD1306_FONT_H
#define SSD1306_FONT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tra bảng font cho 1 ký tự, trả về 5 byte cột pixel
 * @return true nếu tìm thấy ký tự trong bảng, false nếu không (vẽ ô trắng thay thế)
 */
bool ssd1306_font_lookup(char c, uint8_t out_cols[5]);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_FONT_H
/***************************************************************************//**
 * @file app.c
 * @brief Application top-level — EAD Scanner (Project 1) + SSD1306 OLED (Project 2)
 *
 * File này trước đây chứa toàn bộ logic gộp của app_encryption_advertising.c
 * (quét/giải mã Encrypted Advertising Data) và app_i2c.c (điều khiển OLED
 * SSD1306), nay đã được TÁCH NHỎ ra 2 module riêng để dễ đọc/bảo trì:
 *
 *   - app_ead_scanner.c/.h : quét BLE, bonding/pairing, giải mã EAD
 *                            (sl_bt_on_event, sl_button_on_change...)
 *   - app_oled.c/.h        : điều khiển màn hình OLED SSD1306
 *
 * app.c chỉ còn giữ vai trò "điều phối" (orchestration): khởi tạo và gọi
 * các module con từ app_init() / app_process_action(), đúng như những hàm
 * này đã được main.c gọi trong bản gốc. Toàn bộ logic nghiệp vụ và các FIX
 * (xem app_ead_scanner.c) được giữ nguyên, không thay đổi hành vi.
 ******************************************************************************/
#include "app.h"
#include "app_oled.h"
#include "app_ead_scanner.h"

// -----------------------------------------------------------------------------
// Application Init
// -----------------------------------------------------------------------------
void app_init(void)
{
  app_oled_init();
  app_ead_scanner_init();
}

// -----------------------------------------------------------------------------
// Application Process Action
// -----------------------------------------------------------------------------
void app_process_action(void)
{
  if (app_is_process_required()) {
  }

  app_oled_process();
}

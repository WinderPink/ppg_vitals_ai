/***************************************************************************//**
 * @file app_oled.c
 * @brief Cài đặt module hiển thị OLED SSD1306.
 *
 * Được tách ra từ app.c gốc (phần OLED, trước đây thuộc app_i2c.c đã được
 * gộp chung vào app.c). Toàn bộ logic giữ nguyên như bản gốc, chỉ thay đổi
 * cách tổ chức file.
 ******************************************************************************/
#include "app_oled.h"

#include "sl_sleeptimer.h"
#include "sl_i2cspm_instances.h"
#include "ssd1306.h"
#include "app_log.h"

#include <stdio.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// OLED line layout (SSD1306 128x64 -> 8 page/dong, xac nhan tu ssd1306.h
// SSD1306_PAGES=8):
//   Dong 0: tieu de (tinh, ghi 1 lan luc init)
//   Dong 1: trang thai (WAITING / KEY UPDATED / DECRYPT FAILED / OK)
//   Dong 2: Heart Rate
//   Dong 3: Huyet ap (Body Temperature line cu, thay bang BP)
//   Dong 4: SpO2
//   Dong 5: heartbeat counter (debug xac nhan I2C/OLED khong bi treo)
//   Dong 6-7: chua dung, de trong cho mo rong sau nay
//
// QUAN TRONG: ssd1306_font.c chi co glyph cho chu HOA (A-Z), so 0-9 va cac
// ky tu ' ' '.' ':' '%' '-' '/'. Chu thuong hoac ky tu khac se bi ve thanh
// 1 khoi dac (xem ssd1306_font_lookup tra ve false -> memset 0x7F).
// => Moi chuoi truyen cho ssd1306_print_line() phia duoi PHAI viet HOA.
// Moi dong toi da ~21 ky tu (128px / 6px moi glyph); cac chuoi ben duoi deu
// duoi gioi han nay.
// -----------------------------------------------------------------------------
#define OLED_LINE_TITLE   0
#define OLED_LINE_STATUS  1
#define OLED_LINE_HR      2
#define OLED_LINE_BP      3   // Thay cho OLED_LINE_TEMP
#define OLED_LINE_SPO2    4
#define OLED_LINE_COUNT   5

// -----------------------------------------------------------------------------
// OLED globals (từ app_i2c.c)
// -----------------------------------------------------------------------------
static sl_sleeptimer_timer_handle_t update_timer;
static volatile bool update_due = false;
static uint16_t counter = 0;
static bool oled_ready = false; // false nếu init OLED thất bại -> bỏ qua các lần ghi sau

// chỉ set cờ trong callback, xử lý thật (ghi I2C) ở main loop
static void update_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  update_due = true;
}

// -----------------------------------------------------------------------------
// OLED helpers
// -----------------------------------------------------------------------------
void oled_show_status(const char *status)
{
  if (!oled_ready) {
    return;
  }
  ssd1306_print_line(sl_i2cspm_oled, OLED_LINE_STATUS, status);
}

void oled_display_health_data(health_data_t *data)
{
  char line[17];

  if (!oled_ready) {
    return;
  }

  oled_show_status("OK");

  // Dòng 2: Nhịp tim
  snprintf(line, sizeof(line), "HR:%3u BPM", data->heart_rate);
  ssd1306_print_line(sl_i2cspm_oled, OLED_LINE_HR, line);

  // Dòng 3: Huyết áp (Tâm thu / Tâm trương)
  snprintf(line, sizeof(line), "BP:%3u/%3u", data->systolic, data->diastolic);
  ssd1306_print_line(sl_i2cspm_oled, OLED_LINE_BP, line);

  // Dòng 4: SpO2
  snprintf(line, sizeof(line), "SPO2:%3u %%", data->spo2);
  ssd1306_print_line(sl_i2cspm_oled, OLED_LINE_SPO2, line);
}

// -----------------------------------------------------------------------------
// Init / process action
// -----------------------------------------------------------------------------
void app_oled_init(void)
{
  if (ssd1306_init(sl_i2cspm_oled)) {
    oled_ready = true;
    app_log_info("OLED: khoi tao thanh cong\r\n");
    ssd1306_print_line(sl_i2cspm_oled, OLED_LINE_TITLE, "EAD HEALTH MON.");
    oled_show_status("WAITING DATA...");
  } else {
    oled_ready = false;
    app_log_error("OLED: khong tim thay man hinh / loi I2C\r\n");
  }
}

void app_oled_process(void)
{
  // Heartbeat counter — xác nhận OLED/I2C không bị treo.
  // Panel 128x64 có 8 dòng, dòng 5 chưa dùng bởi dữ liệu health nên in
  // counter ở đó, không đè lên HR/BP/SPO2/STATUS.
  if (update_due) {
    update_due = false;
    counter++;
    if (oled_ready) {
      char line[17];
      snprintf(line, sizeof(line), "COUNT:%05u", counter);
      ssd1306_print_line(sl_i2cspm_oled, OLED_LINE_COUNT, line);
    }
  }
}

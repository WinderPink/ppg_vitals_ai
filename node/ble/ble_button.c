// app_button.c - Xử lý nút bấm
#include "sl_simple_button_instances.h"
#include "sl_bt_api.h"
#include "internal.h"

uint8_t btn_count = 0;

void sl_button_on_change(const sl_button_t *handle)
{
  if (handle == &sl_button_btn0 && sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    sl_bt_external_signal(CONFIRM_BTN);
  }
}

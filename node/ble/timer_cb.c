// app_timer_cb.c - Sleeptimer callbacks (chuyển sang BT external signal)
#include "sl_sleeptimer.h"
#include "sl_bt_api.h"
#include "internal.h"

void payload_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle; (void)data;
  sl_bt_external_signal(PAYLOAD_UPDATE_TIMER_CALLBACK);
}

void oneshot_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle; (void)data;
  sl_bt_external_signal(ONESHOT_BTN_TIMER_CALLBACK);
}

void periodic_sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle; (void)data;
  sl_bt_external_signal(PERIODIC_TIMER_CALLBACK);
}

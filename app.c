// app_init.c - Khởi tạo ứng dụng
#include "sl_sleeptimer.h"
#include "app_assert.h"
#include "app.h"
#include "ble/internal.h"
#include "ble/ble_log.h"

sl_sleeptimer_timer_handle_t periodic_timer_handle;
sl_sleeptimer_timer_handle_t oneshot_btn_timer_handle;
sl_sleeptimer_timer_handle_t payload_timer_handle;

health_data_t health_payload = { .heart_rate = 0, .systolic = 0, .diastolic = 0, .spo2 = 0 };
SemaphoreHandle_t g_health_payload_mutex = NULL;

void app_init(void)
{
  sl_status_t sc;

  app_log_init();   // tạo log mutex trước tiên
  log_safe("[OK] Log mutex created\r\n");

  g_health_payload_mutex = xSemaphoreCreateMutex();
  app_assert(g_health_payload_mutex != NULL, "Health payload mutex creation failed.");
  log_safe("[OK] Health payload mutex created\r\n");

  sc = sl_sleeptimer_start_periodic_timer_ms(&periodic_timer_handle,
                                             ADDRESS_CHANGE_PERIOD_MS,
                                             periodic_sleeptimer_callback,
                                             (void *)NULL, 0, 0);
  app_assert_status(sc);
  log_safe("[OK] Periodic address-change timer started (%d ms)\r\n", ADDRESS_CHANGE_PERIOD_MS);

  app_init_bt();
  log_safe("[OK] app_init_bt() done - PPG/FILTER/AI/BLE tasks created\r\n");
}

void app_process_action(void)
{
  if (app_is_process_required()) {
    // Put your additional application code here!
  }
}

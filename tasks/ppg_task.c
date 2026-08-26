// ppg_task.c
#include "ppg_task.h"
#include "sl_i2cspm_instances.h"
#include "app_log.h"
#include "../sensor/max30102.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "gpiointerrupt.h"
#include "em_gpio.h"

#define MAX30102_INT_PORT   gpioPortC
#define MAX30102_INT_PIN    0

static uint32_t drop_count = 0;

QueueHandle_t xPPGQueue = NULL;
static SemaphoreHandle_t data_ready_sem = NULL;

static void max30102_int_callback(uint8_t intNo)
{
  (void)intNo;
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(data_ready_sem, &woken);
  portYIELD_FROM_ISR(woken);
}

static void max30102_int_pin_init(void)
{
  GPIO_PinModeSet(MAX30102_INT_PORT, MAX30102_INT_PIN, gpioModeInputPull, 1);
  GPIOINT_Init();
  GPIOINT_CallbackRegister(MAX30102_INT_PIN, max30102_int_callback);
  max30102_clear_interrupt(sl_i2cspm_sensor);
  GPIO_ExtIntConfig(MAX30102_INT_PORT, MAX30102_INT_PIN, MAX30102_INT_PIN,
                     false, true, true);
}

void ppg_queue_create(void)
{
    xPPGQueue = xQueueCreate(32, sizeof(ppg_raw_sample_t));
}

void ppg_task(void *p_arg)
{
  (void)p_arg;
  max30102_sample_t sample_buf[32];

  data_ready_sem = xSemaphoreCreateBinary();
  if (data_ready_sem == NULL) {
    app_log_error("[PPG] Khong tao duoc semaphore!\r\n");
    vTaskDelete(NULL);
    return;
  }

  if (xPPGQueue == NULL) {
    app_log_error("[PPG] xPPGQueue chua duoc tao! Kiem tra ppg_queue_create().\r\n");
    vTaskDelete(NULL);
    return;
  }

  if (max30102_init(sl_i2cspm_sensor)) {
    app_log_info("[PPG] MAX30102 khoi tao thanh cong\r\n");
    max30102_int_pin_init();
  } else {
    app_log_error("[PPG] MAX30102 khoi tao that bai!\r\n");
    vTaskDelete(NULL);
    return;
  }

  for (;;) {
    if (xSemaphoreTake(data_ready_sem, portMAX_DELAY) == pdTRUE) {
      uint8_t count = max30102_get_fifo_count(sl_i2cspm_sensor);

      if (count > 0) {
        uint8_t got = max30102_read_fifo_burst(sl_i2cspm_sensor, sample_buf, count);

        for (uint8_t i = 0; i < got; i++) {
          ppg_raw_sample_t s = {
              .red = sample_buf[i].red,
              .ir  = sample_buf[i].ir
          };
          if (xQueueSend(xPPGQueue, &s, 0) != pdPASS) {
              // queue day -> drop sample, khong duoc block PPG task
              drop_count++;
              if (drop_count % 100 == 0) {
                  app_log_warning("[PPG] Da drop %lu mau do lay mau cham!\r\n", drop_count);
              }
          }
        }
      }
      max30102_clear_interrupt(sl_i2cspm_sensor);
    }
  }
}
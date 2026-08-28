#include "ai_task.h"
#include "../algo/app_ml.h"
#include "../tasks/filter_task.h"
#include "app_log.h"
#include "FreeRTOS.h"
#include "task.h"

void ai_task(void *p_arg)
{
  (void)p_arg;

  filtered_sample_t in;

  // Khoi tao model TFLite Micro 1 lan duy nhat khi task bat dau chay
  ml_init();

  app_log_info("[AI] AI Task started\r\n");

  for (;;) {
    // Cho du lieu tu Filter task qua Queue, khong dung vTaskDelay nua
    if (xQueueReceive(xFilterQueue, &in, portMAX_DELAY) == pdPASS) {

      // Ham nay tu gom du 800 mau roi goi vitals_compute() + inference
      // moi INFERENCE_HOP (100) mau, thuc hien chay lai HR/SpO2 + AI moi 100 mau moi (~1s)
      ml_push_sample(in.red_bp, in.ir_bp, in.red_z, in.ir_z,
                     in.red_raw, in.ir_raw);
    }
  }
}
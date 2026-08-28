#include "bluetooth_data_task.h"
#include "../algo/app_ml.h"
#include "../app.h"
#include "app_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

void bluetooth_data_task(void *p_arg)
{
    (void)p_arg;
    ai_result_t result;

    app_log_info("[BLE] Bluetooth data task started\r\n");

    for (;;) {
        if (xQueueReceive(xAIResultQueue, &result, portMAX_DELAY) == pdPASS) {

            if (g_health_payload_mutex != NULL &&
                xSemaphoreTake(g_health_payload_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {

                if (!result.finger_detected) {
                    // Reset toàn bộ khi mất ngón tay
                    health_payload.heart_rate = 0;
                    health_payload.systolic   = 0;
                    health_payload.diastolic  = 0;
                    health_payload.spo2       = 0;
                } else {
                    health_payload.heart_rate = result.hr_valid  ? (uint8_t)result.hr   : 0;
                    health_payload.spo2       = result.spo2_valid? (uint8_t)result.spo2 : 0;
                    health_payload.systolic   = result.bp_valid  ? (uint8_t)result.sbp  : 0;
                    health_payload.diastolic  = result.bp_valid  ? (uint8_t)result.dbp  : 0;
                }


                xSemaphoreGive(g_health_payload_mutex);
                app_log_info("[BLE] Da cap nhat health_payload: HR=%.1f SpO2=%.1f SBP=%.1f DBP=%.1f\r\n",
                                    result.hr, result.spo2, result.sbp, result.dbp);
            }

        }
    }
}
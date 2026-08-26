#ifndef APP_ML_H
#define APP_ML_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    float hr, spo2, sbp, dbp;
    bool  hr_valid, spo2_valid, bp_valid, finger_detected;
} ai_result_t;

#ifdef __cplusplus
extern "C" {
#endif

void ml_init(void);
void ml_reset(void);
void ml_push_sample(float red_bp, float ir_bp,
                     float red_z,  float ir_z,
                     uint32_t red_raw, uint32_t ir_raw);

extern QueueHandle_t xAIResultQueue;
void ai_result_queue_create(void);

#ifdef __cplusplus
}
#endif

#endif // APP_ML_H
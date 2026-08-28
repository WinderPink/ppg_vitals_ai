// filter_task.h
#ifndef FILTER_TASK_H
#define FILTER_TASK_H
#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

typedef struct {
    float    red_bp, ir_bp;     // sau EMA+bandpass, CHUA z-score (vitals can ban nay)
    float    red_z,  ir_z;      // sau z-score (AI model can ban nay)
    uint32_t red_raw, ir_raw;   // tho, dung tinh DC/finger-detect
} filtered_sample_t;

extern QueueHandle_t xFilterQueue;

void filter_queue_create(void);
void filter_task(void *p_arg);

#endif
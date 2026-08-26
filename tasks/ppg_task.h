// ppg_task.h
#ifndef PPG_TASK_H
#define PPG_TASK_H
#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

typedef struct {
    uint32_t red;
    uint32_t ir;
} ppg_raw_sample_t;

extern QueueHandle_t xPPGQueue;

void ppg_queue_create(void);   // goi trong app_init_bt(), TRUOC khi tao task
void ppg_task(void *p_arg);

#endif
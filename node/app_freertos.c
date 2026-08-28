#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "app.h"

#include "tasks/ppg_task.h"
#include "tasks/filter_task.h"  
#include "tasks/ai_task.h"
#include "algo/app_ml.h"              
#include "tasks/bluetooth_data_task.h"

#define APP_TASK_NAME          "app_task"
#define APP_TASK_STACK_SIZE    512u
#define APP_TASK_PRIO          27u

#define PPG_TASK_NAME           "ppg_task"
#define PPG_TASK_STACK_SIZE     768u
#define PPG_TASK_PRIO           26u

#define FILTER_TASK_NAME        "filter_task"
#define FILTER_TASK_STACK_SIZE  512u
#define FILTER_TASK_PRIO        25u

#define AI_TASK_NAME             "ai_task"
#define AI_TASK_STACK_SIZE       4096u    
#define AI_TASK_PRIO              23u

#define BT_DATA_TASK_NAME         "bt_data_task"
#define BT_DATA_TASK_STACK_SIZE   2048u
#define BT_DATA_TASK_PRIO          24u

#define APP_MUTEX_WAIT         100 // Timeout to wait for mutex in ticks

// Application task.
static void app_task(void *p_arg);

// Task handles
static TaskHandle_t app_task_handle       = NULL;
static TaskHandle_t ppg_task_handle       = NULL;
static TaskHandle_t filter_task_handle    = NULL;
static TaskHandle_t ai_task_handle        = NULL;
static TaskHandle_t bt_data_task_handle   = NULL;

// Semaphore handle
static SemaphoreHandle_t app_semaphore_handle = NULL;

// Mutex handle
static SemaphoreHandle_t app_mutex_handle = NULL;

// Application Runtime Init.
void app_init_bt(void)
{
  BaseType_t ret;

  // ---- TẠO TẤT CẢ QUEUE TRƯỚC (bắt buộc, tránh Task đọc Queue = NULL) ----
  ppg_queue_create();          // tạo xPPGQueue
  filter_queue_create();       // tạo xFilterQueue
  ai_result_queue_create();    // tạo xAIResultQueue

  // --- Task xu ly Bluetooth stack (co san tu template) ---
  ret = xTaskCreate(app_task,
                    APP_TASK_NAME,
                    APP_TASK_STACK_SIZE,
                    NULL,
                    APP_TASK_PRIO,
                    &app_task_handle);
  app_assert(ret == pdPASS, "Application task creation failed.");

  app_semaphore_handle = xSemaphoreCreateCounting(UINT16_MAX, 0);
  app_assert(app_semaphore_handle != NULL, "Semaphore creation failed.");

  app_mutex_handle = xSemaphoreCreateRecursiveMutex();
  app_assert(app_mutex_handle != NULL, "Mutex creation failed.");

  // --- PPG task ---
  ret = xTaskCreate(ppg_task, PPG_TASK_NAME, PPG_TASK_STACK_SIZE,
                    NULL, PPG_TASK_PRIO, &ppg_task_handle);
  app_assert(ret == pdPASS, "PPG task creation failed.");

  // --- Filter task ---
  ret = xTaskCreate(filter_task, FILTER_TASK_NAME, FILTER_TASK_STACK_SIZE,
                    NULL, FILTER_TASK_PRIO, &filter_task_handle);
  app_assert(ret == pdPASS, "Filter task creation failed.");

  // --- AI task ---
  ret = xTaskCreate(ai_task, AI_TASK_NAME, AI_TASK_STACK_SIZE,
                    NULL, AI_TASK_PRIO, &ai_task_handle);
  app_assert(ret == pdPASS, "AI task creation failed.");

  // --- Bluetooth data task ---
  ret = xTaskCreate(bluetooth_data_task, BT_DATA_TASK_NAME, BT_DATA_TASK_STACK_SIZE,
                    NULL, BT_DATA_TASK_PRIO, &bt_data_task_handle);
  app_assert(ret == pdPASS, "BT data task creation failed.");
}

/******************************************************************************
 * Application task.
 *****************************************************************************/
static void app_task(void *p_arg)
{
  (void)p_arg;
  while (1) {
    app_process_action();
  }
}

// Proceed with execution.
void app_proceed(void)
{
  if (xPortIsInsideInterrupt()) {
    BaseType_t woken = pdFALSE;
    (void)xSemaphoreGiveFromISR(app_semaphore_handle, &woken);
    portYIELD_FROM_ISR(woken);
  } else {
    (void)xSemaphoreGive(app_semaphore_handle);
  }
}

// Check if it is required to process with execution.
bool app_is_process_required(void)
{
  BaseType_t ret = xSemaphoreTake(app_semaphore_handle, portMAX_DELAY);
  return (ret == pdTRUE);
}

// Acquire access to protected variables
bool app_mutex_acquire(void)
{
  BaseType_t response;
  response = xSemaphoreTakeRecursive(app_mutex_handle, (TickType_t)APP_MUTEX_WAIT);
  return response == pdTRUE;
}

// Finish access to protected variables
void app_mutex_release(void)
{
  (void)xSemaphoreGiveRecursive(app_mutex_handle);
}
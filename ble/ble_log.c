// app_log.c
#include "FreeRTOS.h"
#include "semphr.h"
#include "app_log.h"
#include <stdarg.h>
#include <stdio.h>

static SemaphoreHandle_t g_log_mutex = NULL;

void app_log_module_init(void)
{
  g_log_mutex = xSemaphoreCreateMutex();
}

void log_safe(const char *fmt, ...)
{
  char buf[256];
  va_list args;

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (g_log_mutex != NULL) {
    xSemaphoreTake(g_log_mutex, portMAX_DELAY);
  }
  app_log_info("%s", buf);
  if (g_log_mutex != NULL) {
    xSemaphoreGive(g_log_mutex);
  }
}

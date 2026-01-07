#pragma once
#include "FreeRTOS.h"

typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

#define tskIDLE_PRIORITY (0)

/* ---- Task creation stub ---- */
static inline BaseType_t xTaskCreate(
  TaskFunction_t pxTaskCode,
  const char * const pcName,
  const uint16_t usStackDepth,
  void * const pvParameters,
  UBaseType_t uxPriority,
  TaskHandle_t * const pxCreatedTask
){
  (void)pxTaskCode; (void)pcName; (void)usStackDepth;
  (void)pvParameters; (void)uxPriority; (void)pxCreatedTask;
  return pdPASS;
}

/* ---- Tick counter stub ---- */
static inline TickType_t xTaskGetTickCount(void)
{
  static TickType_t fake_tick = 0;
  return fake_tick++;   /* monotonic, wrap-safe */
}

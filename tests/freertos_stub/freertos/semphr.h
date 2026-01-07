#pragma once
#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;
typedef struct { uint32_t dummy; } StaticSemaphore_t;

static inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer)
{
  (void)pxMutexBuffer;
  return (SemaphoreHandle_t)0x1;
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xTicksToWait)
{
  (void)xSemaphore; (void)xTicksToWait;
  return pdPASS;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
  (void)xSemaphore;
  return pdPASS;
}

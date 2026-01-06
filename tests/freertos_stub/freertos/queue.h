#pragma once
#include "FreeRTOS.h"

typedef void* QueueHandle_t;

static inline QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
  (void)uxQueueLength; (void)uxItemSize;
  return (QueueHandle_t)0x1; /* non-NULL */
}

static inline BaseType_t xQueueSend(QueueHandle_t xQueue, const void * pvItemToQueue, uint32_t xTicksToWait)
{
  (void)xQueue; (void)pvItemToQueue; (void)xTicksToWait;
  return pdPASS;
}

static inline BaseType_t xQueueReceive(QueueHandle_t xQueue, void * pvBuffer, uint32_t xTicksToWait)
{
  (void)xQueue; (void)pvBuffer; (void)xTicksToWait;
  return pdFAIL; /* compile-only */
}

static inline BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void * pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken)
{
  (void)xQueue; (void)pvItemToQueue;
  if (pxHigherPriorityTaskWoken) *pxHigherPriorityTaskWoken = pdFALSE;
  return pdPASS;
}


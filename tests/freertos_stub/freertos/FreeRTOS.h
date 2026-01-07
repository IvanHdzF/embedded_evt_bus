#pragma once
#include <stdint.h>

/* Core FreeRTOS scalar types */
typedef int32_t  BaseType_t;
typedef uint32_t UBaseType_t;

/* Pick one; this is common for 32-bit ports */
typedef uint32_t TickType_t;

/* Config needed for time conversions */
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000U
#endif

#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS (1000U / configTICK_RATE_HZ)
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t)((xTimeInMs) / portTICK_PERIOD_MS))
#endif

/* Status codes */
#define pdPASS   (1)
#define pdFAIL   (0)
#define pdTRUE   (1)
#define pdFALSE  (0)

#define portMAX_DELAY ((TickType_t)0xffffffffu)
#define portYIELD_FROM_ISR(x) do { (void)(x); } while (0)

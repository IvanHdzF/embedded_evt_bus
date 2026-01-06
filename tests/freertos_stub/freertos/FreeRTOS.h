#pragma once
#include <stdint.h>

typedef int32_t  BaseType_t;
typedef uint32_t UBaseType_t;

#define pdPASS   (1)
#define pdFAIL   (0)
#define pdTRUE   (1)
#define pdFALSE  (0)

#define portMAX_DELAY (0xffffffffu)

/* If your port uses these */
#define portYIELD_FROM_ISR(x) do { (void)(x); } while(0)


/* Extracted from portmacro.h */
#define portCHAR                    int8_t
#define portFLOAT                   float
#define portDOUBLE                  double
#define portLONG                    int32_t
#define portSHORT                   int16_t
#define portSTACK_TYPE              uint8_t
#define portBASE_TYPE               int

typedef portSTACK_TYPE              StackType_t;
typedef portBASE_TYPE               BaseType_t;
typedef unsigned portBASE_TYPE      UBaseType_t;

/* Extracted from FreeRTOSConfig_arch.h */
#define configMINIMAL_STACK_SIZE                   ( ( StackType_t ) ( 0x4000 + 40 ) / sizeof( StackType_t ) )
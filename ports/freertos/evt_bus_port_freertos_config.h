#ifndef PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_CONFIG_H_
#define PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_CONFIG_H_

#include "freertos/FreeRTOS.h"

#if defined(ESP_PLATFORM)
#include "sdkconfig.h"
/* Override default FreeRTOS configuration */
#define EVT_BUS_FREERTOS_TASK_NAME CONFIG_EVT_BUS_PORT_TASK_NAME
#define EVT_BUS_FREERTOS_TASK_PRIO CONFIG_EVT_BUS_PORT_TASK_PRIO
#define EVT_BUS_FREERTOS_STACK_WORDS CONFIG_EVT_BUS_PORT_STACK_WORDS
#define EVT_BUS_FREERTOS_QUEUE_DEPTH CONFIG_EVT_BUS_PORT_QUEUE_DEPTH
#define EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS CONFIG_EVT_BUS_PORT_HEARTBEAT_TICK_MS
#endif

/* FreeRTOS-specific configuration for the Event Bus port */

/* Name of the FreeRTOS task that runs the event bus dispatcher */

#ifndef EVT_BUS_FREERTOS_TASK_NAME
#define EVT_BUS_FREERTOS_TASK_NAME "evt_bus"
#endif

/* Priority of the FreeRTOS task that runs the event bus dispatcher */
#ifndef EVT_BUS_FREERTOS_TASK_PRIO
#define EVT_BUS_FREERTOS_TASK_PRIO (tskIDLE_PRIORITY + 1)
#endif
/* Stack size (in words) of the FreeRTOS task that runs the event bus dispatcher */
#ifndef EVT_BUS_FREERTOS_STACK_WORDS
#define EVT_BUS_FREERTOS_STACK_WORDS (4096 + 128)
#endif

/* Heartbeat tick rate in milliseconds */
#ifndef EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS
#define EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS 1000
#endif

#endif /* PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_CONFIG_H_ */
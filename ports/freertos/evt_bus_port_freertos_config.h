#ifndef PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_CONFIG_H_

#define PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_CONFIG_H_

#include "FreeRTOS.h"

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
#define EVT_BUS_FREERTOS_STACK_WORDS (configMINIMAL_STACK_SIZE + 128)
#endif

#endif /* PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_CONFIG_H_ */
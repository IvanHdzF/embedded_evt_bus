#ifndef PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_H_
#define PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_H_

#include <stdbool.h>
#include <stdint.h>

#include "evt_bus/evt_bus_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize FreeRTOS backend + (optionally) create dispatcher task.
 * Returns false on queue/task creation failure.
 */
bool evt_bus_freertos_init(void);

/* Optional: ISR-safe publish wrapper (if backend supports enqueue_isr). */
bool evt_bus_publish_from_isr(evt_id_t evt_id, const void *payload, uint16_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_H_ */
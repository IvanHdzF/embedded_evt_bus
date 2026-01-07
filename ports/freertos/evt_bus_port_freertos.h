#ifndef PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_H_
#define PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_H_

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the tick count of the last heartbeat beat.
 * 
 * @return TickType_t 
 */
TickType_t evt_bus_freertos_hb_last_tick(void);

/**
 * @brief Returns the number of heartbeat beats since start.
 * 
 * @return uint32_t 
 */
uint32_t   evt_bus_freertos_hb_beat_count(void);

/**
 * @brief Returns the number of events dispatched since start.
 * 
 * @return uint32_t 
 */
uint32_t   evt_bus_freertos_hb_events_dispatched(void);

#ifdef __cplusplus
}
#endif

#endif /* PORTS_FREERTOS_EVT_BUS_PORT_FREERTOS_H_ */
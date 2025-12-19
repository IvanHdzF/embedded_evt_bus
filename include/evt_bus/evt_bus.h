#ifndef EVT_BUS_H
#define EVT_BUS_H

#include "evt_bus_types.h"
#ifdef __cplusplus

extern "C" {
#endif

/* Event Bus API */

void evt_bus_init(void);

evt_handle_t evt_bus_subscribe(evt_id_t evt_id, evt_cb_t cb);

void evt_bus_unsubscribe(evt_handle_t handle);

/* Enqueue an event for later dispatch (payload model defined below). */
bool evt_bus_publish(evt_id_t evt_id, const void *payload, size_t payload_len);

/* Dispatch helpers (called by the platform binding / user loop). */
bool evt_bus_dispatch_one(void);   /* returns false if no event available */
void evt_bus_dispatch_all(void);   /* drains until empty */

#ifdef __cplusplus
}
#endif
#endif /* EVT_BUS_H */
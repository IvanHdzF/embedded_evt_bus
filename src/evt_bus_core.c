#include "evt_bus/evt_bus.h"
#include "evt_bus/evt_bus_config.h"

//TODO: Add necessary includes for backend implementation (e.g., evt_bus_port_freertos)

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


typedef struct{
    evt_sub_handle_t handle;
    evt_cb_t cb;
} evt_subscriber_t;

typedef struct{
    evt_id_t id;
    evt_sub_handle_t subscribers[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT];
} evt_subscription_t;


static evt_subscriber_t subscriber_pool[EVT_BUS_MAX_HANDLES];
static evt_subscription_t subscriptions[EVT_BUS_MAX_EVT_IDS];

extern evt_bus_backend_t evt_bus_backend; /* Defined in port file */

/* Local helpers */

static bool allocate_handle(evt_sub_handle_t *out_handle){
    for (size_t i = 0; i < EVT_BUS_MAX_HANDLES; i++){
        if (subscriber_pool[i].cb == NULL){
            /* Found free handle */
            out_handle->id = (evt_id_t)i;
            out_handle->gen += 1; /* Increment generation */
            return true;
        }
    }
    return false; /* No free handles */
}

/* Public API */

void evt_bus_init(void){
    /* Initialize subscriber pool */
    for (size_t i = 0; i < EVT_BUS_MAX_HANDLES; i++){
        subscriber_pool[i].cb = NULL;
    }
    /* Initialize subscription table */
    for (size_t i = 0; i < EVT_BUS_MAX_EVT_IDS; i++){
        subscriptions[i].id = 0;
        for (size_t j = 0; j < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; j++){
            subscriptions[i].subscribers[j].id = EVT_HANDLE_ID_INVALID;
            subscriptions[i].subscribers[j].gen = 0;
        }
    }
}



evt_handle_t evt_bus_subscribe(evt_id_t evt_id, evt_cb_t cb){

}

void evt_bus_unsubscribe(evt_handle_t handle){

}



/* Enqueue an event for later dispatch (payload model defined below). */
bool evt_bus_publish(evt_id_t evt_id, const void *payload, size_t payload_len){
    /* Validate inputs */
    if (payload_len > EVT_INLINE_MAX){
        return false;
    }
    if (evt_id > EVT_BUS_MAX_EVT_IDS){
        return false;
    }

    evt_t evt = {0};
    evt.id = evt_id;
    evt.len = (uint16_t)payload_len;
    memcpy(evt.payload, payload, payload_len);

    /* Send callback to dispatcher queue */
    return evt_bus_backend.enqueue(&evt);
}


/* Core dispatch: called by the port-side dispatcher thread after dequeueing an evt_t.
 *
 * Assumptions (per your “slots live under evt_id” design):
 * - subscriptions[evt_id] owns the subscriber slots (no global handle pool).
 * - Each slot has: cb, active, gen (gen is mainly for unsubscribe stale-handle protection).
 * - publish() enqueues only evt_t; dispatch() fans out to all active slots.
 * - Optional locking is provided by evt_bus_backend.lock/unlock (can be NULL).
 */

static void evt_bus_core_dispatch_evt(const evt_t *evt)
{
    if (evt == NULL) return;

    const evt_id_t evt_id = evt->id;
    if (evt_id >= EVT_BUS_MAX_EVT_IDS) 
    {
        return;
    }

    /* Optional: clamp len defensively (should already be bounded by publish/queue). */
    uint16_t len = evt->len;
    if (len > EVT_INLINE_MAX){
        len = EVT_INLINE_MAX;
    }

    if (evt_bus_backend.lock){
        evt_bus_backend.lock(evt_bus_backend.ctx);
    }
    /* TODO: Fanout: iterate fixed subscriber slots for this evt_id */


}

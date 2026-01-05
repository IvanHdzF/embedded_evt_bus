#include "evt_bus/evt_bus.h"
#include "evt_bus/evt_bus_config.h"

//TODO: Add necessary includes for backend implementation (e.g., evt_bus_port_freertos)

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>


typedef struct{
    evt_sub_handle_t handle;
    evt_cb_t cb;
    void* user_ctx;
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
            subscriber_pool[i].handle.id = (hndl_id_t)i;
            subscriber_pool[i].handle.gen += 1; /* Increment generation */
            out_handle->id = subscriber_pool[i].handle.id;
            out_handle->gen = subscriber_pool[i].handle.gen;
            return true;
        }
    }
    return false; /* No free handles */
}

static bool register_subscription_slot(evt_id_t evt_id, evt_sub_handle_t handle){
    if (evt_id >= EVT_BUS_MAX_EVT_IDS){
        return false;
    }

    evt_subscription_t *subscription = &subscriptions[evt_id];
    for (size_t i = 0; i < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; i++){
        if (subscription->subscribers[i].id == EVT_HANDLE_ID_INVALID){
            /* Found free slot */
            subscription->subscribers[i] = handle;
            return true;
        }
    }
    return false; /* No free slots */
}

/* Public API */

void evt_bus_init(void){

    assert((evt_bus_backend.lock == NULL) == (evt_bus_backend.unlock == NULL)
       && "evt_bus_backend lock/unlock must both be NULL or both non-NULL");

    /* Initialize subscriber pool */
    for (size_t i = 0; i < EVT_BUS_MAX_HANDLES; i++){
        subscriber_pool[i].cb = NULL;
        subscriber_pool[i].user_ctx = NULL;
        subscriber_pool[i].handle.id = EVT_HANDLE_ID_INVALID;
        subscriber_pool[i].handle.gen = 0;
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



evt_sub_handle_t evt_bus_subscribe(evt_id_t evt_id, evt_cb_t cb, void* user_ctx){
    if (evt_id >= EVT_BUS_MAX_EVT_IDS)
    {
        return (evt_sub_handle_t){ .id = EVT_HANDLE_ID_INVALID, .gen = 0 };
    }

    if (cb == NULL){
        return (evt_sub_handle_t){ .id = EVT_HANDLE_ID_INVALID, .gen = 0 };
    }

    evt_sub_handle_t handle = {0};

    if (!allocate_handle(&handle)){
        return handle;
    }
    subscriber_pool[handle.id].cb = cb;
    subscriber_pool[handle.id].user_ctx = user_ctx;
    subscriptions[evt_id].id = evt_id;
    register_subscription_slot(evt_id, handle);
    return handle;
}

void evt_bus_unsubscribe(evt_sub_handle_t handle){

}



/* Enqueue an event for later dispatch (payload model defined below). */
bool evt_bus_publish(evt_id_t evt_id, const void *payload, size_t payload_len){
    /* Validate inputs */
    if (payload_len > EVT_INLINE_MAX){
        return false;
    }
    if (evt_id >= EVT_BUS_MAX_EVT_IDS){
        return false;
    }

    if (payload_len > 0 && payload == NULL) 
    {
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

void evt_bus_dispatch_evt(const evt_t *evt)
{
    if (!evt) return;

    const evt_id_t evt_id = evt->id;
    if (evt_id >= EVT_BUS_MAX_EVT_IDS) return;

    /* Local snapshot for just this event id */
    evt_cb_t cbs[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT];
    void   *ctxs[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT];
    size_t  n = 0;


    /* Snapshot events on lock to avoid lock contention on executing callback */
    if (evt_bus_backend.lock) {
        evt_bus_backend.lock(evt_bus_backend.ctx);
    }

    const evt_subscription_t *subscription = &subscriptions[evt_id];

    for (size_t i = 0; i < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; i++) {
        const evt_sub_handle_t h = subscription->subscribers[i];
        if (h.id == EVT_HANDLE_ID_INVALID) 
        {
            continue;
        }
        /* Validate handle id range (defensive) */
        if ((size_t)h.id >= EVT_BUS_MAX_HANDLES) 
        {
            continue;
        }
        const evt_subscriber_t *sub = &subscriber_pool[h.id];

        /* Stale handle check */
        if (sub->cb == NULL) 
        {
            continue;
        }

        if (sub->handle.gen != h.gen) 
        {
            continue;
        }

        /* Snapshot cb + ctx */
        cbs[n]  = sub->cb;
        ctxs[n] = sub->user_ctx;
        n++;
    }

    if (evt_bus_backend.unlock) {
        evt_bus_backend.unlock(evt_bus_backend.ctx);
    }

    /* Fan out without lock */
    for (size_t i = 0; i < n; i++) {
        cbs[i](evt, ctxs[i]);
    }
}

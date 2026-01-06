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

static inline bool evt_handle_is_valid(evt_sub_handle_t h) {
  return h.id != EVT_HANDLE_ID_INVALID;
}

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

static bool register_subscription_slot(const evt_id_t evt_id, const evt_sub_handle_t handle)
{
    evt_subscription_t *sub = &subscriptions[evt_id];

    for (size_t i = 0; i < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; i++) {
        evt_sub_handle_t *slot = &sub->subscribers[i];

        if (slot->id != EVT_HANDLE_ID_INVALID) {
            /* self-heal stale slot */
            if ((size_t)slot->id >= EVT_BUS_MAX_HANDLES ||
                subscriber_pool[slot->id].cb == NULL ||
                subscriber_pool[slot->id].handle.gen != slot->gen) {
                slot->id = EVT_HANDLE_ID_INVALID;
                slot->gen = 0;
            }
        }

        if (slot->id == EVT_HANDLE_ID_INVALID) {
            *slot = handle;
            return true;
        }
    }
    return false;
}

/* Public API */

void evt_bus_init(void){

    assert((evt_bus_backend.lock == NULL) == (evt_bus_backend.unlock == NULL)
       && "evt_bus_backend lock/unlock must both be NULL or both non-NULL");

    /* Check for init function provided in port */
    if (evt_bus_backend.init != NULL) {
        bool ok = evt_bus_backend.init();
        assert(ok && "evt_bus_backend init failed");
    } else {
        /* No init function: assume no further initialization in port needed */
    }
    assert(EVT_BUS_MAX_HANDLES > 0 && "EVT_BUS_MAX_HANDLES must be > 0");

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
        /* No free handles */
        handle.id = EVT_HANDLE_ID_INVALID;
        handle.gen = 0;
        return handle;
    }

    if(register_subscription_slot(evt_id, handle) == false){
        /* Failed to register slot */
        handle.id = EVT_HANDLE_ID_INVALID;
        handle.gen = 0;
        return handle;
    }

    subscriber_pool[handle.id].cb = cb;
    subscriber_pool[handle.id].user_ctx = user_ctx;
    subscriptions[evt_id].id = evt_id;

    return handle;
}

void evt_bus_unsubscribe(evt_sub_handle_t handle){
    if (!evt_handle_is_valid(handle)){
        return;
    }

    if ((size_t)handle.id >= EVT_BUS_MAX_HANDLES){
        return;
    }

    evt_bus_backend.lock(evt_bus_backend.ctx);
    /* Remove from subscription slots */
    subscriber_pool[handle.id].cb = NULL;
    subscriber_pool[handle.id].user_ctx = NULL;
    subscriber_pool[handle.id].handle.id = EVT_HANDLE_ID_INVALID;
    evt_bus_backend.unlock(evt_bus_backend.ctx);
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
        evt_sub_handle_t h = subscription->subscribers[i];
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
        if (sub->cb == NULL || sub->handle.gen != h.gen) {
            /* self-heal: reclaim dead slot */
            h.id = EVT_HANDLE_ID_INVALID;
            h.gen = 0;
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

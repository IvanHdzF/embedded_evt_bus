#include "evt_bus/evt_bus.h"
#include "evt_bus/evt_bus_config.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <array>


typedef struct{
    evt_sub_handle_t handle;
    evt_cb_t cb;
    void* user_ctx;
} evt_subscriber_t;

typedef struct{
    evt_id_t id;
    evt_sub_handle_t subscribers[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT];
} evt_subscription_t;



extern evt_bus_backend_t evt_bus_backend; /* Defined in port file */

namespace{
class EventBus  {
    public:
        void init(void) noexcept;
        evt_sub_handle_t subscribe(evt_id_t evt_id, evt_cb_t cb, void* user_ctx);
        void unsubscribe(evt_sub_handle_t handle);
        bool publish(evt_id_t evt_id, const void *payload, size_t payload_len);
        bool publish_from_isr(evt_id_t evt_id, const void *payload, size_t payload_len);
        void dispatch_evt(const evt_t *evt);

    private: 
        void reset_tables() noexcept;

        std::array<evt_subscriber_t, EVT_BUS_MAX_HANDLES> subscriber_pool_;
        std::array<evt_subscription_t, EVT_BUS_MAX_EVT_IDS> subscriptions_;

        /* Local helpers */
        struct BackendLockGuard {
            evt_bus_backend_t& b;
            bool locked{false};

            explicit BackendLockGuard(evt_bus_backend_t& backend) noexcept : b(backend) {
            if (b.lock) { b.lock(b.ctx); locked = true; }
            }
            ~BackendLockGuard() noexcept {
            if (locked && b.unlock) { b.unlock(b.ctx); }
            }

            BackendLockGuard(const BackendLockGuard&) = delete;
            BackendLockGuard& operator=(const BackendLockGuard&) = delete;
        };

        static constexpr bool evt_handle_is_valid(evt_sub_handle_t h) noexcept {
            return h.id != EVT_HANDLE_ID_INVALID;
        }

        bool allocate_handle(evt_sub_handle_t& out_handle) noexcept{
            for (size_t i = 0; i < EVT_BUS_MAX_HANDLES; i++){
                if (subscriber_pool_[i].cb == NULL){
                    /* Found free handle */
                    subscriber_pool_[i].handle.id = (hndl_id_t)i;
                    subscriber_pool_[i].handle.gen += 1; /* Increment generation */
                    out_handle.id = subscriber_pool_[i].handle.id;
                    out_handle.gen = subscriber_pool_[i].handle.gen;
                    return true;
                }
            }
            return false; /* No free handles */
        }

        bool register_subscription_slot(const evt_id_t evt_id, const evt_sub_handle_t handle)
        {
            evt_subscription_t *sub = &subscriptions_[evt_id];

            for (size_t i = 0; i < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; i++) {
                evt_sub_handle_t *slot = &sub->subscribers[i];

                if (slot->id != EVT_HANDLE_ID_INVALID) {
                    /* self-heal stale slot */
                    if ((size_t)slot->id >= EVT_BUS_MAX_HANDLES ||
                        subscriber_pool_[slot->id].cb == NULL ||
                        subscriber_pool_[slot->id].handle.gen != slot->gen) {
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

};

void EventBus::reset_tables() noexcept {
    /* Clear subscriber pool */
    for (auto& sub : subscriber_pool_) {
        sub.handle.id = EVT_HANDLE_ID_INVALID;
        sub.handle.gen = 0;
        sub.cb = nullptr;
        sub.user_ctx = nullptr;
    }

    /* Clear subscriptions */
    for (auto& sub : subscriptions_) {
        sub.id = 0;
        for (auto& slot : sub.subscribers) {
            slot.id = EVT_HANDLE_ID_INVALID;
            slot.gen = 0;
        }
    }
}

void EventBus::init(void) noexcept {
    // These are runtime asserts; compile-time invariants should be static_assert.
    assert(EVT_BUS_MAX_HANDLES > 0);

    // lock/unlock consistency
    assert(((evt_bus_backend.lock == nullptr) == (evt_bus_backend.unlock == nullptr)));

    // port init
    if (evt_bus_backend.init) {
        const bool ok = evt_bus_backend.init();
        assert(ok);
    }

    reset_tables();
}

evt_sub_handle_t EventBus::subscribe(evt_id_t evt_id, evt_cb_t cb, void* user_ctx) {
    evt_sub_handle_t handle = { .id = EVT_HANDLE_ID_INVALID, .gen = 0 };
    BackendLockGuard lk(evt_bus_backend); // <- lock held for scope

    /* Cheap validation first */
    if (evt_id >= EVT_BUS_MAX_EVT_IDS) return handle;
    if (cb == NULL) return handle;



    /* Allocate handle */
    if (!allocate_handle(handle)) {
        handle.id = EVT_HANDLE_ID_INVALID;
        handle.gen = 0;
    }

    /* Register slot */
    if (!register_subscription_slot(evt_id, handle)) {
        /* Optional: release the allocated handle explicitly (not strictly required if cb==NULL marks free) */
        subscriber_pool_[handle.id].cb = NULL;
        subscriber_pool_[handle.id].user_ctx = NULL;
        handle.id = EVT_HANDLE_ID_INVALID;
        handle.gen = 0;

        // optionally keep gen as-is; just mark free
        return handle;
    }

    subscriber_pool_[handle.id].cb = cb;
    subscriber_pool_[handle.id].user_ctx = user_ctx;
    subscriptions_[evt_id].id = evt_id;

    return handle;
}


void EventBus::unsubscribe(evt_sub_handle_t handle) {
    if (!evt_handle_is_valid(handle)){
        return;
    }

    if ((size_t)handle.id >= EVT_BUS_MAX_HANDLES){
        return;
    }

    if (subscriber_pool_[handle.id].cb == NULL){
        /* Stale handle */
        return;
    }

    if (subscriber_pool_[handle.id].handle.gen != handle.gen){
        /* Stale handle */
        return;
    }
    BackendLockGuard lk(evt_bus_backend); // <- lock held for scope

    /* Remove from subscription slots */
    subscriber_pool_[handle.id].cb = NULL;
    subscriber_pool_[handle.id].user_ctx = NULL;
    subscriber_pool_[handle.id].handle.id = EVT_HANDLE_ID_INVALID;
}

void EventBus::dispatch_evt(const evt_t *evt) {
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

    evt_subscription_t *subscription = &subscriptions_[evt_id];

    for (size_t i = 0; i < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; i++) {
        evt_sub_handle_t *slot = (evt_sub_handle_t *)&subscription->subscribers[i];
        evt_sub_handle_t h = *slot;

        if (h.id == EVT_HANDLE_ID_INVALID) {
            continue;
        }

        /* Validate handle id range (defensive) */
        if ((size_t)h.id >= EVT_BUS_MAX_HANDLES) {
            /* self-heal: reclaim invalid id slot */
            slot->id  = EVT_HANDLE_ID_INVALID;
            slot->gen = 0;
            continue;
        }

        const evt_subscriber_t *sub = &subscriber_pool_[h.id];

        /* Stale handle check */
        if (sub->cb == NULL || sub->handle.gen != h.gen) {
            /* self-heal: reclaim dead slot */
            slot->id  = EVT_HANDLE_ID_INVALID;
            slot->gen = 0;
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

bool EventBus::publish(evt_id_t evt_id, const void *payload, size_t payload_len) {
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

    if (evt_bus_backend.enqueue == NULL) {
        return false;
    }

    evt_t evt = {};
    evt.id = evt_id;
    evt.len = (uint16_t)payload_len;
    if (payload_len) {
        memcpy(evt.payload, payload, payload_len);
    }

    /* Send callback to dispatcher queue */
    return evt_bus_backend.enqueue(&evt);
}


bool EventBus::publish_from_isr(evt_id_t evt_id, const void *payload, size_t payload_len) {
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

    if (evt_bus_backend.enqueue_isr == NULL) {
        return false;
    }

    evt_t evt = {};
    evt.id = evt_id;
    evt.len = (uint16_t)payload_len;
    if (payload_len) {
        memcpy(evt.payload, payload, payload_len);
    }

    /* Send callback to dispatcher queue */
    return evt_bus_backend.enqueue_isr(&evt);
}




}// namespace




/* Public API */

namespace evt_bus {

namespace {
    EventBus  evt_bus_inst;
}
    void init(void)
    {
        evt_bus_inst.init();
    }

    evt_sub_handle_t subscribe(evt_id_t evt_id, evt_cb_t cb, void* user_ctx)
    {
        return evt_bus_inst.subscribe(evt_id, cb, user_ctx);
    }
    void unsubscribe(evt_sub_handle_t handle)
    {
        evt_bus_inst.unsubscribe(handle);
    }
    bool publish(evt_id_t evt_id, const void *payload, size_t payload_len)
    {
        return evt_bus_inst.publish(evt_id, payload, payload_len);
    }
    bool publish_from_isr(evt_id_t evt_id, const void *payload, size_t payload_len)
    {
        return evt_bus_inst.publish_from_isr(evt_id, payload, payload_len);
    }
    void dispatch_evt(const evt_t *evt)
    {
        evt_bus_inst.dispatch_evt(evt);
    }
}



extern "C" {

void evt_bus_init(void) {
  evt_bus::init();
}

evt_sub_handle_t evt_bus_subscribe(evt_id_t evt_id, evt_cb_t cb, void* user_ctx) {
  return evt_bus::subscribe(evt_id, cb, user_ctx);
}

void evt_bus_unsubscribe(evt_sub_handle_t handle) {
  evt_bus::unsubscribe(handle);
}

bool evt_bus_publish(evt_id_t evt_id, const void* payload, size_t payload_len) {
  return evt_bus::publish(evt_id, payload, payload_len);
}

bool evt_bus_publish_from_isr(evt_id_t evt_id, const void* payload, size_t payload_len) {
  return evt_bus::publish_from_isr(evt_id, payload, payload_len);
}

void evt_bus_dispatch_evt(const evt_t* evt) {
  evt_bus::dispatch_evt(evt);
}

} // extern "C"
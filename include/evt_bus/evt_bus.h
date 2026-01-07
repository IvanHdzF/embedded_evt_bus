#ifndef EVT_BUS_H
#define EVT_BUS_H

/**
 * @file evt_bus.h
 * @brief Deterministic, bounded publish/subscribe event bus for embedded systems.
 *
 * Core properties:
 * - Publish is enqueue-only (no callbacks in publisher context).
 * - Dispatch executes callbacks in a single dispatcher context (serialized, deterministic).
 * - Unsubscribe is O(1) using generation-validated handles (stale handles are safe no-ops).
 * - Bounded resources: no heap; fixed limits; copy-in inline payload (see evt_bus_types.h).
 *
 * Threading model:
 * - evt_bus_publish() is intended to be thread-safe via the backend queue implementation.
 * - evt_bus_subscribe()/evt_bus_unsubscribe() may be called from tasks; not ISR-safe.
 * - Callbacks MUST NOT block; offload heavy work to module queues/tasks.
 *
 * Locking model:
 * - If the backend provides lock/unlock, the core uses them to protect subscription tables.
 * - Dispatch snapshots callbacks under lock, then releases lock before invoking callbacks.
 */

#include "evt_bus_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the event bus core state.
 *
 * Initializes internal tables (subscriber pool + per-event subscription lists).
 *
 * @note Must be called once before any other evt_bus_* API.
 * @note Safe to call at boot; not intended to be called concurrently with other APIs.
 */
void evt_bus_init(void);

/**
 * @brief Subscribe a callback to an event ID.
 *
 * Registers @p cb for the given @p evt_id and stores @p user_ctx to be passed back
 * on dispatch.
 *
 * @param evt_id   Event identifier to subscribe to.
 * @param cb       Callback invoked during dispatch in dispatcher context.
 * @param user_ctx Opaque user pointer passed back to @p cb (may be NULL).
 *
 * @return Opaque subscription handle used for evt_bus_unsubscribe().
 *         Returns an invalid handle (id == EVT_HANDLE_ID_INVALID) on failure.
 *
 * @note Not ISR-safe.
 * @note Duplicates are implementation-defined (may be allowed or rejected).
 */
evt_sub_handle_t evt_bus_subscribe(evt_id_t evt_id, evt_cb_t cb, void *user_ctx);

/**
 * @brief Unsubscribe a previously registered handle.
 *
 * Unsubscribes in O(1) by invalidating the handle slot and bumping its generation.
 * Any stale references in per-event subscription lists are cleaned lazily (self-healing)
 * when detected during dispatch/subscribe.
 *
 * @param handle Handle returned by evt_bus_subscribe().
 *
 * @note Safe to call with invalid or stale handles (no-op).
 * @note Not ISR-safe.
 */
void evt_bus_unsubscribe(evt_sub_handle_t handle);

/**
 * @brief Publish an event (enqueue-only).
 *
 * Copies the payload bytes inline into the event envelope and enqueues to the backend.
 *
 * @param evt_id       Event identifier to publish.
 * @param payload      Pointer to payload bytes (may be NULL if @p payload_len == 0).
 * @param payload_len  Number of payload bytes to copy. Must be <= EVT_INLINE_MAX.
 *
 * @return true if the event was accepted/enqueued, false otherwise (invalid args, queue full,
 *         or backend enqueue failure).
 *
 * @note This function does not execute callbacks.
 * @note ISR-safety depends on the backend/port (use a dedicated ISR publish helper if provided).
 */
bool evt_bus_publish(evt_id_t evt_id, const void *payload, size_t payload_len);

/**
 * @brief Publish an event from an ISR context (enqueue-only).
 *
 * Copies the payload bytes inline into the event envelope and enqueues to the backend
 * using an ISR-safe enqueue function if provided.
 *
 * @param evt_id       Event identifier to publish.
 * @param payload      Pointer to payload bytes (may be NULL if @p payload_len == 0).
 * @param payload_len  Number of payload bytes to copy. Must be <= EVT_INLINE_MAX.
 *
 * @return true if the event was accepted/enqueued, false otherwise (invalid args, queue full,
 *         or backend enqueue failure).
 *
 * @note This function does not execute callbacks.
 * @note Requires that the backend provides an ISR-safe enqueue function.
 */
bool evt_bus_publish_from_isr(evt_id_t evt_id, const void *payload, size_t payload_len);

/**
 * @brief Dispatch (fan out) a single event to all subscribers of evt->id.
 *
 * Called by the platform dispatcher after dequeueing an event from the backend queue.
 * Executes callbacks in the dispatcher context, serialized, in subscription slot order.
 *
 * Self-healing:
 * - Any stale/invalid handle entries encountered are cleared from the subscription list.
 *
 * @param evt Pointer to event envelope to dispatch. If NULL, function returns immediately.
 *
 * @note Callbacks MUST NOT block.
 * @note Dispatch does not hold backend lock while executing callbacks.
 */
void evt_bus_dispatch_evt(const evt_t *evt);

#ifdef __cplusplus
}
#endif

#endif /* EVT_BUS_H */

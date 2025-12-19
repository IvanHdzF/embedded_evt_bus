/* event_bus_types.h
 *
 * POLICY:
 * - Delivery:
 *   - Events are delivered on the event-bus task context (serialized, in-order per publish).
 *
 * - Blocking rule:
 *   - Callbacks MUST NOT block (no long waits, no indefinite semaphores, no network I/O).
 *   - If work is heavy/async, callback should enqueue to its own worker/task/queue.
 */

#ifndef EVENT_BUS_TYPES_H
#define EVENT_BUS_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "evt_bus/evt_bus_config.h"

typedef uint16_t evt_id_t;
typedef uint16_t mod_id_t;

#define EVT_HANDLE_ID_INVALID 0xFFFFu

typedef struct {
  evt_id_t id;            /* EVT_* */
  uint16_t    len;           /* bytes; must be <= EVT_INLINE_MAX */
  uint8_t     payload[EVT_INLINE_MAX];
} evt_t;


typedef struct __attribute__((packed)) {
  evt_id_t id;
  uint16_t gen; /* generated count */
} evt_sub_handle_t;

typedef struct {
  void* ctx; /* opaque backend state (FreeRTOS queue handle, ringbuf instance, etc.) */

  /* Enqueue a message (thread context). Returns false on full/failure. */
  bool (*enqueue)(const evt_t *evt);

  /* Dequeue a message WITHOUT blocking. Returns true if msg written. */
  bool (*dequeue_nb)(void* ctx, evt_t* evt_out);

  /* Dequeue a message WITH blocking/wait. Returns true if msg written. */
  bool (*dequeue_block)(void* ctx, evt_t* evt_out);

  /* Optional: ISR-safe enqueue (NULL if not supported). */
  bool (*enqueue_isr)(const evt_t *evt);

  /* Optional: protect subscribe/unsubscribe vs dispatch if needed (NULL if not used). */
  void (*lock)(void* ctx);
  void (*unlock)(void* ctx);

} evt_bus_backend_t;

/* Callback signature: runs on event-bus task context */
/* user_ctx is optional context provided at subscribe time */
typedef void (*evt_cb_t)(const evt_t *evt, void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUS_TYPES_H */

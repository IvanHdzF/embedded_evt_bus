# Event Bus (`evt_bus`) — Design

## Overview

`evt_bus` is a lightweight, deterministic **publish–subscribe event system** designed for embedded firmware.

The design prioritizes:
- **Determinism**
- **Bounded memory usage**
- **RTOS portability**
- **Clear execution context separation**
- **O(1) unsubscribe semantics** (with amortized cleanup)

The bus is split into:
- a **platform-agnostic core**, and
- **thin platform bindings** (e.g. FreeRTOS).

---

## Core Principles

- **Publish = enqueue only**
  - Publishing an event never executes callbacks.
  - Safe to call from application tasks.
  - ISR publishing is supported **only if the selected port provides an ISR-safe enqueue**.

- **Dispatch = single worker context**
  - All callbacks execute in a serialized dispatcher context (the port’s dispatcher task or loop).
  - Callback execution is never performed in the publisher’s context.

- **Bounded resources**
  - No heap allocation in core.
  - Fixed-size tables and fixed-size inline payload storage.

- **O(1) unsubscribe**
  - `evt_bus_unsubscribe()` invalidates the subscriber entry in O(1) time using handle ID + generation.
  - Subscription slots are reclaimed lazily (self-healed) during subsequent subscribe or dispatch operations.

- **Self-healing subscription lists**
  - Stale or invalid subscription slots are cleaned lazily during dispatch and during new subscriptions.

---

## Ordering Guarantees

- **Event ordering:** FIFO by enqueue order (as provided by the port queue backend).
- **Subscriber ordering:** callbacks for a given `evt_id` are invoked in **subscription-slot order**, which corresponds to **subscription order** in normal operation.

> Ordering is stable assuming the port backend preserves FIFO queue semantics.

---

## Payload Model

`evt_bus` uses a copy-in payload model with an inline, fixed-size buffer.

- **Max payload size:** `EVT_INLINE_MAX` bytes.
- **Publish behavior on oversize payload:** if `payload_len > EVT_INLINE_MAX`, the publish API returns `false` and the event is not enqueued.
- **Payload lifetime:** the payload pointer provided to callbacks is valid **only during callback execution**.

Implications for applications:
- Callbacks that offload work to another task must copy any required payload bytes into application-managed storage (e.g. a worker queue item or memory pool).

---

## Module Split

### `evt_bus` Core (platform-agnostic)

**Responsibilities**
- Subscription handle allocation and validation (ID + generation)
- Event-to-subscriber fanout logic
- Generation-based stale handle detection
- Copy-in payload model (`EVT_INLINE_MAX`)
- Self-healing subscription tables (lazy reclamation)

**Does NOT depend on**
- FreeRTOS headers or types
- Dynamic allocation
- Any specific queue, mutex, or timing primitive
- Periodic wakeups or timers

The core is suitable for RTOS-based systems *and* bare-metal systems.

---

### Platform Port (e.g. FreeRTOS)

**Responsibilities**
- Event queue backend (FIFO)
- Dispatcher task or loop
- Optional ISR-safe publish helper (`enqueue_isr`)
- Optional locking primitives (`lock` / `unlock`)
- Optional observability hooks (platform-specific)

The port owns:
- the queue object
- the dispatcher execution context
- any RTOS-specific synchronization or timing behavior

---

### Thread-Safety / Locking Contract

The core is written to support concurrent usage, but **thread-safety is provided by the selected backend lock strategy**.

- If `evt_bus_backend.lock` / `unlock` are provided by the port, the core will use them to protect shared state during:
  - subscribe
  - unsubscribe
  - dispatch snapshot
- If `lock` / `unlock` are `NULL`, the application must ensure serialization externally (e.g. single-threaded or bare-metal usage).

The core itself does not enforce RTOS semantics; it only consumes optional lock hooks if present.

> At initialization time, the core asserts that `lock` and `unlock` are either both NULL or both non-NULL.

---

### Port Configuration (`evt_bus_port_freertos_config.h`)

Platform ports may expose a **port-local configuration header** to control platform-specific behavior without affecting the core.

For the FreeRTOS port, configuration is provided via:

`evt_bus_port_freertos_config.h`

Typical configuration options include:
- Dispatcher task name
- Task priority
- Stack size
- Queue depth
- Optional heartbeat / observability settings

These options:
- Are compile-time constants
- Apply only to the platform port
- Do not change core semantics or APIs

This pattern is recommended for other ports using:
`evt_bus_port_<platform>_config.h`

---

## Optional Observability / Heartbeat (Port-Specific)

Some platform ports (e.g. FreeRTOS) may implement an **optional dispatcher heartbeat**.

Purpose:
- Allow applications to observe dispatcher liveness
- Detect stalled or wedged dispatcher tasks
- Integrate with system-level watchdog or health monitoring

Properties:
- Implemented **entirely in the port layer**
- The core does **not** depend on heartbeat
- No watchdog policy or reset behavior is enforced by `evt_bus`

Recommended signals:
- **Liveness:** last heartbeat tick / beat count
- **Forward progress:** events dispatched count

|> ⚠ Enabling heartbeat implies periodic wakeups and may affect tickless idle or low-power modes.

---

## Backend Selection Model

`evt_bus` supports a single backend at link time.

- A port must define the global symbol `evt_bus_backend`.
- `evt_bus_init()` assumes `evt_bus_backend` is already populated by the selected port and calls `evt_bus_backend.init()` if non-NULL.

---

## Public Core API

```c
void evt_bus_init(void);

evt_sub_handle_t evt_bus_subscribe(
    evt_id_t evt_id,
    evt_cb_t cb,
    void *user_ctx
);

void evt_bus_unsubscribe(evt_sub_handle_t handle);

bool evt_bus_publish(
    evt_id_t evt_id,
    const void *payload,
    size_t payload_len
);

bool evt_bus_publish_from_isr(
    evt_id_t evt_id,
    const void *payload,
    size_t payload_len
);

void evt_bus_dispatch_evt(const evt_t *evt);

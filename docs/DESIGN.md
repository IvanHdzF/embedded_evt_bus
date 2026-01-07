# Event Bus (`evt_bus`) — Design

## Overview

`evt_bus` is a lightweight, deterministic **publish–subscribe event system** designed for embedded firmware.

The design prioritizes:
- **Determinism**
- **Bounded memory usage**
- **RTOS portability**
- **Clear execution context separation**
- **O(1) unsubscribe semantics**

The bus is split into:
- a **platform-agnostic core**, and
- **thin platform bindings** (e.g. FreeRTOS).

---

## Core Principles

- **Publish = enqueue only**
  - Publishing an event never executes callbacks.
  - Safe to call from application tasks (and optionally ISRs via port helpers).

- **Dispatch = single worker context**
  - All callbacks execute in a serialized dispatcher context.
  - Predictable execution order and timing.

- **Bounded resources**
  - No heap allocation in core.
  - Fixed-size tables and queues.

- **O(1) unsubscribe**
  - No global scans on unsubscribe.
  - Uses handle generation to prevent stale-handle bugs.

- **Self-healing subscription lists**
  - Stale entries are cleaned lazily during dispatch or subscription.

---

## Module Split

### `evt_bus` Core (platform-agnostic)

**Responsibilities**
- Subscription handle allocation and validation
- Event-to-subscriber fanout logic
- Generation-based stale handle detection
- Copy-in payload model
- Self-healing subscription tables

**Does NOT depend on**
- FreeRTOS headers or types
- Dynamic allocation
- Any specific queue, mutex, or timing primitive
- Periodic wakeups or timers

The core is suitable for RTOS-based systems *and* bare-metal systems.

---

### Platform Port (e.g. FreeRTOS)

**Responsibilities**
- Event queue backend
- Dispatcher task or loop
- Optional ISR-safe publish helper
- Optional locking primitives
- Optional observability hooks (platform-specific)

The port owns:
- the queue object
- the dispatcher execution context
- any RTOS-specific synchronization or timing behavior

#### Port Configuration (`evt_bus_port_freertos_config.h`)

Platform ports may expose a **port-local configuration header** to control
platform-specific behavior without affecting the core.

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

This pattern is recommended for other ports as well, using a naming convention:
`evt_bus_port_<platform>_config.h`

Examples:
- `evt_bus_port_freertos_config.h`
- `evt_bus_port_zephyr_config.h`
- `evt_bus_port_baremetal_config.h`

Keeping port configuration isolated:
- Preserves core portability
- Avoids `#ifdef` leakage into core code
- Makes platform behavior explicit and auditable

---

### Optional Observability / Heartbeat (Port-Specific)

Some platform ports (e.g. FreeRTOS) may implement an **optional dispatcher heartbeat**.

Purpose:
- Allow applications to observe dispatcher liveness
- Detect stalled or wedged dispatcher tasks
- Integrate with system-level watchdog or health monitoring

Properties:
- Implemented **entirely in the port layer**
- The core does **not** depend on heartbeat
- No watchdog policy or reset behavior is enforced by `evt_bus`

Example (FreeRTOS port):
- Dispatcher wakes periodically via bounded `xQueueReceive()`
- Heartbeat updated on:
  - idle wakeups
  - successful event dispatch
- Port exposes read-only observability APIs

Applications may consume these signals to implement:
- health monitoring
- watchdog feeding
- fault reporting

|> ⚠ Enabling heartbeat implies periodic wakeups and may affect tickless idle / low-power modes.

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

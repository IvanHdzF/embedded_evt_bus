# Porting `evt_bus` to a New Platform

This document describes how to implement a **platform port** for `evt_bus`.

The goal of a port is to bind the platform-agnostic core to a concrete execution
and synchronization environment (RTOS or bare-metal) **without changing core semantics**.

A correct port provides:

* a bounded event queue
* a single dispatcher execution context
* optional ISR-safe publish helpers
* optional locking primitives
* optional observability hooks (heartbeat)

The `evt_bus` core does **not** depend on any specific RTOS, timing, or watchdog behavior.

---

## 1. Port Responsibilities

A platform port is responsible for:

* Creating and owning the **event queue**
* Running the **dispatcher loop or task**
* Wiring backend function pointers (`evt_bus_backend_t`)
* Providing platform-specific synchronization if needed

The port must **not**:

* Execute callbacks during publish
* Allocate heap memory in steady state
* Change core data structures or semantics

---

## 2. Required Backend Functions

Each port must populate the global backend descriptor:

```c
evt_bus_backend_t evt_bus_backend;
```

### Required functions

| Function                         | Responsibility                                |
| -------------------------------- | --------------------------------------------- |
| `enqueue(const evt_t *)`         | Enqueue a copy of an event (non-blocking)     |
| `dequeue_block(void *, evt_t *)` | Block until an event is available             |
| `init()`                         | Initialize backend state and start dispatcher |

### Optional functions

| Function                     | Responsibility              |
| ---------------------------- | --------------------------- |
| `enqueue_isr(const evt_t *)` | ISR-safe publish helper     |
| `lock()` / `unlock()`        | Protect subscription tables |

### Backend contract rules

* `enqueue()` **must not block**
* `dequeue_block()` **may block indefinitely** or wake periodically
* `dequeue_block()` should return `false` **only on fatal backend error**
* Timeout wakeups (if used) must not be treated as errors

---

## 3. Dispatcher Execution Model (Critical)

Each port must ensure:

* **Exactly one dispatcher context** exists
* All callbacks execute **only** in this context
* Publish paths never execute callbacks

### RTOS example

* Dispatcher runs in a dedicated task
* Task blocks on queue receive

### Bare-metal example

* Dispatcher runs in the main loop
* Queue dequeue is polled or interrupt-driven

> ❗ The dispatcher must never run concurrently in multiple contexts.

---

## 4. Queue Semantics

The event queue must:

* Be **bounded** (fixed depth)
* Store fixed-size elements (`sizeof(evt_t)`)
* Avoid dynamic allocation in steady state

Overflow policy is backend-defined. Recommended behavior:

* **Drop-new** on overflow

The core does not assume any retry or backpressure mechanism.

---

## 5. ISR Publishing Rules (Optional)

Ports may optionally provide an ISR-safe publish helper.

If implemented:

* Must be explicitly named (e.g. `enqueue_isr`)
* Must not block
* Must copy event payload immediately

If not provided:

* ISR publishing is unsupported on that platform

The core never assumes ISR support.

---

## 6. Locking Contract

Ports may optionally provide locking primitives:

```c
lock();
unlock();
```

Rules:

* If one is provided, **both must be provided**
* Lock protects:

  * subscriber pool
  * per-event subscription tables

Dispatch behavior:

* Core snapshots callbacks under lock
* Lock is released **before executing callbacks**

This prevents deadlocks and reduces contention.

---

## 7. Optional Observability / Heartbeat

Ports may implement **optional dispatcher observability**, such as a heartbeat.

Recommended characteristics:

* Implemented in the dispatcher loop
* Updated on:

  * idle wakeups
  * successful event dispatch
* Exposed via **read-only accessors**

Purpose:

* Dispatcher liveness detection
* System health monitoring
* Watchdog integration (at application level)

> The port must **not** enforce watchdog or reset policy.

---

## 8. Port Configuration Header

Ports are encouraged to expose a platform-local configuration header:

```c
evt_bus_port_<platform>_config.h
```

Examples:

* `evt_bus_port_freertos_config.h`
* `evt_bus_port_zephyr_config.h`
* `evt_bus_port_baremetal_config.h`

Typical configuration options:

* Dispatcher task name / priority
* Stack size
* Queue depth
* Optional heartbeat timing

Rules:

* Compile-time constants only
* Must not affect core semantics
* Must not introduce platform conditionals into core

---

## 9. Common Pitfalls

Avoid the following mistakes:

* Blocking or long-running work inside callbacks
* Executing callbacks during publish
* Returning `false` from `dequeue_block()` on timeout
* Forgetting that generation counters must increment on unsubscribe
* Introducing periodic wakeups without documenting power impact
* Letting ISR publish paths allocate memory

---

## 10. Validation Checklist

Before considering a port complete:

* [ ] Single dispatcher context enforced
* [ ] No heap allocation in steady state
* [ ] Queue is bounded and fixed-size
* [ ] Unsubscribe remains O(1)
* [ ] Stale handles are safely ignored
* [ ] Callbacks never block
* [ ] Optional features are clearly documented

---

A correct port preserves all **core behavioral guarantees** while adapting execution
and synchronization to the target platform.

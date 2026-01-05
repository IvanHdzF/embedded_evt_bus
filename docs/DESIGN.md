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
- Any specific queue or mutex implementation

---

### Platform Port (e.g. FreeRTOS)

**Responsibilities**
- Event queue backend
- Dispatcher task or loop
- Optional ISR-safe publish helper
- Optional locking primitives

The port owns:
- the queue object
- the dispatcher thread
- any RTOS-specific synchronization

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

void evt_bus_dispatch_evt(const evt_t *evt);
```

---

## Callback Contract

```c
typedef void (*evt_cb_t)(const evt_t *evt, void *user_ctx);
```

- `evt` is the event envelope
- `user_ctx` is the pointer provided at subscribe time (may be `NULL`)
- Callbacks execute in dispatcher context
- Callbacks **must not block**

---

## Execution Model

1. **Publish**
   - `evt_bus_publish()` validates arguments
   - Copies `(evt_id, payload)` into a bounded queue
   - Returns immediately

2. **Dispatch**
   - A platform-owned dispatcher dequeues `evt_t`
   - Calls `evt_bus_dispatch_evt(evt)`
   - Fanout executes callbacks sequentially

---

## Data Model

### 1) Subscriber Pool (Global)

Indexed by **handle index**.

Each entry contains:
- callback function pointer
- `user_ctx`
- handle `{id, generation}`

A slot is considered **free** when `cb == NULL`.

---

### 2) Subscription Table (Per Event)

Indexed by `evt_id`.

Each event owns a **fixed-size array of handles**:

```
evt_id → [ handle_0, handle_1, ... handle_N ]
```

Properties:
- Maximum subscribers per event is compile-time bounded
- Entries may temporarily contain stale handles
- Cleaned lazily (self-healing)

---

### 3) Event Queue (Port-Owned)

Stores `evt_t` entries:
- `evt_id`
- payload length
- inline payload bytes

Overflow behavior is defined by the backend (recommended: drop-new).

---

## Handle Model (Index + Generation)

Handles are opaque values:

```
handle = { index, generation }
```

Rules:
- Each handle slot has a monotonically increasing generation counter
- A handle is valid **only if index exists AND generation matches**
- Generation increments whenever a slot is freed/reused

This prevents:
- stale handles unsubscribing new subscribers
- dispatch executing the wrong callback after reuse

---

## Subscribe Semantics

- Allocate a free handle slot
- Increment generation
- Insert handle into the event’s subscription list
- Store `(cb, user_ctx)` in the subscriber pool
- Return handle to caller

Failure cases:
- No free handle slots
- Event subscription list is full (after repair)

---

## Unsubscribe Semantics (Lazy)

- `evt_bus_unsubscribe(handle)`:
  - invalidates the subscriber pool entry
  - leaves stale handles in event lists

Rationale:
- O(1) unsubscribe
- No global scans
- Stale entries are cleaned automatically

---

## Self-Healing Strategy

Stale handles are removed automatically:

### During Dispatch
For each handle in the event’s subscription list:
- validate `(index, generation)`
- if invalid:
  - skip callback
  - clear the handle slot (self-heal)

### During Subscribe
Before inserting a new handle:
- stale entries may be reclaimed to make room

Guarantee:
- Subscription lists do not permanently fill with dead entries

---

## Payload Model (Core)

The core uses a **copy-in inline payload model**:

- Payload bytes are stored inside `evt_t`
- `payload_len == 0` means “no payload”
- `payload == NULL` is only valid when `payload_len == 0`
- Maximum payload size is compile-time bounded

Benefits:
- No lifetime issues
- Deterministic memory usage
- Simple mental model

---

## Locking Contract

The backend may optionally provide `lock` / `unlock`:

- If used, **both must be provided**
- Lock protects:
  - subscriber pool
  - subscription tables
- Dispatch:
  - snapshots callbacks + contexts under lock
  - releases lock
  - executes callbacks without holding the lock

This avoids deadlocks and minimizes contention.

---

## Threading and Safety Rules

- Callbacks always run in dispatcher context
- Publish path is thread-safe via backend queue
- Subscribe / unsubscribe must not race unless backend lock is provided
- Callbacks may safely call `unsubscribe(self)`

---

## Configuration and Limits

All limits are compile-time constants:
- `EVT_BUS_MAX_EVT_IDS`
- `EVT_BUS_MAX_HANDLES`
- `EVT_BUS_MAX_SUBSCRIBERS_PER_EVT`
- `EVT_INLINE_MAX`
- Queue depth (backend-defined)

Complexity:
- `publish()` → O(1)
- `unsubscribe()` → O(1)
- `dispatch(evt)` → O(MAX_SUBSCRIBERS_PER_EVT)

---

## Behavioral Guarantees

- No callbacks after successful unsubscribe
- Stale handles cannot affect new subscribers
- Deterministic execution time per event
- No heap allocation in core
- Platform portability

---

## Non-Goals

- Priority-based dispatch
- Wildcard or topic strings
- Dynamic resizing
- Unbounded subscribers
- Executing callbacks in ISR context

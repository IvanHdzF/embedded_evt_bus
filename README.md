# evt_bus

Deterministic, zero-heap event bus for embedded systems: **publish = enqueue**, **single-context dispatch**, handle-based subscribe/unsubscribe, portable core + optional OS ports.

---

## Description

`evt_bus` is a small pub/sub middleware intended for firmware and other resource-constrained systems.

Key properties:
- **No heap**: fixed-size tables and bounded queue
- **Deterministic** runtime: bounded fanout (`MAX_SUBS_PER_EVT`)
- **Thread-friendly**: publishers never execute callbacks
- **Safe handles**: index + generation to avoid stale-handle reuse bugs
- **Self-healing**: stale handles are cleared during dispatch/subscribe
- **Portable core**: core does not depend on any RTOS headers/types
- **Ports**: optional bindings can provide queue + dispatcher task integration (e.g. FreeRTOS)

---

## Usage

### 1) Configure `evt_bus_config.h`
You can either:
- edit the provided `include/evt_bus/evt_bus_config.h`, or
- provide your own `evt_bus_config.h` earlier in the include path (recommended), following the template.

Typical config knobs:
- `EVT_BUS_MAX_EVT`
- `EVT_BUS_MAX_HANDLES`
- `EVT_BUS_MAX_SUBS_PER_EVT`
- `EVT_BUS_QUEUE_DEPTH`
- `EVT_BUS_MAX_PAYLOAD_SIZE` (if using copy-in payloads)

### 2) Include and initialize
```c
#include "evt_bus/evt_bus.h"

void app_init(void)
{
  evt_bus_init();
}
```

### 3) Subscribe / publish
```c
static void on_my_event(evt_id_t evt, const void* payload, size_t len)
{
  (void)evt; (void)payload; (void)len;
  /* ... */
}

void app_setup(void)
{
  evt_handle_t h = evt_bus_subscribe(MY_EVT_ID, on_my_event);
  (void)h;
}

void app_do_something(void)
{
  const uint32_t x = 123;
  (void)evt_bus_publish(MY_EVT_ID, &x, sizeof(x));
}
```

### 4) Dispatch
You must run the dispatcher logic in **one dedicated context** (thread/task or main loop).

- Bare-metal / polling:
```c
for (;;)
{
  evt_bus_dispatch_all();
  /* ... other work ... */
}
```

- RTOS: create a task/thread that blocks (via port) and runs the dispatch loop.

---

## Porting OS

The project is split into:
- **Core**: `src/evt_bus_core.c` (portable, no RTOS dependencies)
- **Ports**: `ports/<os>/...` (queue backend, blocking wait, task creation convenience)

### What a port typically provides
- enqueue / dequeue backend (often a bounded queue)
- blocking wait for events (optional)
- dispatcher task/thread wrapper (optional convenience)
- ISR-safe publish helper (optional, OS dependent)
- lock/critical-section mapping (if required by your integration)

### FreeRTOS (example)
If using the FreeRTOS port, you typically:
- init the port (queue depth, task cfg)
- start the dispatcher task
- use normal `evt_bus_publish()` (and optional `publish_from_isr()`)

> See `ports/freertos/` for details.

---

## Documentation
- `docs/DESIGN.md` for architecture + semantics

---

## License
MIT
# evt_bus

Deterministic, zero-heap event bus for embedded systems: **publish = enqueue**, **single-context dispatch**, handle-based subscribe/unsubscribe, portable core + optional OS ports.

---

## Description

`evt_bus` is a small publish/subscribe middleware intended for firmware and other resource‑constrained systems.

Key properties:
- **No heap**: fixed-size tables and bounded queue
- **Deterministic runtime**: bounded fanout (`MAX_SUBS_PER_EVT`)
- **Thread-friendly**: publishers never execute callbacks
- **Safe handles**: index + generation to avoid stale-handle reuse bugs
- **Self-healing**: stale handles are cleared during dispatch/subscribe
- **Portable core**: core does not depend on any RTOS headers/types
- **Ports**: optional bindings provide queue + dispatcher task integration (e.g. FreeRTOS)

---

## Repository Structure

```
.
├── include/
│   └── evt_bus/
│       ├── evt_bus.h
│       ├── evt_bus_types.h
│       └── evt_bus_config.h
├── src/
│   └── evt_bus.c
├── ports/
│   └── freertos/        # (optional, future)
├── tests/
│   ├── test_evt_bus.c
│   ├── fake_evt_bus_backend.c
│   └── test_helpers.h
├── externals/
│   └── unity/           # Unity test framework (git submodule)
├── docs/
│   └── DESIGN.md
├── CMakeLists.txt
└── Makefile
```

---

## Configuration

### 1) Configure `evt_bus_config.h`

You can either:
- edit the provided `include/evt_bus/evt_bus_config.h`, or
- provide your own `evt_bus_config.h` earlier in the include path (recommended).

Typical configuration knobs:
- `EVT_BUS_MAX_EVT_IDS`
- `EVT_BUS_MAX_HANDLES`
- `EVT_BUS_MAX_SUBSCRIBERS_PER_EVT`
- `EVT_BUS_QUEUE_DEPTH`
- `EVT_INLINE_MAX`

---

## Basic Usage

### Initialize

```c
#include "evt_bus/evt_bus.h"

void app_init(void)
{
    evt_bus_init();
}
```

### Subscribe and Publish

```c
static void on_my_event(const evt_t *evt, void *ctx)
{
    (void)ctx;
    /* process evt->payload / evt->len */
}

void app_setup(void)
{
    evt_sub_handle_t h =
        evt_bus_subscribe(MY_EVT_ID, on_my_event, NULL);
    (void)h;
}

void app_do_something(void)
{
    const uint32_t x = 123;
    evt_bus_publish(MY_EVT_ID, &x, sizeof(x));
}
```

---

## Dispatching Events

The event bus does **not** create threads by itself.

You must run dispatching in **one dedicated context**:

- **Bare-metal / polling**:
  - dequeue events from your backend
  - call `evt_bus_dispatch_evt(&evt)`

- **RTOS**:
  - create a dispatcher task
  - block on the queue
  - call `evt_bus_dispatch_evt()` for each dequeued event

Ports may provide helpers for this (see below).

---

## OS Ports

The project is split into:
- **Core**: portable, RTOS-agnostic logic
- **Ports**: optional OS-specific bindings

A port typically provides:
- bounded queue backend
- blocking dequeue
- dispatcher task wrapper (optional)
- ISR-safe publish helper (optional)
- lock / critical-section mapping (optional)

### FreeRTOS

`ports/freertos/` provides a thin backend implementation using the FreeRTOS queue + a dispatcher task.

**Important:** this repository does **not** vendor FreeRTOS. The FreeRTOS port is intended to be built **inside a consumer project** that already has FreeRTOS configured.

Your application must provide the normal FreeRTOS include environment, including:

- `FreeRTOS.h`
- `FreeRTOSConfig.h` (application/project-specific)
- `portmacro.h` (comes from the selected FreeRTOS *portable* layer)

In other words: if your FreeRTOS app already builds, adding `evt_bus` + enabling the FreeRTOS port should “just work”.

**This repo only validates:**
- core behavior via Unity unit tests (`make test`)
- optional host compile-check of the FreeRTOS port using stub headers (no real RTOS runtime)

> See `ports/freertos/` for the port implementation.

#### Build selection (CMake)

- FreeRTOS port: build `evt_bus_port_freertos` (consumer must supply FreeRTOS headers/config)

This project intentionally does not attempt to build FreeRTOS itself.

---

## Running Tests (Unity)

### 1) Initialize submodules

Unity is included as a git submodule:

```sh
git submodule update --init --recursive
```

### 2) Build and run tests

Using the provided Makefile:

```sh
make test
```

Or manually with CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

All core behavior (subscribe, publish, dispatch, unsubscribe, self-healing)
is covered by unit tests.

---

## Documentation

- `docs/DESIGN.md` — detailed architecture, semantics, and guarantees

---

## License

MIT

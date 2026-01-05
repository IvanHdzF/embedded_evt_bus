/* ========================================================================== */
/* File: tests/test_helpers.h                                                 */
/* ========================================================================== */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "unity.h"
#include "evt_bus/evt_bus.h"            /* evt_bus_init/subscribe/publish/dispatch */
#include "evt_bus/evt_bus_config.h"     /* EVT_* limits if needed */

/* Fake backend state exposed to tests */
typedef struct {
  evt_t   last_evt;
  bool    has_evt;

  int     lock_depth;
  int     lock_calls;
  int     unlock_calls;
  int     enqueue_calls;

  bool    enqueue_ret;   /* allow forcing enqueue failure */
} fake_backend_state_t;

extern fake_backend_state_t g_fake_backend;

/* Reset both bus + backend state */
static inline void test_reset_bus(void)
{
  /* Reset fake backend */
  memset(&g_fake_backend, 0, sizeof(g_fake_backend));
  g_fake_backend.enqueue_ret = true;

  /* Reset event bus internal tables */
  evt_bus_init();
}


/* ========================================================================== */
/* File: tests/fake_evt_bus_backend.c                                         */
/* ========================================================================== */
#include <string.h>
#include "evt_bus/evt_bus.h"
#include "evt_bus/evt_bus_config.h"

#include "test_helpers.h"

/* Global fake backend instance */
fake_backend_state_t g_fake_backend;

/* Backend functions */
static bool fake_enqueue(const evt_t *evt)
{
  g_fake_backend.enqueue_calls++;

  if (!g_fake_backend.enqueue_ret) return false;
  if (!evt) return false;

  g_fake_backend.last_evt = *evt;   /* evt is POD w/ inline payload */
  g_fake_backend.has_evt = true;
  return true;
}

static bool fake_dequeue_nb(void *ctx, evt_t *evt_out)
{
  (void)ctx;
  if (!evt_out) return false;
  if (!g_fake_backend.has_evt) return false;

  *evt_out = g_fake_backend.last_evt;
  g_fake_backend.has_evt = false;
  return true;
}

static bool fake_dequeue_block(void *ctx, evt_t *evt_out)
{
  /* For host tests we don't block; behave like nb */
  return fake_dequeue_nb(ctx, evt_out);
}

static void fake_lock(void *ctx)
{
  (void)ctx;
  g_fake_backend.lock_calls++;
  g_fake_backend.lock_depth++;
}

static void fake_unlock(void *ctx)
{
  (void)ctx;
  g_fake_backend.unlock_calls++;
  g_fake_backend.lock_depth--;
}

/* This is the symbol your evt_bus_core.c expects */
evt_bus_backend_t evt_bus_backend = {
  .ctx          = NULL,
  .enqueue      = fake_enqueue,
  .dequeue_nb   = fake_dequeue_nb,
  .dequeue_block= fake_dequeue_block,
  .enqueue_isr  = NULL,
  .lock         = fake_lock,
  .unlock       = fake_unlock,
};


/* ========================================================================== */
/* File: tests/test_evt_bus.c                                                 */
/* ========================================================================== */
#include <string.h>
#include "unity.h"

#include "evt_bus/evt_bus.h"
#include "evt_bus/evt_bus_config.h"
#include "test_helpers.h"

/* ------------------------------ Test callbacks ---------------------------- */

typedef struct {
  int calls;
  int saw_lock_depth_nonzero;
  void *expected_ctx;

  evt_id_t last_id;
  uint16_t last_len;
  uint8_t  last_payload[EVT_INLINE_MAX];
} cb_probe_t;

static void cb_probe(const evt_t *evt, void *user_ctx)
{
  cb_probe_t *p = (cb_probe_t*)user_ctx;
  TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_NOT_NULL(evt);

  p->calls++;
  p->last_id  = evt->id;
  p->last_len = evt->len;

  if (evt->len > 0) {
    memcpy(p->last_payload, evt->payload, evt->len);
  }

  /* Critical property: callback must NOT execute under bus lock */
  if (g_fake_backend.lock_depth != 0) {
    p->saw_lock_depth_nonzero = 1;
  }
}

/* ------------------------------ Unity hooks ------------------------------- */

void setUp(void)    { test_reset_bus(); }
void tearDown(void) {}

/* --------------------------------- Tests --------------------------------- */

static void test_subscribe_and_dispatch_calls_cb(void)
{
  cb_probe_t probe = {0};

  evt_sub_handle_t h = evt_bus_subscribe((evt_id_t)1, cb_probe, &probe);
  TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h.id);

  /* Publish -> backend stores -> we dispatch manually */
  TEST_ASSERT_TRUE(evt_bus_publish((evt_id_t)1, NULL, 0));
  TEST_ASSERT_TRUE(g_fake_backend.has_evt);

  evt_bus_dispatch_evt(&g_fake_backend.last_evt);

  TEST_ASSERT_EQUAL_INT(1, probe.calls);
  TEST_ASSERT_EQUAL_UINT16((evt_id_t)1, probe.last_id);
  TEST_ASSERT_EQUAL_UINT16(0, probe.last_len);
  TEST_ASSERT_EQUAL_INT(0, probe.saw_lock_depth_nonzero);
}

static void test_user_ctx_is_passed(void)
{
  cb_probe_t probe = {0};

  evt_sub_handle_t h = evt_bus_subscribe((evt_id_t)2, cb_probe, &probe);
  TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h.id);

  TEST_ASSERT_TRUE(evt_bus_publish((evt_id_t)2, NULL, 0));
  evt_bus_dispatch_evt(&g_fake_backend.last_evt);

  TEST_ASSERT_EQUAL_INT(1, probe.calls);
}

static void test_payload_is_visible_to_cb(void)
{
  cb_probe_t probe = {0};
  const uint8_t payload[4] = { 0xAA, 0xBB, 0xCC, 0xDD };

  evt_sub_handle_t h = evt_bus_subscribe((evt_id_t)3, cb_probe, &probe);
  TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h.id);

  TEST_ASSERT_TRUE(evt_bus_publish((evt_id_t)3, payload, sizeof(payload)));
  evt_bus_dispatch_evt(&g_fake_backend.last_evt);

  TEST_ASSERT_EQUAL_INT(1, probe.calls);
  TEST_ASSERT_EQUAL_UINT16(sizeof(payload), probe.last_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, probe.last_payload, sizeof(payload));
}

static void test_publish_rejects_null_payload_with_nonzero_len(void)
{
  TEST_ASSERT_FALSE(evt_bus_publish((evt_id_t)1, NULL, 1));
}

static void test_publish_rejects_len_gt_inline_max(void)
{
  uint8_t big[EVT_INLINE_MAX + 1];
  memset(big, 0x11, sizeof(big));
  TEST_ASSERT_FALSE(evt_bus_publish((evt_id_t)1, big, sizeof(big)));
}

static void test_dispatch_does_not_hold_lock_during_cb(void)
{
  cb_probe_t probe = {0};

  evt_sub_handle_t h = evt_bus_subscribe((evt_id_t)4, cb_probe, &probe);
  TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h.id);

  TEST_ASSERT_TRUE(evt_bus_publish((evt_id_t)4, NULL, 0));
  evt_bus_dispatch_evt(&g_fake_backend.last_evt);

  TEST_ASSERT_EQUAL_INT(0, probe.saw_lock_depth_nonzero);
  TEST_ASSERT_TRUE(g_fake_backend.lock_calls >= 1);
  TEST_ASSERT_TRUE(g_fake_backend.unlock_calls >= 1);
}

/* Main */
int main(void)
{
  UNITY_BEGIN();

  RUN_TEST(test_subscribe_and_dispatch_calls_cb);
  RUN_TEST(test_user_ctx_is_passed);
  RUN_TEST(test_payload_is_visible_to_cb);
  RUN_TEST(test_publish_rejects_null_payload_with_nonzero_len);
  RUN_TEST(test_publish_rejects_len_gt_inline_max);
  RUN_TEST(test_dispatch_does_not_hold_lock_during_cb);

  return UNITY_END();
}

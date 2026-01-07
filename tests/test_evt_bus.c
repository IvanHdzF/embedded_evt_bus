
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

static void test_unsubscribe_stops_callback(void)
{
    cb_probe_t probe = {0};

    evt_sub_handle_t h = evt_bus_subscribe((evt_id_t)5, cb_probe, &probe);
    TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h.id);

    TEST_ASSERT_TRUE(evt_bus_publish((evt_id_t)5, NULL, 0));
    evt_bus_dispatch_evt(&g_fake_backend.last_evt);
    TEST_ASSERT_EQUAL_INT(1, probe.calls);

    evt_bus_unsubscribe(h);

    TEST_ASSERT_TRUE(evt_bus_publish((evt_id_t)5, NULL, 0));
    evt_bus_dispatch_evt(&g_fake_backend.last_evt);
    TEST_ASSERT_EQUAL_INT(1, probe.calls); /* unchanged */
}

static void test_unsubscribe_stale_handle_does_not_affect_new_sub(void)
{
    cb_probe_t probe1 = {0};
    cb_probe_t probe2 = {0};

    evt_sub_handle_t h1 = evt_bus_subscribe((evt_id_t)6, cb_probe, &probe1);
    TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h1.id);

    evt_bus_unsubscribe(h1);

    evt_sub_handle_t h2 = evt_bus_subscribe((evt_id_t)6, cb_probe, &probe2);
    TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h2.id);

    TEST_ASSERT_TRUE(evt_bus_publish((evt_id_t)6, NULL, 0));
    evt_bus_dispatch_evt(&g_fake_backend.last_evt);

    TEST_ASSERT_EQUAL_INT(0, probe1.calls);
    TEST_ASSERT_EQUAL_INT(1, probe2.calls);
}

static void test_unsubscribe_invalid_handle_safe(void)
{
    evt_sub_handle_t invalid = { .id = EVT_HANDLE_ID_INVALID, .gen = 0 };
    evt_bus_unsubscribe(invalid);
    /* Should not crash */
}

static void test_subscribe_reclaims_stale_slot(void)
{
    cb_probe_t probes[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT] = {0};
    evt_sub_handle_t hs[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT];

    const evt_id_t E = (evt_id_t)7;

    /* Fill all slots for this event */
    for (size_t i = 0; i < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; i++) {
        hs[i] = evt_bus_subscribe(E, cb_probe, &probes[i]);
        TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, hs[i].id);
    }

    /* Next subscribe should fail (no slots) */
    evt_sub_handle_t h_fail = evt_bus_subscribe(E, cb_probe, NULL);
    TEST_ASSERT_EQUAL(EVT_HANDLE_ID_INVALID, h_fail.id);

    /* Unsubscribe one (makes its slot stale) */
    evt_bus_unsubscribe(hs[0]);

    /* Now subscribe should succeed if we reclaimed the stale slot */
    cb_probe_t probe_new = {0};
    evt_sub_handle_t h_new = evt_bus_subscribe(E, cb_probe, &probe_new);
    TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h_new.id);

    /* Dispatch and ensure the new one is called */
    TEST_ASSERT_TRUE(evt_bus_publish(E, NULL, 0));
    evt_bus_dispatch_evt(&g_fake_backend.last_evt);
    TEST_ASSERT_EQUAL_INT(1, probe_new.calls);
}

static void test_dispatch_reclaims_stale_slot_then_subscribe_succeeds(void)
{
    const evt_id_t E = (evt_id_t)8;

    cb_probe_t probes[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT] = {0};
    evt_sub_handle_t hs[EVT_BUS_MAX_SUBSCRIBERS_PER_EVT];

    /* Fill all slots for E */
    for (size_t i = 0; i < EVT_BUS_MAX_SUBSCRIBERS_PER_EVT; i++) {
        hs[i] = evt_bus_subscribe(E, cb_probe, &probes[i]);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(EVT_HANDLE_ID_INVALID, hs[i].id, "fill: handle invalid");
    }

    /* Unsubscribe one -> leaves a stale slot in subscriptions[E] */
    evt_bus_unsubscribe(hs[0]);

    /* Trigger dispatch once to self-heal stale slots for E */
    TEST_ASSERT_TRUE_MESSAGE(evt_bus_publish(E, NULL, 0), "publish failed");
    evt_bus_dispatch_evt(&g_fake_backend.last_evt);

    /* Now a new subscribe should succeed if dispatch-time healing reclaimed a slot */
    cb_probe_t probe_new = {0};
    evt_sub_handle_t h_new = evt_bus_subscribe(E, cb_probe, &probe_new);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(EVT_HANDLE_ID_INVALID, h_new.id, "subscribe did not reclaim slot");

    /* And it should be callable */
    TEST_ASSERT_TRUE(evt_bus_publish(E, NULL, 0));
    evt_bus_dispatch_evt(&g_fake_backend.last_evt);
    TEST_ASSERT_EQUAL_INT(1, probe_new.calls);
}

static void test_unsubscribe_stale_handle_is_noop_and_does_not_affect_new_sub(void)
{
    cb_probe_t probe1 = {0};
    cb_probe_t probe2 = {0};

    const evt_id_t E = (evt_id_t)6;

    /* Step 1: subscribe -> h1 */
    evt_sub_handle_t h1 = evt_bus_subscribe(E, cb_probe, &probe1);
    TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h1.id);

    /* Step 2: unsubscribe -> h1 becomes stale */
    evt_bus_unsubscribe(h1);

    /* Step 3: subscribe again -> h2 */
    evt_sub_handle_t h2 = evt_bus_subscribe(E, cb_probe, &probe2);
    TEST_ASSERT_NOT_EQUAL(EVT_HANDLE_ID_INVALID, h2.id);

    /* Optional (but very useful) sanity checks:
     * - If your allocator reuses the same slot, id should match and gen should differ.
     * - If allocator does NOT reuse immediately, the core property still holds.
     */
    if (h2.id == h1.id) {
        TEST_ASSERT_NOT_EQUAL_MESSAGE(h1.gen, h2.gen, "expected generation bump on slot reuse");
    }

    /* Step 4: attempt to unsubscribe with stale handle h1:
     * MUST be NO-OP and MUST NOT unsubscribe h2.
     */
    evt_bus_unsubscribe(h1);

    /* Step 5: publish+dispatch -> probe2 must still be called */
    TEST_ASSERT_TRUE(evt_bus_publish(E, NULL, 0));
    evt_bus_dispatch_evt(&g_fake_backend.last_evt);

    TEST_ASSERT_EQUAL_INT(0, probe1.calls);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, probe2.calls, "stale unsubscribe likely nuked the new subscriber");
}


/* --------------------------------- Runner --------------------------------- */
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

    RUN_TEST(test_unsubscribe_stops_callback);
    RUN_TEST(test_unsubscribe_stale_handle_does_not_affect_new_sub);
    RUN_TEST(test_unsubscribe_invalid_handle_safe);
    RUN_TEST(test_subscribe_reclaims_stale_slot);
    RUN_TEST(test_dispatch_reclaims_stale_slot_then_subscribe_succeeds);
    RUN_TEST(test_unsubscribe_stale_handle_is_noop_and_does_not_affect_new_sub);


  return UNITY_END();
}

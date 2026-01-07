#include "evt_bus_port_freertos.h"
#include "evt_bus_port_freertos_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "evt_bus/evt_bus.h"
#include "evt_bus/evt_bus_config.h"

/* -------- Port config defaults (override via compile defs or a config header) -------- */
#ifndef EVT_BUS_FREERTOS_TASK_NAME
#define EVT_BUS_FREERTOS_TASK_NAME "evt_bus"
#endif

#ifndef EVT_BUS_FREERTOS_TASK_PRIO
#define EVT_BUS_FREERTOS_TASK_PRIO (tskIDLE_PRIORITY + 1)
#endif

#ifndef EVT_BUS_FREERTOS_STACK_WORDS
#define EVT_BUS_FREERTOS_STACK_WORDS (configMINIMAL_STACK_SIZE + 128)
#endif

#ifndef EVT_BUS_FREERTOS_QUEUE_DEPTH
#define EVT_BUS_FREERTOS_QUEUE_DEPTH 16u
#endif

/* ---- Heartbeat (port-owned) ----------------------------------------------- */

#ifndef EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS
#define EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS 0  /* 0 => disabled (block forever) */
#endif

typedef struct {
  volatile TickType_t last_beat;
  volatile uint32_t   beat_count;
  volatile uint32_t   events_dispatched;
} evt_bus_fr_hb_t;

static evt_bus_fr_hb_t s_hb;

static inline void fr_heartbeat_tick(void)
{
  s_hb.last_beat = xTaskGetTickCount();
  s_hb.beat_count++;
}

static inline void fr_heartbeat_on_dispatch(void)
{
  s_hb.events_dispatched++;
}

/* Function prototypes */
bool evt_bus_freertos_init(void);
static void evt_bus_dispatcher_task(void *arg);

/* -------- Port-owned backend state -------- */
typedef struct {
  QueueHandle_t q;
} freertos_backend_ctx_t;

static freertos_backend_ctx_t s_ctx;

/* Core references this symbol (declared extern in core .c) */
evt_bus_backend_t evt_bus_backend = {
  .ctx          = NULL,
  .enqueue      = NULL,
  .dequeue_nb   = NULL,
  .dequeue_block= NULL,
  .enqueue_isr  = NULL,
  .lock         = NULL,
  .unlock       = NULL,
  .init         = evt_bus_freertos_init,
};

/* -------- Backend function implementations -------- */

static bool fr_enqueue(const evt_t *evt)
{
  if (s_ctx.q == NULL) return false;
  /* evt_t is POD and fixed-size => send by copy */
  return (xQueueSend(s_ctx.q, evt, 0) == pdPASS);
}

static bool fr_dequeue_block(void *ctx, evt_t *evt_out)
{
  (void)ctx;
  if (s_ctx.q == NULL) return false;
  return (xQueueReceive(s_ctx.q, evt_out, pdMS_TO_TICKS(EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS)) == pdPASS);
}

static bool fr_enqueue_isr(const evt_t *evt)
{
  if (s_ctx.q == NULL) return false;

  BaseType_t hpw = pdFALSE;
  BaseType_t ok = xQueueSendFromISR(s_ctx.q, evt, &hpw);
  portYIELD_FROM_ISR(hpw);
  return (ok == pdPASS);
}


static StaticSemaphore_t s_mtx_buf;
static SemaphoreHandle_t s_mtx;

static void fr_lock(void *ctx)
{
  (void)ctx;
  (void)xSemaphoreTake(s_mtx, portMAX_DELAY);
}

static void fr_unlock(void *ctx)
{
  (void)ctx;
  (void)xSemaphoreGive(s_mtx);
}

/**
 * @brief Initialize FreeRTOS backend + create dispatcher task.
 * Returns false on queue/task creation failure.
 *
 * @return true 
 * @return false 
 */
bool evt_bus_freertos_init(void)
{
  /* Create queue before wiring backend */
  s_ctx.q = xQueueCreate((UBaseType_t)EVT_BUS_FREERTOS_QUEUE_DEPTH, (UBaseType_t)sizeof(evt_t));
  if (s_ctx.q == NULL) return false;

  s_mtx = xSemaphoreCreateMutexStatic(&s_mtx_buf);
  if (s_mtx == NULL) return false;

  /* Wire backend */
  evt_bus_backend.ctx = &s_ctx;
  evt_bus_backend.enqueue = fr_enqueue;
  evt_bus_backend.dequeue_block = fr_dequeue_block;
  evt_bus_backend.enqueue_isr = fr_enqueue_isr;


  evt_bus_backend.lock = fr_lock;
  evt_bus_backend.unlock = fr_unlock;

  /* Create dispatcher task */
  BaseType_t ok = xTaskCreate(
      evt_bus_dispatcher_task,
      EVT_BUS_FREERTOS_TASK_NAME,
      (uint16_t)EVT_BUS_FREERTOS_STACK_WORDS,
      NULL,
      (UBaseType_t)EVT_BUS_FREERTOS_TASK_PRIO,
      NULL);

  return (ok == pdPASS);
}


/* -------- Dispatcher task -------- */

static void evt_bus_dispatcher_task(void *arg)
{
  (void)arg;

  evt_t evt;

#if EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS > 0
  const TickType_t to = pdMS_TO_TICKS(EVT_BUS_FREERTOS_HEARTBEAT_TICKS_MS);

  for (;;)
  {
    /* Wake periodically to tick heartbeat even when idle */
    if (xQueueReceive(s_ctx.q, &evt, to) == pdPASS) {
      evt_bus_dispatch_evt(&evt);
      fr_heartbeat_on_dispatch();
    }
    fr_heartbeat_tick();
  }
#else
  for (;;)
  {
    /* Pure blocking, no periodic wakeups */
    if (xQueueReceive(s_ctx.q, &evt, portMAX_DELAY) == pdPASS) {
      evt_bus_dispatch_evt(&evt);
    }
  }
#endif
}

/* -------- Public port API -------- */

TickType_t evt_bus_freertos_hb_last_tick(void)      { return s_hb.last_beat; }
uint32_t   evt_bus_freertos_hb_beat_count(void)     { return s_hb.beat_count; }
uint32_t   evt_bus_freertos_hb_events_dispatched(void){ return s_hb.events_dispatched; }

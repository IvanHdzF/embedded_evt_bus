#pragma once

#include <stdint.h>


/* EVT_BUS_CONFIG_H
 *
 * Configuration options for the Event Bus.
 *
 * This file can be modified per-project to tune the event bus behavior.
 *
 */

 /* Payload is always copied into the queued event (no pointer payloads). */
#ifndef EVT_INLINE_MAX
#define EVT_INLINE_MAX 16u  /* tune based on typical payload sizes */
#endif

/* Maximum number of subscribers per event ID */
#ifndef EVT_BUS_MAX_SUBSCRIBERS_PER_EVT
#define EVT_BUS_MAX_SUBSCRIBERS_PER_EVT 10u
#endif

/* Maximum number of distinct event IDs that can be subscribed to */
#ifndef EVT_BUS_MAX_EVT_IDS
#define EVT_BUS_MAX_EVT_IDS 20u
#endif

#ifndef EVT_BUS_MAX_HANDLES
#define EVT_BUS_MAX_HANDLES (EVT_BUS_MAX_SUBSCRIBERS_PER_EVT * EVT_BUS_MAX_EVT_IDS)
#endif


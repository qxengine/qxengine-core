# QXEngine Core — API Reference: Telemetry

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

The telemetry subsystem records engine events into an append-only ring
buffer and exposes them for inspection, export, and flush. Every
significant engine operation — allocation, deallocation, law evaluation,
snapshot capture, and pressure response — produces a `QXTelemetryEvent`
that is stored automatically.

Events are never lost silently. When the ring buffer reaches capacity
(`QX_TELEMETRY_HISTORY_CAP`), the oldest event is overwritten. The
caller can drain the buffer at any time using `qx_telemetry_flush` or
copy events using `qx_telemetry_events_copy`.

---

## Headers

| Header | Purpose |
|---|---|
| `include/qxengine/telemetry/qx_telemetry_event_types.h` | Event structs, categories, severities |
| `include/qxengine/telemetry/qx_telemetry_types.h` | Config, stats structs |
| `include/qxengine/telemetry/qx_telemetry.h` | Full C ABI for telemetry |

---

## Lifecycle

### `qx_telemetry_create`

```c
QXError qx_telemetry_create(const QXTelemetryConfig* cfg,
                              QXTelemetryHandle*       out);
```

Creates a telemetry instance with the given configuration. `cfg` sets
the ring buffer capacity, minimum severity filter, and flush behaviour.
Passing NULL for `cfg` uses all defaults. Returns `QX_ERR_NULL_ARG` if
`out` is NULL. Not thread-safe.

### `qx_telemetry_destroy`

```c
void qx_telemetry_destroy(QXTelemetryHandle handle);
```

Destroys the telemetry instance and frees the ring buffer. Passing NULL
is a no-op. Not thread-safe.

---

## Record Event

### `qx_telemetry_record`

```c
QXError qx_telemetry_record(QXTelemetryHandle        handle,
                              const QXTelemetryEvent*  event);
```

Appends `event` to the ring buffer. If the buffer is full, the oldest
entry is overwritten. Events below the configured `min_severity` are
silently dropped. Returns `QX_ERR_NULL_HANDLE` if `handle` is NULL,
`QX_ERR_NULL_ARG` if `event` is NULL. Thread-safe.

---

## History Access

### `qx_telemetry_event_count`

```c
uint32_t qx_telemetry_event_count(QXTelemetryHandle handle);
```

Returns the number of events currently in the ring buffer. Returns 0
for a NULL handle. Thread-safe.

### `qx_telemetry_event_at`

```c
QXError qx_telemetry_event_at(QXTelemetryHandle  handle,
                                uint32_t           index,
                                QXTelemetryEvent*  out);
```

Copies the event at `index` into `out`. Index 0 is the oldest entry.
Returns `QX_ERR_OUT_OF_BOUNDS` if `index >= event_count`. Thread-safe.

### `qx_telemetry_events_copy`

```c
QXError qx_telemetry_events_copy(QXTelemetryHandle  handle,
                                   QXTelemetryEvent*  buf,
                                   uint32_t           buf_count,
                                   uint32_t*          copied_out);
```

Copies up to `buf_count` events into `buf`, oldest first. Writes the
actual count copied into `copied_out`. Thread-safe.

### `qx_telemetry_clear`

```c
QXError qx_telemetry_clear(QXTelemetryHandle handle);
```

Removes all events from the ring buffer. The handle remains valid and
usable after clearing. Thread-safe.

---

## Statistics

### `qx_telemetry_stats`

```c
QXError qx_telemetry_stats(QXTelemetryHandle  handle,
                             QXTelemetryStats*  out);
```

Fills `out` with lifetime counters and ring buffer statistics.
Thread-safe.

**QXTelemetryStats fields:**

| Field | Type | Description |
|---|---|---|
| `total_recorded` | `uint64_t` | Lifetime total events recorded |
| `total_dropped` | `uint64_t` | Events dropped due to severity filter |
| `total_overwritten` | `uint64_t` | Events overwritten due to full buffer |
| `warning_count` | `uint32_t` | Events with severity WARNING |
| `error_count` | `uint32_t` | Events with severity ERROR |
| `fatal_count` | `uint32_t` | Events with severity FATAL |
| `current_count` | `uint32_t` | Events currently in ring buffer |
| `capacity` | `uint32_t` | Ring buffer capacity |
| `oldest_timestamp_ms` | `uint64_t` | Timestamp of oldest event in buffer |
| `latest_timestamp_ms` | `uint64_t` | Timestamp of most recent event |

---

## Flush

### `qx_telemetry_flush`

```c
QXError qx_telemetry_flush(QXTelemetryHandle handle);
```

Signals the telemetry subsystem to flush buffered events to any
registered sink (file, network, OS logging). If no sink is configured,
flush is a no-op that returns `QX_OK`. Thread-safe.

---

## Query Helpers

### `qx_telemetry_is_enabled`

```c
bool qx_telemetry_is_enabled(QXTelemetryHandle handle);
```

Returns `true` if the telemetry instance is active and accepting events.
Returns `false` for a NULL handle. Thread-safe.

### `qx_telemetry_capacity`

```c
uint32_t qx_telemetry_capacity(QXTelemetryHandle handle);
```

Returns the ring buffer capacity set at create time. Returns 0 for a
NULL handle. Thread-safe.

---

## QXTelemetryEvent Fields

| Field | Type | Description |
|---|---|---|
| `severity` | `QXTelemetrySeverity` | INFO, WARNING, ERROR, or FATAL |
| `category` | `QXTelemetryCategory` | Subsystem that produced the event |
| `timestamp_ms` | `uint64_t` | Event timestamp in milliseconds |
| `message` | `char[]` | Human-readable description (max `QX_TEL_MESSAGE_MAX`) |
| `segment_id` | `uint8_t` | Relevant segment (0–8), or 255 if N/A |
| `bytes` | `uint64_t` | Bytes involved (alloc size, bytes reclaimed, etc.) |
| `law_id` | `uint8_t` | Relevant law (0–7), or 255 if N/A |
| `health_score` | `double` | Health score at time of event, or -1.0 if N/A |

---

## Event Categories

| Constant | Description |
|---|---|
| `QX_TEL_CAT_MEMORY` | Allocation, deallocation, eviction events |
| `QX_TEL_CAT_LAW` | Law evaluation and violation events |
| `QX_TEL_CAT_SNAPSHOT` | Cognitive snapshot capture events |
| `QX_TEL_CAT_MANIFEST` | Manifest parse and validation events |
| `QX_TEL_CAT_PRESSURE` | Memory pressure and OS signal events |
| `QX_TEL_CAT_ENGINE` | Engine lifecycle events |

---

## Severity Levels

| Constant | Description |
|---|---|
| `QX_TEL_SEV_INFO` | Routine operational event |
| `QX_TEL_SEV_WARNING` | Degraded condition, action recommended |
| `QX_TEL_SEV_ERROR` | Recoverable error occurred |
| `QX_TEL_SEV_FATAL` | Unrecoverable condition, engine may halt |

---

## Common Patterns

**Recording a custom event:**

```c
QXTelemetryEvent ev;
memset(&ev, 0, sizeof(ev));
ev.severity     = QX_TEL_SEV_WARNING;
ev.category     = QX_TEL_CAT_MEMORY;
ev.timestamp_ms = qx_now_ms();
strncpy(ev.message, "segment.cache near soft limit",
        QX_TEL_MESSAGE_MAX - 1);
qx_telemetry_record(telemetry, &ev);
```

**Draining events to a log file:**

```c
uint32_t count = qx_telemetry_event_count(telemetry);
QXTelemetryEvent* buf = malloc(count * sizeof(QXTelemetryEvent));
uint32_t copied = 0;
qx_telemetry_events_copy(telemetry, buf, count, &copied);
for (uint32_t i = 0; i < copied; ++i) {
    fprintf(log, "[%llu] [%d] %s\n",
            buf[i].timestamp_ms,
            buf[i].severity,
            buf[i].message);
}
free(buf);
qx_telemetry_clear(telemetry);
```

---

## Related Documents

- [`../architecture/overview.md`](../architecture/overview.md) — Where telemetry fits in the engine
- [`core.md`](core.md) — Error codes reference
- [`law.md`](law.md) — Law evaluation events
- [`../contributing/testing-guide.md`](../contributing/testing-guide.md) — Testing telemetry

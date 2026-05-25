# QXEngine Core — API Reference: Intelligence

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

The intelligence subsystem computes a **knowledge score** (0 – 100) by
capturing `QXCognitiveSnapshot` records at configurable intervals and
storing them in a ring-buffer history. Each snapshot aggregates five
weighted cognitive signals into a single score. The knowledge score
feeds directly into **Law 4** of the constitutional law enforcer.

In normal usage, callers invoke `qx_engine_snapshot` which delegates
to this subsystem internally. The functions documented here are for
consumers who need direct access to snapshot history or signal
computation.

---

## Headers

| Header | Purpose |
|---|---|
| `include/qxengine/intelligence/qx_snapshot_types.h` | All snapshot structs and constants |
| `include/qxengine/intelligence/qx_snapshot.h` | Full C ABI for snapshot history |

---

## Lifecycle

### `qx_snapshot_history_create`

```c
QXError qx_snapshot_history_create(QXSnapshotHistoryHandle* out);
```

Creates a new snapshot history ring buffer with capacity
`QX_SNAPSHOT_HISTORY_MAX` (720 entries, approximately 60 minutes at a
5-second capture interval). Returns `QX_ERR_NULL_ARG` if `out` is NULL.
Not thread-safe.

### `qx_snapshot_history_destroy`

```c
void qx_snapshot_history_destroy(QXSnapshotHistoryHandle handle);
```

Destroys the history and frees all internal memory. Passing NULL is a
no-op. Not thread-safe.

---

## Capture

### `qx_snapshot_capture`

```c
QXError qx_snapshot_capture(QXSnapshotHistoryHandle handle,
                              const QXSnapshotInput* input,
                              QXCognitiveSnapshot* out);
```

Computes all five cognitive signals from `input`, derives the knowledge
score, appends the snapshot to the history ring, and writes the result
into `out`. If the ring is full, the oldest entry is overwritten.
Thread-safe.

Returns `QX_ERR_NULL_HANDLE` if `handle` is NULL, `QX_ERR_NULL_ARG` if
`input` or `out` is NULL.

**QXSnapshotInput required fields:**

| Field | Description |
|---|---|
| `segment_count` | Must equal `QX_SEGMENT_COUNT` (9) |
| `segment_used_bytes[]` | Used bytes per segment |
| `segment_soft_limits[]` | Soft limit per segment |
| `segment_pressure_tier[]` | Current pressure tier per segment |
| `composite_pressure_tier` | Highest tier across all segments |
| `latest_law_health_score` | Most recent law health score |
| `compliance_streak` | Consecutive compliant evaluations |
| `native_coverage_ratio` | Ratio of native to total allocations |
| `eval_timestamp_ms` | Current evaluation timestamp |
| `prev_timestamp_ms` | Previous capture timestamp |
| `knowledge_score` | Pre-computed score (or 0 to recompute) |

---

## History Access

### `qx_snapshot_count`

```c
uint32_t qx_snapshot_count(QXSnapshotHistoryHandle handle);
```

Returns the number of snapshots currently in the ring buffer. Returns 0
for a NULL handle. Thread-safe.

### `qx_snapshot_at`

```c
QXError qx_snapshot_at(QXSnapshotHistoryHandle handle,
                        uint32_t index,
                        QXCognitiveSnapshot* out);
```

Copies the snapshot at `index` into `out`. Index 0 is the oldest entry.
Returns `QX_ERR_OUT_OF_BOUNDS` if `index >= snapshot_count`. Thread-safe.

### `qx_snapshot_latest`

```c
QXError qx_snapshot_latest(QXSnapshotHistoryHandle handle,
                             QXCognitiveSnapshot* out);
```

Copies the most recently captured snapshot into `out`. Returns
`QX_ERR_NOT_FOUND` if no snapshots have been captured. Thread-safe.

### `qx_snapshot_copy_all`

```c
QXError qx_snapshot_copy_all(QXSnapshotHistoryHandle handle,
                               QXCognitiveSnapshot* buf,
                               uint32_t buf_count,
                               uint32_t* copied_out);
```

Copies up to `buf_count` snapshots into `buf`, oldest first. Writes the
actual number copied into `copied_out`. Thread-safe.

### `qx_snapshot_find_non_compliant`

```c
QXError qx_snapshot_find_non_compliant(QXSnapshotHistoryHandle handle,
                                        QXCognitiveSnapshot* out);
```

Finds and copies the most recent snapshot where `is_law5_compliant` is
`false`. Returns `QX_ERR_NOT_FOUND` if all snapshots are compliant.
Thread-safe.

### `qx_snapshot_clear`

```c
QXError qx_snapshot_clear(QXSnapshotHistoryHandle handle);
```

Removes all snapshots from the ring buffer. The handle remains valid
and usable after clearing. Thread-safe.

---

## Statistics

### `qx_snapshot_history_stats`

```c
QXError qx_snapshot_history_stats(QXSnapshotHistoryHandle handle,
                                   QXSnapshotHistoryStats* out);
```

Fills `out` with aggregate statistics. Thread-safe.

**QXSnapshotHistoryStats fields:**

| Field | Type | Description |
|---|---|---|
| `snapshot_count` | `uint32_t` | Entries currently in the ring |
| `total_captured` | `uint64_t` | Lifetime total captures |
| `peak_score` | `double` | Highest knowledge score recorded |
| `trough_score` | `double` | Lowest knowledge score recorded |
| `mean_score` | `double` | Running mean knowledge score |
| `last_score` | `double` | Most recent knowledge score |
| `non_compliant_count` | `uint32_t` | Snapshots where Law 5 failed |
| `oldest_timestamp_ms` | `uint64_t` | Timestamp of oldest entry |
| `latest_timestamp_ms` | `uint64_t` | Timestamp of most recent entry |
| `seconds_since_last` | `double` | Seconds since last capture |

---

## Signal Helper Functions

The following helpers compute individual cognitive signal values. They
are used internally by `qx_snapshot_capture` and exposed for testing
and diagnostics.

### `qx_snapshot_clarity_for_utilisation`

```c
double qx_snapshot_clarity_for_utilisation(double utilisation_ratio);
```

Maps a segment utilisation ratio (0.0 – 1.0) to a memory clarity signal
(0.0 – 1.0). Higher utilisation produces lower clarity. Returns 1.0 for
zero utilisation and approaches 0.0 as utilisation approaches 1.0.

### `qx_snapshot_recency_signal`

```c
double qx_snapshot_recency_signal(double seconds_since_previous);
```

Computes a recency signal from the seconds elapsed since the previous
capture. Uses an exponential decay with time constant
`QX_SNAPSHOT_RECENCY_DECAY_SEC` (10.0 seconds). Returns 1.0 for
`seconds_since_previous = 0` and decays toward 0.0 for large values.

### `qx_snapshot_compliance_signal`

```c
double qx_snapshot_compliance_signal(double law_health_score,
                                      uint32_t compliance_streak);
```

Combines the current law health score and compliance streak into a
single compliance signal (0.0 – 1.0). A health score ≥ 90.0 with a
streak ≥ 10 produces the maximum signal of 1.0.

---

## Cognitive Signal Weights

The five signals and their fixed weights are defined in
`qx_snapshot_types.h` as `QX_SNAPSHOT_SIGNAL_WEIGHTS`.

| Signal | Index | Weight | Description |
|---|---|---|---|
| Memory clarity | 0 | 0.25 | Mean clarity across all segments |
| Pressure stability | 1 | 0.20 | Inverse of composite pressure tier |
| Law compliance | 2 | 0.25 | Derived from health score and streak |
| Native coverage | 3 | 0.15 | Native allocation coverage ratio |
| Recency | 4 | 0.15 | Exponential decay since last capture |

The knowledge score formula is:

$$\text{knowledge\_score} = 100 \times \sum_{i=0}^{4} w_i \times s_i$$

Where $$w_i$$ is the signal weight and $$s_i$$ is the signal value in
[0.0, 1.0]. Weights sum to exactly 1.0.

---

## Law 5 Compliance Thresholds

A snapshot sets `is_law5_compliant = true` when
`knowledge_score >= QX_SNAPSHOT_MIN_SCORE` (90.0).

| Score range | Compliance | Severity fed to Law 4 |
|---|---|---|
| ≥ 90.0 | ✅ Compliant | — |
| 70.0 – 89.9 | ❌ Non-compliant | WARNING |
| 50.0 – 69.9 | ❌ Non-compliant | CRITICAL |
| < 50.0 | ❌ Non-compliant | FATAL |

---

## Common Patterns

**Capturing a snapshot after law evaluation:**

```c
QXCognitiveSnapshot snap;
memset(&snap, 0, sizeof(snap));
QXError err = qx_engine_snapshot(engine, &snap);
if (err == QX_OK) {
    printf("knowledge_score=%.2f compliant=%d\n",
           snap.knowledge_score, snap.is_law5_compliant);
}
```

**Retrieving the most recent non-compliant snapshot:**

```c
QXCognitiveSnapshot snap;
memset(&snap, 0, sizeof(snap));
QXError err = qx_snapshot_find_non_compliant(history, &snap);
if (err == QX_OK) {
    /* snap contains the most recent failing snapshot */
}
```

---

## Related Documents

- [`../architecture/constitutional-laws.md`](../architecture/constitutional-laws.md) — Law 4 and Law 5 details
- [`../architecture/memory-model.md`](../architecture/memory-model.md) — Memory clarity signal source
- [`law.md`](law.md) — Law enforcer API reference
- [`core.md`](core.md) — Error codes reference

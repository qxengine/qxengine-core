# QXEngine Core — API Reference: Memory

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

The memory subsystem implements the three-tier leaf → segment → memloc
hierarchy. All allocation, deallocation, eviction, and pressure
management functions are declared here. In normal usage, callers do not
call these functions directly — they call `qx_engine_alloc` and
`qx_engine_free` which delegate internally. These APIs are documented
for platform bridge authors and advanced consumers.

---

## Headers

| Header | Purpose |
|---|---|
| `include/qxengine/memory/qx_leaf.h` | Leaf lifecycle and state transitions |
| `include/qxengine/memory/qx_segment.h` | Segment allocation and stats |
| `include/qxengine/memory/qx_memloc.h` | Multi-segment coordinator |
| `include/qxengine/memory/qx_pressure.h` | Pressure evaluation and OS signals |

---

## QXLeaf API

### `qx_leaf_create`

```c
QXError qx_leaf_create(const QXLeafConfig* cfg, QXLeafHandle* out);
```

Creates a new leaf with the given configuration. The caller must supply
a non-blank label, a non-zero size, a valid segment ID, and an eviction
class. Returns `QX_ERR_NULL_ARG` if `cfg` or `out` is NULL.

### `qx_leaf_destroy`

```c
void qx_leaf_destroy(QXLeafHandle handle);
```

Frees the leaf handle. Passing NULL is a no-op. The leaf must be in
`RELEASED` state before destroy; calling destroy on an `ACTIVE` leaf
is undefined behaviour.

### `qx_leaf_transition`

```c
QXError qx_leaf_transition(QXLeafHandle handle, QXLeafState new_state);
```

Transitions the leaf to a new state. Valid transitions are
`ACTIVE → PROTECTED`, `PROTECTED → ACTIVE`, and `ACTIVE → RELEASED`.
Returns `QX_ERR_INVALID_STATE` for any other transition.

### `qx_leaf_touch`

```c
QXError qx_leaf_touch(QXLeafHandle handle);
```

Updates `last_touch_ms` and increments `touch_count`. Used by the
eviction subsystem to implement LRU ordering. Returns
`QX_ERR_INVALID_STATE` if the leaf is `RELEASED`.

### `qx_leaf_info`

```c
QXError qx_leaf_info(QXLeafHandle handle, QXLeafInfo* out);
```

Fills `out` with the leaf's current label, size, state, eviction class,
touch count, and timestamps. Thread-safe.

### `qx_leaf_is_evictable`

```c
bool qx_leaf_is_evictable(QXLeafHandle handle, QXTierId tier);
```

Returns `true` if the leaf's eviction class permits eviction at the
given pressure tier. Always returns `false` for class A leaves and
for `RELEASED` or `PROTECTED` leaves.

---

## QXSegment API

### `qx_segment_stats`

```c
QXError qx_segment_stats(QXSegmentHandle handle, QXSegmentStats* out);
```

Fills `out` with the segment's current used bytes, soft and hard
limits, allocation and deallocation counts, pairs ratio, pressure tier,
orphaned bytes, and eviction candidate count. Thread-safe.

### `qx_segment_has_capacity`

```c
bool qx_segment_has_capacity(QXSegmentHandle handle, size_t size_bytes);
```

Returns `true` if allocating `size_bytes` would not exceed the segment's
hard limit. Thread-safe.

### `qx_segment_over_soft_limit`

```c
bool qx_segment_over_soft_limit(QXSegmentHandle handle);
```

Returns `true` if `used_bytes > soft_limit_bytes`. Thread-safe.

### `qx_segment_over_hard_limit`

```c
bool qx_segment_over_hard_limit(QXSegmentHandle handle);
```

Returns `true` if `used_bytes >= hard_limit_bytes`. Thread-safe.

### `qx_segment_pressure_tier`

```c
QXTierId qx_segment_pressure_tier(QXSegmentHandle handle);
```

Returns the current pressure tier for this segment. Thread-safe.

### `qx_segment_eviction_candidate_count`

```c
uint32_t qx_segment_eviction_candidate_count(QXSegmentHandle handle,
                                              QXTierId tier);
```

Returns the number of leaves eligible for eviction at the given tier.

### `qx_segment_flush_terminated`

```c
QXError qx_segment_flush_terminated(QXSegmentHandle handle);
```

Removes all `RELEASED` leaves from the slot array, reclaiming their
slots for new allocations. Thread-safe.

---

## QXMemloc API

### `qx_memloc_create`

```c
QXError qx_memloc_create(const QXMemlocConfig* cfg, QXMemlocHandle* out);
```

Creates a memloc with nine segments configured from `cfg`. Limits are
derived from the manifest soft and hard limits multiplied by each
segment's `budget_percent`.

### `qx_memloc_destroy`

```c
void qx_memloc_destroy(QXMemlocHandle handle);
```

Destroys the memloc and all nine segments. Passing NULL is a no-op.

### `qx_memloc_alloc`

```c
QXError qx_memloc_alloc(QXMemlocHandle handle, uint8_t segment_id,
                         size_t size_bytes, const char* label,
                         QXEvictClass evict_class, QXLeafHandle* out);
```

Routes the allocation to the correct segment. Returns
`QX_ERR_LIMIT_EXCEEDED` if the segment's hard limit would be breached.

### `qx_memloc_free`

```c
QXError qx_memloc_free(QXMemlocHandle handle, QXLeafHandle leaf);
```

Marks the leaf as `RELEASED` and updates the segment's `used_bytes`.

### `qx_memloc_total_used_bytes`

```c
uint64_t qx_memloc_total_used_bytes(QXMemlocHandle handle);
```

Returns the sum of `used_bytes` across all nine segments. Thread-safe.

### `qx_memloc_law_input`

```c
QXError qx_memloc_law_input(QXMemlocHandle handle,
                             QXLawEvaluationInput* out);
```

Builds a `QXLawEvaluationInput` snapshot from the current memloc state.
Called internally by `qx_engine_evaluate`. Thread-safe.

---

## QXPressure API

### `qx_pressure_create`

```c
QXError qx_pressure_create(QXMemlocHandle memloc,
                            QXPressureHandle* out);
```

Creates a pressure coordinator bound to the given memloc.

### `qx_pressure_destroy`

```c
void qx_pressure_destroy(QXPressureHandle handle);
```

Destroys the coordinator. Passing NULL is a no-op.

### `qx_pressure_evaluate`

```c
QXError qx_pressure_evaluate(QXPressureHandle handle,
                              QXTierId tier,
                              QXPressureEvent* out);
```

Evaluates pressure at the given tier, runs an eviction pass if needed,
and fills `out` with the event record. Thread-safe.

### `qx_pressure_handle_android_trim`

```c
QXError qx_pressure_handle_android_trim(QXPressureHandle handle,
                                         int trim_level);
```

Maps an Android `onTrimMemory` level to a pressure tier and triggers
an eviction pass. Trim level 5 → tier 2, level 10 → tier 3,
level 15 → tier 4.

### `qx_pressure_handle_ios_warning`

```c
QXError qx_pressure_handle_ios_warning(QXPressureHandle handle,
                                        double ram_used_ratio);
```

Maps an iOS memory warning to a pressure tier based on `ram_used_ratio`.
Ratio ≥ 0.85 → tier 4, otherwise → tier 3.

### `qx_pressure_stats`

```c
QXError qx_pressure_stats(QXPressureHandle handle,
                           QXPressureStats* out);
```

Fills `out` with lifetime eviction counts, peak tier, bytes reclaimed,
and event history statistics. Thread-safe.

---

## Common Patterns

**Checking capacity before allocating:**

```c
if (qx_segment_has_capacity(seg, size)) {
    qx_memloc_alloc(memloc, segment_id, size, label,
                    QX_EVICT_CLASS_C, &leaf);
}
```

**Responding to Android memory pressure:**

```c
/* In onTrimMemory callback */
qx_engine_pressure_trim(engine, trim_level);
```

**Responding to iOS memory warning:**

```c
/* In didReceiveMemoryWarning */
qx_engine_pressure_ios(engine, ram_used_ratio);
```

---

## Related Documents

- [`../architecture/memory-model.md`](../architecture/memory-model.md) — Memory model internals
- [`core.md`](core.md) — Core types and error codes
- [`law.md`](law.md) — How memory data feeds law evaluation
- [`../integration/android-ndk.md`](../integration/android-ndk.md) — Android integration guide
- [`../integration/ios-xcframework.md`](../integration/ios-xcframework.md) — iOS integration guide

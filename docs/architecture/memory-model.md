# QXEngine Core — Memory Model

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

QXEngine organises all managed memory into a strict three-tier hierarchy:
**Leaf → Segment → Memloc**. Every allocation is a leaf. Leaves belong
to exactly one segment. All nine segments are owned by one memloc.
Nothing is allocated outside this hierarchy.

---

## Three-Tier Hierarchy

```
QXMemloc  (one per engine)
│
├── Segment 0  – CORE
├── Segment 1  – MODEL
├── Segment 2  – CACHE
├── Segment 3  – SESSION
├── Segment 4  – ETHICS
├── Segment 5  – TELEMETRY
├── Segment 6  – SNAPSHOT
├── Segment 7  – MANIFEST
└── Segment 8  – SCRATCH
         │
         ├── Leaf [slot 0]  label="model.weights"  size=4MiB  class=B
         ├── Leaf [slot 1]  label="cache.hot"      size=512KiB class=C
         └── Leaf [slot N]  ...
```

Each segment has a fixed slot array. The slot count per segment is
defined by `QX_SEGMENT_SLOT_CAP` at compile time. A segment can hold
at most `QX_SEGMENT_SLOT_CAP` live leaves simultaneously.

---

## QXLeaf

A `QXLeaf` is the atomic unit of managed memory. It represents a single
named allocation with a defined eviction policy.

| Field | Type | Description |
|---|---|---|
| `label` | `char[]` | UTF-8 name, max `QX_LEAF_LABEL_MAX` chars |
| `size_bytes` | `size_t` | Allocation size in bytes |
| `state` | `QXLeafState` | `ACTIVE`, `PROTECTED`, or `RELEASED` |
| `evict_class` | `QXEvictClass` | Eviction eligibility class (A–D) |
| `touch_count` | `uint32_t` | Number of times the leaf has been touched |
| `last_touch_ms` | `uint64_t` | Timestamp of most recent touch |
| `created_ms` | `uint64_t` | Creation timestamp |

Leaves are created by `qx_engine_alloc` and freed by `qx_engine_free`.
A leaf transitions through states via `qx_leaf_transition`. Once a leaf
reaches `RELEASED` it cannot be reactivated.

---

## QXSegment

A `QXSegment` owns a fixed slot array and enforces two memory limits:
a soft limit (warning threshold) and a hard limit (absolute ceiling).

| Property | Description |
|---|---|
| `segment_id` | Unique ID 0–8, maps to a semantic role |
| `soft_limit_bytes` | Derived from `budget_percent` × `soft_limit_mb` |
| `hard_limit_bytes` | Derived from `budget_percent` × `hard_limit_mb` |
| `used_bytes` | Sum of all active leaf sizes in this segment |
| `pressure_tier` | Current tier 1–4 based on utilisation ratio |
| `pairs_ratio` | `dealloc_count / alloc_count` — balance health |

Allocations that would push `used_bytes` above `hard_limit_bytes` are
rejected with `QX_ERR_LIMIT_EXCEEDED`. Allocations that push above
`soft_limit_bytes` succeed but raise the pressure tier.

---

## QXMemloc

The `QXMemloc` is the single owner of all nine segments. It provides
the aggregate view used by the law enforcer and snapshot subsystems.

Key responsibilities of `QXMemloc`:

- Routes `alloc` and `free` calls to the correct segment by `segment_id`.
- Aggregates `total_used_bytes` across all segments.
- Builds a `QXLawEvaluationInput` on demand for the law enforcer.
- Reports the unlabelled allocation count for Law 0 (Pattern Integrity).

---

## Segment Identity Table

| ID | Name | Role |
|---|---|---|
| 0 | CORE | Engine internals, bootstrap allocations |
| 1 | MODEL | AI model weights and parameters |
| 2 | CACHE | Hot-path inference cache |
| 3 | SESSION | Per-session user context |
| 4 | ETHICS | Ethics and bias-detection buffers |
| 5 | TELEMETRY | Event ring buffers |
| 6 | SNAPSHOT | Cognitive snapshot history |
| 7 | MANIFEST | Parsed manifest data |
| 8 | SCRATCH | Temporary / ephemeral allocations |

Each segment's memory budget is expressed as a `budget_percent` in the
manifest. All nine `budget_percent` values must sum to exactly 100.

---

## Pressure Tiers

The pressure coordinator assigns each segment a tier based on its
utilisation ratio (`used_bytes / hard_limit_bytes`).

| Tier | Utilisation | Meaning |
|---|---|---|
| 1 | < 50 % | Normal — no action |
| 2 | 50 – 74 % | Elevated — monitor |
| 3 | 75 – 89 % | High — evict class D |
| 4 | ≥ 90 % | Critical — evict classes C and D |

The composite pressure tier across all segments is the maximum tier of
any individual segment. OS signals (Android `onTrimMemory`, iOS memory
warning) can force the tier to 3 or 4 regardless of utilisation.

---

## Eviction Classes

Every leaf is assigned an eviction class at allocation time. The class
determines at which pressure tier the leaf becomes eligible for eviction.

| Class | Evictable at tier | Typical use |
|---|---|---|
| A | Never | Critical engine state, manifest data |
| B | 4 only | Model weights, long-lived session data |
| C | 3 and 4 | Inference cache, snapshot buffers |
| D | 2, 3, and 4 | Scratch allocations, ephemeral results |

The eviction pass selects the highest-tier eligible leaves first, sorted
by last-touch timestamp ascending (least recently used first).

---

## Allocation Lifecycle

```
qx_engine_alloc()
    │
    ├─ validate label, size, segment_id, evict_class
    ├─ check segment hard limit → reject if exceeded
    ├─ find free slot in segment
    ├─ create QXLeaf (state = ACTIVE)
    ├─ update segment.used_bytes
    ├─ record telemetry event
    └─ return QXLeafHandle

qx_engine_free()
    │
    ├─ validate handle
    ├─ transition leaf state → RELEASED
    ├─ update segment.used_bytes
    ├─ record telemetry event
    └─ clear slot
```

---

## Limits and Budgets

Memory limits flow from the manifest into the engine at create time and
are immutable thereafter.

$$\text{segment\_soft\_limit} = \frac{\text{budget\_percent}}{100} \times \text{soft\_limit\_mb} \times 1048576$$

$$\text{segment\_hard\_limit} = \frac{\text{budget\_percent}}{100} \times \text{hard\_limit\_mb} \times 1048576$$

The manifest enforces `soft_limit_mb >= 64` and
`hard_limit_mb >= 128` and `hard_limit_mb > soft_limit_mb` via MFT
rules 0005 and 0006.

---

## Related Documents

- [`overview.md`](overview.md) — High-level architecture
- [`constitutional-laws.md`](constitutional-laws.md) — How memory data feeds the law evaluator
- [`../api/memory.md`](../api/memory.md) — Memory subsystem API reference
- [`../integration/android-ndk.md`](../integration/android-ndk.md) — Android memory pressure integration

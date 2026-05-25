# QXEngine Core — API Reference: Core

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

The core module defines the fundamental types, constants, error codes,
and version information shared by every other QXEngine subsystem. No
subsystem header can be included without pulling in the core types.

---

## Headers

| Header | Purpose |
|---|---|
| `include/qxengine/core/qx_types.h` | All primitive types and enums |
| `include/qxengine/core/qx_constants.h` | Compile-time constants |
| `include/qxengine/core/qx_error.h` | Error codes and string helpers |
| `include/qxengine/core/qx_version.h` | Version macros (CMake-generated) |

---

## QXError

Returned by every public QXEngine function. Always check the return
value before using any output parameter.

| Code | Value | Meaning |
|---|---|---|
| `QX_OK` | 0 | Success |
| `QX_ERR_NULL_HANDLE` | 1 | A required handle was NULL |
| `QX_ERR_NULL_ARG` | 2 | A required pointer argument was NULL |
| `QX_ERR_INVALID_ARG` | 3 | An argument value is out of range |
| `QX_ERR_LIMIT_EXCEEDED` | 4 | Memory hard limit would be breached |
| `QX_ERR_NOT_INITIALISED` | 5 | Engine was not successfully created |
| `QX_ERR_ALREADY_EXISTS` | 6 | Duplicate label or handle |
| `QX_ERR_NOT_FOUND` | 7 | Requested item does not exist |
| `QX_ERR_OUT_OF_BOUNDS` | 8 | Index exceeds valid range |
| `QX_ERR_INVALID_STATE` | 9 | Operation invalid for current state |
| `QX_ERR_PARSE_FAILED` | 10 | JSON or manifest parse error |
| `QX_ERR_VALIDATION_FAILED` | 11 | Manifest validation rule violated |
| `QX_ERR_ALLOC_FAILED` | 12 | Internal memory allocation failed |

---

## QXTierId — Pressure Tiers

| Value | Constant | Utilisation range |
|---|---|---|
| 1 | `QX_TIER_1` | < 50 % — Normal |
| 2 | `QX_TIER_2` | 50 – 74 % — Elevated |
| 3 | `QX_TIER_3` | 75 – 89 % — High |
| 4 | `QX_TIER_4` | ≥ 90 % — Critical |

---

## QXEvictClass — Eviction Classes

| Value | Constant | Evictable at tier |
|---|---|---|
| 0 | `QX_EVICT_CLASS_A` | Never |
| 1 | `QX_EVICT_CLASS_B` | Tier 4 only |
| 2 | `QX_EVICT_CLASS_C` | Tier 3 and 4 |
| 3 | `QX_EVICT_CLASS_D` | Tier 2, 3, and 4 |

---

## QXLeafState — Leaf States

| Value | Constant | Meaning |
|---|---|---|
| 0 | `QX_LEAF_ACTIVE` | Allocation is live and usable |
| 1 | `QX_LEAF_PROTECTED` | Temporarily shielded from eviction |
| 2 | `QX_LEAF_RELEASED` | Freed — handle is no longer valid |

State transitions are strictly one-directional:
`ACTIVE → PROTECTED → ACTIVE → RELEASED`. A released leaf
cannot be reactivated.

---

## QXCertificationTier — Certification Tiers

| Value | Constant | Min health score |
|---|---|---|
| 0 | `QX_CERT_UNCERTIFIED` | 0.0 |
| 1 | `QX_CERT_PROVISIONAL` | 60.0 |
| 2 | `QX_CERT_STANDARD` | 75.0 |
| 3 | `QX_CERT_ADVANCED` | 88.0 |
| 4 | `QX_CERT_SOVEREIGN` | 96.0 |

---

## Compile-Time Constants

| Constant | Value | Meaning |
|---|---|---|
| `QX_SEGMENT_COUNT` | 9 | Number of memory segments |
| `QX_LAW_COUNT` | 8 | Number of constitutional laws |
| `QX_LEAF_LABEL_MAX` | 128 | Maximum label length (bytes, incl. null) |
| `QX_ABI_VERSION` | 1 | Current C ABI version |
| `QX_ENFORCER_HISTORY_CAP` | 1000 | Max law report history entries |
| `QX_SNAPSHOT_HISTORY_MAX` | 720 | Max snapshot history entries |
| `QX_MANIFEST_MAX_ERRORS` | 32 | Max manifest validation errors |
| `QX_TELEMETRY_HISTORY_CAP` | — | Defined in `qx_telemetry_types.h` |

---

## Version Functions

Declared in `qx_version.h` (CMake-generated) and `qxengine.h`.

### `qx_engine_version`

```c
const char* qx_engine_version(void);
```

Returns the full version string, e.g. `"1.0.0"`. The returned pointer
is valid for the lifetime of the process.

### `qx_engine_build_date`

```c
const char* qx_engine_build_date(void);
```

Returns the build date string in `YYYY-MM-DD` format.

### `qx_engine_is_initialised`

```c
bool qx_engine_is_initialised(QXEngineHandle handle);
```

Returns `true` if the engine handle was successfully created and has
not been destroyed. Returns `false` for a NULL handle.

---

## Error Helper Functions

Declared in `qx_error.h`.

### `qx_error_string`

```c
const char* qx_error_string(QXError err);
```

Returns a human-readable string for the given error code, e.g.
`"QX_ERR_NULL_HANDLE"`. Returns `"QX_ERR_UNKNOWN"` for unrecognised
values. The returned pointer is a string literal — do not free it.

### `qx_error_is_fatal`

```c
bool qx_error_is_fatal(QXError err);
```

Returns `true` if the error code represents an unrecoverable condition
(e.g. `QX_ERR_NOT_INITIALISED`, `QX_ERR_ALLOC_FAILED`).

---

## Related Documents

- [`../architecture/overview.md`](../architecture/overview.md) — Architecture overview
- [`../architecture/c-abi-boundary.md`](../architecture/c-abi-boundary.md) — ABI rules
- [`memory.md`](memory.md) — Memory subsystem API reference
- [`law.md`](law.md) — Law enforcer API reference

# QXEngine Core — C ABI Boundary

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Why a C ABI?

QXEngine is written in C++17 internally, but every public symbol is
declared `extern "C"`. This deliberate constraint means:

- Swift can call QXEngine directly via a bridging header with no ObjC runtime.
- Kotlin can call QXEngine via JNI without any C++ name-mangling issues.
- Python can bind QXEngine via `ctypes` or `cffi` without a compiled wrapper.
- Any future language with a C FFI can consume the engine without recompilation.

A C++ ABI would tie every consumer to the same compiler, version, and
standard library. The C ABI boundary eliminates that dependency entirely.

---

## ABI Rules

The following rules are enforced across the entire public surface of QXEngine.
Violations are treated as build errors via `static_assert` where possible.

| # | Rule | Rationale |
|---|---|---|
| 1 | Every public function is declared `extern "C"` | Prevents C++ name mangling |
| 2 | No C++ types cross the boundary | `std::string`, `std::vector`, exceptions are banned from headers |
| 3 | All handles are opaque pointers (`void*` typedef) | Hides internal struct layout from consumers |
| 4 | All structs are C-compatible (`POD` types only) | Ensures stable layout across compilers |
| 5 | All strings cross as `const char*` (UTF-8, null-terminated) | No `std::string` or wide chars |
| 6 | All sizes use `size_t`, counts use `uint32_t` | Fixed-width types prevent platform surprises |
| 7 | All return values are `QXError` or a primitive | No exceptions escape the boundary |
| 8 | Output parameters use pointer-to-pointer or pointer-to-struct | Caller owns the result buffer |

---

## Handle Convention

Every QXEngine subsystem exposes an opaque handle type. Handles follow
a strict lifecycle: **create → use → destroy**. No handle is ever
stack-allocated by the caller.

```c
/* Correct usage pattern */
QXEngineHandle engine = NULL;
QXError err = qx_engine_create(&cfg, &engine);
if (err != QX_OK) { /* handle error */ }

/* ... use engine ... */

qx_engine_destroy(engine);
engine = NULL;   /* caller nulls the handle after destroy */
```

Rules for handles:

- Handles returned by a `_create` function are owned by the caller.
- Passing `NULL` to any function other than `_destroy` returns `QX_ERR_NULL_HANDLE`.
- Passing `NULL` to a `_destroy` function is always a no-op.
- Handles must not be copied or shared across processes.

---

## Error Handling

QXEngine uses `QXError` (a `uint32_t` enum) for all error reporting.
No exceptions are thrown across the boundary. The caller is responsible
for checking every return value.

```c
QXError err = qx_engine_evaluate(engine, &report);
if (err != QX_OK) {
    /* inspect err — see include/qxengine/core/qx_error.h */
}
```

Key error codes:

| Code | Meaning |
|---|---|
| `QX_OK` | Success |
| `QX_ERR_NULL_HANDLE` | A required handle was NULL |
| `QX_ERR_NULL_ARG` | A required pointer argument was NULL |
| `QX_ERR_INVALID_ARG` | An argument value is out of range |
| `QX_ERR_LIMIT_EXCEEDED` | Memory hard limit would be breached |
| `QX_ERR_NOT_INITIALISED` | Engine was not successfully created |

---

## Memory Ownership

Ownership rules are explicit and never implicit:

- **Engine owns subsystem memory.** All internal state allocated by
  `qx_engine_create` is freed by `qx_engine_destroy`.
- **Caller owns the manifest.** `qx_manifest_parse` allocates the
  manifest handle. The caller must call `qx_manifest_destroy` separately
  after `qx_engine_destroy`.
- **Caller owns output structs.** Functions that write into a
  `QXLawReport`, `QXCognitiveSnapshot`, or `QXEngineStats` write into
  caller-supplied stack or heap memory. QXEngine does not allocate these.
- **Strings are transient.** `const char*` values returned by accessor
  functions (e.g. `qx_manifest_app_id`) point into the handle's internal
  buffer. They are valid only while the handle is alive.

---

## Thread Safety

| Scenario | Safe? |
|---|---|
| Calling `qx_engine_create` from two threads simultaneously | ❌ |
| Calling `qx_engine_alloc` and `qx_engine_evaluate` concurrently | ✅ |
| Calling `qx_manifest_parse` from two threads simultaneously | ❌ |
| Reading `qx_manifest_app_id` from two threads concurrently | ✅ |
| Calling `qx_engine_destroy` while another thread uses the engine | ❌ |

The golden rule: **create and destroy on one thread; use from many.**

---

## ABI Versioning

The ABI version is defined as `QX_ABI_VERSION` in `qx_version.h` and
stamped into every platform bridge at compile time. A breaking ABI
change increments this integer. Consumers should assert at startup:

```c
#include "qxengine/core/qx_version.h"
static_assert(QX_ABI_VERSION == 1, "QXEngine ABI version mismatch");
```

---

## Quick Reference — Do and Do Not

| ✅ Do | ❌ Do not |
|---|---|
| Check every `QXError` return value | Ignore return values |
| Null the handle pointer after `_destroy` | Reuse a destroyed handle |
| Allocate output structs on the caller stack | Pass NULL output pointers |
| Call create/destroy on a single thread | Create on one thread, destroy on another |
| Use `QX_ABI_VERSION` assert at startup | Assume ABI compatibility across versions |

---

## Related Documents

- [`overview.md`](overview.md) — High-level architecture
- [`memory-model.md`](memory-model.md) — Memory subsystem internals
- [`constitutional-laws.md`](constitutional-laws.md) — Eight constitutional laws
- [`../api/core.md`](../api/core.md) — Core types and error codes reference

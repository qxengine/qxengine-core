# QXEngine Core — Contributing: Coding Standards

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

These coding standards apply to every file in the `qxengine-core`
repository. They are non-negotiable. Pull requests that violate any
standard will be returned without review until the violation is
corrected. Standards exist to keep the codebase consistent, auditable,
and safe for all target platforms.

---

## General Principles

- **One responsibility per file.** Each `.h` and `.cpp` file has exactly
  one clearly stated purpose, declared in its header comment block.
- **Under 500 lines per file.** Every file — header or source — must be
  strictly under 500 lines. Comments count. No exceptions.
- **No hidden state.** No process-wide globals, no static mutable state
  outside of an opaque handle struct.
- **Explicit over implicit.** All ownership, error handling, and
  lifecycle transitions must be visible at the call site.

---

## File Structure Rules

Every `.h` and `.cpp` file must open with this comment block:

```cpp
/* =============================================================================
 * path/to/file.h  (or .cpp)
 * QXEngine Core – <one-line purpose>
 * Repository : https://github.com/qxengine/qxengine-core
 * Owner      : Masa Bayu
 * Created    : YYYY-MM-DD
 * =========================================================================== */
```

Headers must use `#ifndef` include guards. The guard macro must match
the full file path with slashes replaced by underscores and all letters
uppercased, e.g. `QXENGINE_MEMORY_QX_LEAF_H`.

Source files must list their includes in this order, separated by a
blank line:

1. Corresponding header (for `.cpp` files)
2. Standard library headers (`<cstring>`, `<cstdint>`, etc.)
3. QXEngine internal headers

---

## Naming Conventions

| Entity | Convention | Example |
|---|---|---|
| Public C functions | `qx_<subsystem>_<verb>` | `qx_engine_evaluate` |
| Public C structs | `QX<PascalCase>` | `QXLawReport` |
| Public C enums | `QX<PascalCase>` | `QXCertificationTier` |
| Enum values | `QX_<UPPER_SNAKE>` | `QX_CERT_SOVEREIGN` |
| Constants / macros | `QX_<UPPER_SNAKE>` | `QX_SEGMENT_COUNT` |
| Internal C++ functions | `snake_case` | `compute_tier` |
| Internal C++ structs | `<PascalCase>_s` | `QXEngine_s` |
| Private headers | `qx_<name>_internal.h` | `qx_segment_internal.h` |
| Test files | `test_qx_<subsystem>.cpp` | `test_qx_leaf.cpp` |
| CMake variables | `QX_<UPPER_SNAKE>` | `QX_SOURCES` |

---

## C ABI Rules

- Every public function must be declared `extern "C"` in the header.
- No C++ types (`std::string`, `std::vector`, exceptions) may appear in
  any public header.
- All handles are `typedef void* QX<Name>Handle`.
- All public structs are plain C structs (`POD` types only).
- All public functions return `QXError` or a primitive type.
- Output parameters always come last and are pointer-to-type.

---

## Header Rules

- Every public header must be self-contained — it must compile cleanly
  when included in isolation.
- Every public header must include only what it needs. No transitive
  includes via convenience umbrella headers inside subsystem headers.
- Private headers (`*_internal.h`) must not be listed in `QX_CORE_HEADERS`
  in `CMakeLists.txt` and must not be installed.
- Forward declarations are preferred over includes wherever possible.

---

## Source File Rules

- Every `.cpp` file must include its corresponding header as the first
  include, before any other header.
- Internal helper functions must be declared `static` and placed before
  the first function that uses them.
- Internal structs (`QXEngine_s`, `QXMemloc_s`, etc.) are defined in the
  `.cpp` file or in a private `_internal.h`, never in a public header.
- All mutex locks must use `std::lock_guard` or `std::unique_lock`.
  Raw `mutex.lock()` / `mutex.unlock()` calls are forbidden.

---

## Error Handling Rules

- Every function that can fail must return `QXError`.
- `QX_OK` (0) always means success. Any non-zero value is an error.
- Null pointer arguments must be checked at the top of every function
  before any other logic. Return `QX_ERR_NULL_HANDLE` for null handles
  and `QX_ERR_NULL_ARG` for null data pointers.
- Errors must never be swallowed silently. If a subsystem call fails
  inside an internal function, propagate the error to the caller.

---

## Memory Ownership Rules

- Every `_create` function produces a handle owned by the caller.
- Every `_destroy` function must accept NULL without crashing.
- The engine does not allocate output structs. All output structs
  (`QXLawReport`, `QXEngineStats`, etc.) are caller-allocated.
- `const char*` return values from accessor functions point into
  internal handle memory and must not be freed by the caller.

---

## Threading Rules

- `_create` and `_destroy` functions are **not** thread-safe. Document
  this in the function's header comment.
- All other public functions must be thread-safe. Protect mutable state
  with the handle's internal `std::mutex`.
- Locks must be held for the minimum duration required. Never hold a
  lock while calling back into caller code.
- No recursive mutexes. If recursion is required, refactor.

---

## Documentation Rules

- Every public function must have a documentation comment in its header
  declaring: purpose, parameters, return value, thread-safety, and any
  error codes it can return.
- Every public struct must have a comment describing the role of each
  field.
- Architecture documents live in `docs/architecture/`.
- API reference documents live in `docs/api/`.
- Integration guides live in `docs/integration/`.

---

## Formatting Rules

- Indentation: 4 spaces. No tabs.
- Line length: 100 characters maximum.
- Braces: opening brace on the same line for functions and control flow.
- Pointer and reference declarators attach to the type, not the name:
  `QXError* err` not `QXError *err`.
- Blank lines: one blank line between functions, two blank lines between
  major sections within a file.
- Section separator comments use the 80-character `/* --- */` style as
  shown throughout the codebase.

---

## Related Documents

- [`testing-guide.md`](testing-guide.md) — Test writing standards
- [`../architecture/c-abi-boundary.md`](../architecture/c-abi-boundary.md) — C ABI rules in detail
- [`../architecture/overview.md`](../architecture/overview.md) — Architecture reference

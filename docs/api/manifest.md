# QXEngine Core — API Reference: Manifest

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

The manifest subsystem parses and validates the application JSON manifest
against 14 MFT rules before the engine is created. A successfully parsed
manifest is immutable — no field can be changed after `qx_manifest_parse`
returns. The manifest handle is caller-owned and must be destroyed
separately after `qx_engine_destroy`.

The manifest schema is defined at:
`https://qxengine.io/manifest/v1.0/schema.json`

---

## Headers

| Header | Purpose |
|---|---|
| `include/qxengine/manifest/qx_manifest_types.h` | All manifest structs and constants |
| `include/qxengine/manifest/qx_manifest.h` | Full C ABI for manifest lifecycle and access |

---

## Lifecycle

### `qx_manifest_parse`

```c
QXError qx_manifest_parse(const char*                json,
                            QXManifestHandle*          out,
                            QXManifestValidationResult* result);
```

Parses the UTF-8 JSON string, runs all 14 MFT validation rules, and
writes the handle into `out` and the validation result into `result`.
If validation fails, `out` is set to NULL and `result.is_valid` is
`false`. Returns `QX_ERR_NULL_ARG` if any argument is NULL.
Not thread-safe.

### `qx_manifest_validate`

```c
QXError qx_manifest_validate(QXManifestHandle           handle,
                               QXManifestValidationResult* result);
```

Re-runs all 14 MFT rules against an existing handle and writes the
result into `result`. Useful for confirming manifest integrity after
engine creation. Thread-safe.

### `qx_manifest_destroy`

```c
void qx_manifest_destroy(QXManifestHandle handle);
```

Frees the manifest handle and all internal memory. Passing NULL is a
no-op. Must be called after `qx_engine_destroy`. Not thread-safe.

---

## Validation Result

### QXManifestValidationResult fields

| Field | Type | Description |
|---|---|---|
| `is_valid` | `bool` | True if all MFT rules passed |
| `error_count` | `uint32_t` | Number of rule violations found |
| `errors[]` | `QXManifestValidationError[]` | Array of up to `QX_MANIFEST_MAX_ERRORS` errors |

### QXManifestValidationError fields

| Field | Type | Description |
|---|---|---|
| `rule_id` | `char[]` | MFT rule code e.g. `"MFT-0005"` |
| `message` | `char[]` | Human-readable violation description |
| `severity` | `QXViolationSeverity` | `WARNING`, `CRITICAL`, or `FATAL` |

---

## MFT Validation Rules

| Rule | Severity | Description |
|---|---|---|
| MFT-0001 | FATAL | `$schema` must equal the canonical schema URL |
| MFT-0002 | FATAL | `spec_version` must equal `"1.0.0"` |
| MFT-0003 | FATAL | `app_id` must be non-blank and ≤ 128 characters |
| MFT-0004 | FATAL | `platform` must be one of `android`, `ios`, `web`, `desktop` |
| MFT-0005 | FATAL | All eight law IDs must be present and `all_laws_active` must be true |
| MFT-0006 | FATAL | `soft_limit_mb` must be ≥ 64 |
| MFT-0007 | FATAL | `hard_limit_mb` must be ≥ 128 and > `soft_limit_mb` |
| MFT-0008 | FATAL | Exactly 9 segments must be declared |
| MFT-0009 | FATAL | All segment `budget_percent` values must sum to exactly 100 |
| MFT-0010 | FATAL | All ethics flags must be true |
| MFT-0011 | FATAL | `native_first_policy` must be true |
| MFT-0012 | FATAL | `min_knowledge_score` must be ≥ 90.0 |
| MFT-0013 | FATAL | `battery_drain_max_pct_per_10min` must be ≤ 10.0 |
| MFT-0014 | WARNING | `version` field should follow semver format |

---

## Block Accessor Functions

Block accessors return a const pointer into the handle's internal
memory. The pointer is valid only while the handle is alive. All
block accessors are thread-safe.

### `qx_manifest_identity`

```c
const QXManifestIdentity* qx_manifest_identity(QXManifestHandle handle);
```

Returns the identity block (`app_id`, `platform`, `version`).

### `qx_manifest_law_compliance`

```c
const QXManifestLawCompliance* qx_manifest_law_compliance(QXManifestHandle handle);
```

Returns the law compliance block (`all_laws_active`, `law_ids[]`).

### `qx_manifest_limit`

```c
const QXManifestLimit* qx_manifest_limit(QXManifestHandle handle);
```

Returns the limit block (`soft_limit_mb`, `hard_limit_mb`, `segments[]`).

### `qx_manifest_ethics`

```c
const QXManifestEthics* qx_manifest_ethics(QXManifestHandle handle);
```

Returns the ethics block (`fully_ethical`, `bias_detection`,
`harm_prevention`).

### `qx_manifest_creativity`

```c
const QXManifestCreativity* qx_manifest_creativity(QXManifestHandle handle);
```

Returns the creativity block (`native_first_policy`).

### `qx_manifest_knowledge`

```c
const QXManifestKnowledge* qx_manifest_knowledge(QXManifestHandle handle);
```

Returns the knowledge block (`min_knowledge_score`).

### `qx_manifest_economy`

```c
const QXManifestEconomy* qx_manifest_economy(QXManifestHandle handle);
```

Returns the economy block (`battery_drain_max_pct_per_10min`).

---

## Scalar Accessor Functions

Scalar accessors extract individual fields directly. All are thread-safe.

| Function | Return type | Description |
|---|---|---|
| `qx_manifest_app_id(handle)` | `const char*` | Application ID string |
| `qx_manifest_platform(handle)` | `const char*` | Platform string |
| `qx_manifest_soft_limit_mb(handle)` | `uint32_t` | Soft memory limit in MiB |
| `qx_manifest_hard_limit_mb(handle)` | `uint32_t` | Hard memory limit in MiB |
| `qx_manifest_all_laws_active(handle)` | `bool` | True if all eight laws are active |
| `qx_manifest_is_fully_ethical(handle)` | `bool` | True if all ethics flags are set |
| `qx_manifest_min_knowledge_score(handle)` | `double` | Minimum required knowledge score |
| `qx_manifest_battery_drain_max(handle)` | `double` | Max battery drain %/10 min |

---

## Serialisation

### `qx_manifest_json_size`

```c
size_t qx_manifest_json_size(QXManifestHandle handle);
```

Returns the minimum buffer size in bytes required to hold the JSON
serialisation of the manifest, including the null terminator.

### `qx_manifest_to_json`

```c
QXError qx_manifest_to_json(QXManifestHandle handle,
                              char*            buf,
                              size_t           buf_size);
```

Serialises the manifest to a UTF-8 JSON string and writes it into `buf`.
Returns `QX_ERR_NULL_ARG` if `buf` is NULL. Returns `QX_ERR_INVALID_ARG`
if `buf_size < qx_manifest_json_size(handle)`. Thread-safe.

---

## Common Patterns

**Parsing and validating a manifest before engine creation:**

```c
QXManifestHandle manifest = NULL;
QXManifestValidationResult result;
memset(&result, 0, sizeof(result));

QXError err = qx_manifest_parse(json, &manifest, &result);
if (err != QX_OK || !result.is_valid) {
    for (uint32_t i = 0; i < result.error_count; ++i) {
        printf("[%s] %s\n",
               result.errors[i].rule_id,
               result.errors[i].message);
    }
    return;
}
/* manifest is valid — proceed to qx_engine_create */
```

**Reading scalar values after parse:**

```c
printf("app_id   = %s\n", qx_manifest_app_id(manifest));
printf("platform = %s\n", qx_manifest_platform(manifest));
printf("soft_mb  = %u\n", qx_manifest_soft_limit_mb(manifest));
printf("hard_mb  = %u\n", qx_manifest_hard_limit_mb(manifest));
```

**Correct destroy order:**

```c
qx_engine_destroy(engine);     /* engine first  */
qx_manifest_destroy(manifest); /* manifest second */
```

---

## Related Documents

- [`../architecture/overview.md`](../architecture/overview.md) — How the manifest fits into the engine
- [`../integration/android-ndk.md`](../integration/android-ndk.md) — Manifest loading on Android
- [`../integration/ios-xcframework.md`](../integration/ios-xcframework.md) — Manifest loading on iOS
- [`core.md`](core.md) — Error codes reference
- [`../contributing/testing-guide.md`](../contributing/testing-guide.md) — Testing manifest validation

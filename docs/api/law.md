# QXEngine Core — API Reference: Law

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

The law subsystem evaluates eight immutable constitutional laws on every
call to `qx_engine_evaluate`. It produces a `QXLawReport` containing
per-law scores, a composite health score (0 – 100), a certification
tier, and an append-only audit history capped at
`QX_ENFORCER_HISTORY_CAP` (1 000) entries.

In normal usage, callers invoke `qx_engine_evaluate` and receive a
`QXLawReport`. The functions documented here are for consumers who need
direct access to the enforcer handle, history, or per-law diagnostics.

---

## Headers

| Header | Purpose |
|---|---|
| `include/qxengine/law/qx_law_types.h` | Law IDs, weights, `QXViolation` |
| `include/qxengine/law/qx_law_report.h` | Input/output structs, `QXLawReport` |
| `include/qxengine/law/qx_law_enforcer_types.h` | `QXEnforcerConfig`, `QXEnforcerStats` |
| `include/qxengine/law/qx_law_enforcer.h` | Full C ABI for the enforcer |

---

## Lifecycle

### `qx_law_enforcer_create`

```c
QXError qx_law_enforcer_create(const QXEnforcerConfig* cfg,
                                QXLawEnforcerHandle* out);
```

Creates a law enforcer with the given configuration. Passing `NULL` for
`cfg` uses all default thresholds. Returns `QX_ERR_NULL_ARG` if `out`
is NULL. Not thread-safe.

### `qx_law_enforcer_destroy`

```c
void qx_law_enforcer_destroy(QXLawEnforcerHandle handle);
```

Destroys the enforcer and frees all internal history. Passing NULL is
a no-op. Not thread-safe.

---

## Evaluation

### `qx_law_enforcer_evaluate`

```c
QXError qx_law_enforcer_evaluate(QXLawEnforcerHandle handle,
                                  const QXLawEvaluationInput* input,
                                  QXLawReport* out);
```

Evaluates all eight constitutional laws against `input` and writes the
result into `out`. The report is also appended to the internal audit
history. If `fail_fast_on_fatal` is set in the config, evaluation halts
at the first `FATAL` violation. Thread-safe.

Returns `QX_ERR_NULL_HANDLE` if `handle` is NULL, `QX_ERR_NULL_ARG` if
`input` or `out` is NULL.

### `qx_law_enforcer_evaluate_single`

```c
QXError qx_law_enforcer_evaluate_single(QXLawEnforcerHandle handle,
                                         uint8_t law_index,
                                         const QXLawEvaluationInput* input,
                                         QXLawScore* out);
```

Evaluates a single law by index (0 – 7) and writes the per-law score
into `out`. Useful for diagnostics and targeted testing. Does not append
to history. Returns `QX_ERR_INVALID_ARG` if `law_index >= QX_LAW_COUNT`.
Thread-safe.

---

## History

### `qx_law_enforcer_history_count`

```c
QXError qx_law_enforcer_history_count(QXLawEnforcerHandle handle,
                                       uint32_t* out);
```

Writes the number of reports currently in the audit history into `out`.
Thread-safe.

### `qx_law_enforcer_history_at`

```c
QXError qx_law_enforcer_history_at(QXLawEnforcerHandle handle,
                                    uint32_t index,
                                    QXLawReport* out);
```

Copies the report at `index` into `out`. Returns `QX_ERR_OUT_OF_BOUNDS`
if `index >= history_count`. Thread-safe.

### `qx_law_enforcer_history_latest`

```c
QXError qx_law_enforcer_history_latest(QXLawEnforcerHandle handle,
                                        QXLawReport* out);
```

Copies the most recently appended report into `out`. Returns
`QX_ERR_NOT_FOUND` if the history is empty. Thread-safe.

### `qx_law_enforcer_history_copy`

```c
QXError qx_law_enforcer_history_copy(QXLawEnforcerHandle handle,
                                      QXLawReport* buf,
                                      uint32_t buf_count,
                                      uint32_t* copied_out);
```

Copies up to `buf_count` reports into `buf`, starting from the oldest.
Writes the actual number copied into `copied_out`. Thread-safe.

### `qx_law_enforcer_clear_history`

```c
QXError qx_law_enforcer_clear_history(QXLawEnforcerHandle handle);
```

Removes all entries from the audit history. The enforcer remains valid
and usable after clearing. Thread-safe.

---

## Statistics

### `qx_law_enforcer_stats`

```c
QXError qx_law_enforcer_stats(QXLawEnforcerHandle handle,
                               QXEnforcerStats* out);
```

Fills `out` with lifetime counters and score statistics. Thread-safe.

**QXEnforcerStats fields:**

| Field | Type | Description |
|---|---|---|
| `total_evaluations` | `uint32_t` | Total calls to `evaluate` |
| `fatal_evaluations` | `uint32_t` | Evaluations with at least one FATAL |
| `critical_evaluations` | `uint32_t` | Evaluations with at least one CRITICAL |
| `compliant_evaluations` | `uint32_t` | Evaluations with all laws passing |
| `peak_health_score` | `double` | Highest health score recorded |
| `trough_health_score` | `double` | Lowest health score recorded |
| `mean_health_score` | `double` | Running mean health score |
| `last_health_score` | `double` | Most recent health score |
| `last_certification` | `QXCertificationTier` | Most recent certification tier |
| `history_count` | `uint32_t` | Current entries in audit history |

---

## Health Score and Certification

### `qx_law_enforcer_health_score`

```c
QXError qx_law_enforcer_health_score(QXLawEnforcerHandle handle,
                                      double* out);
```

Writes the most recent health score into `out`. Returns
`QX_ERR_NOT_FOUND` if no evaluation has been run yet. Thread-safe.

### `qx_law_enforcer_certification`

```c
QXError qx_law_enforcer_certification(QXLawEnforcerHandle handle,
                                       QXCertificationTier* out);
```

Writes the most recent certification tier into `out`. Thread-safe.

### `qx_law_enforcer_is_compliant`

```c
bool qx_law_enforcer_is_compliant(QXLawEnforcerHandle handle);
```

Returns `true` if the most recent evaluation produced a health score
≥ `QX_ENFORCER_MIN_PASS_SCORE` (60.0) with no FATAL violations.
Returns `false` for a NULL handle or if no evaluation has been run.

---

## Certification Utility Functions

### `qx_certification_label`

```c
const char* qx_certification_label(QXCertificationTier tier);
```

Returns a human-readable label for the tier, e.g. `"SOVEREIGN"`.
The returned pointer is a string literal — do not free it.

### `qx_certification_min_score`

```c
double qx_certification_min_score(QXCertificationTier tier);
```

Returns the minimum health score required for the given tier.

### `qx_certification_from_score`

```c
QXCertificationTier qx_certification_from_score(double health_score);
```

Returns the highest certification tier achievable at the given health
score. Returns `QX_CERT_UNCERTIFIED` for scores below 60.0 or negative.

---

## QXLawReport Key Fields

| Field | Type | Description |
|---|---|---|
| `health_score` | `double` | Composite score 0.0 – 100.0 |
| `certification` | `QXCertificationTier` | Current certification tier |
| `is_fully_compliant` | `bool` | True if all eight laws pass |
| `has_fatal_violation` | `bool` | True if any law is FATAL |
| `law_scores[8]` | `QXLawScore[]` | Per-law score and compliance |
| `violations[]` | `QXViolation[]` | All active violations |
| `violation_count` | `uint32_t` | Number of active violations |
| `eval_duration_ms` | `double` | Evaluation duration in ms |
| `sequence` | `uint64_t` | Monotonically increasing eval counter |

---

## Common Patterns

**Full evaluation cycle:**

```c
QXLawReport report;
memset(&report, 0, sizeof(report));
QXError err = qx_engine_evaluate(engine, &report);
if (err == QX_OK && report.has_fatal_violation) {
    /* take corrective action */
}
```

**Checking certification after evaluation:**

```c
if (qx_report_is_certified(&report)) {
    /* engine is certified — safe to proceed */
}
QXCertificationTier tier = report.certification;
const char* label = qx_certification_label(tier);
```

**Inspecting a specific law score:**

```c
QXLawScore score;
memset(&score, 0, sizeof(score));
qx_law_enforcer_evaluate_single(enforcer, 4, &input, &score);
/* Law 4 = Knowledge Score */
```

---

## Related Documents

- [`../architecture/constitutional-laws.md`](../architecture/constitutional-laws.md) — All eight laws explained
- [`core.md`](core.md) — Error codes and certification tier enum
- [`intelligence.md`](intelligence.md) — Knowledge score feeds Law 4
- [`../contributing/testing-guide.md`](../contributing/testing-guide.md) — Testing law evaluations

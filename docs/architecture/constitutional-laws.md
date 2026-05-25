# QXEngine Core — Constitutional Laws

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Why Constitutional Laws?

QXEngine enforces eight immutable constitutional laws on every evaluation
cycle. They are called constitutional because they cannot be disabled,
overridden, or reweighted at runtime. They represent the non-negotiable
operating contract of the engine.

Every law produces a per-law score (0.0 – 1.0), a compliance flag, and
a worst-case severity (`INFO`, `WARNING`, `CRITICAL`, or `FATAL`). The
eight scores are combined into a single **health score** (0 – 100) using
fixed weights that sum to exactly 1.0. A health score below 60 means the
engine is non-compliant and will not receive a certification tier.

---

## Laws at a Glance

| ID | Name | Weight | Fatal threshold |
|---|---|---|---|
| 0 | Pattern Integrity | 0.15 | Any unlabelled allocation |
| 1 | Label Integrity | 0.10 | Blank or duplicate label |
| 2 | Segment Limits | 0.20 | Any segment over hard limit |
| 3 | Ethics Compliance | 0.15 | Any ethics flag false |
| 4 | Knowledge Score | 0.15 | Score < 50.0 |
| 5 | Native-First Policy | 0.10 | Native utilisation < 50 % |
| 6 | Battery and Network | 0.10 | Battery drain > 10 %/10 min |
| 7 | Economic Equilibrium | 0.05 | Overload + starvation > 8 segments |

---

## Law 0 — Pattern Integrity (weight 0.15)

**Purpose:** Ensures every allocation carries a semantic label so that
memory usage is always attributable and auditable.

**Input:** `unlabelled_allocation_count` from `QXLawEvaluationInput`.

**Scoring:** Any unlabelled allocation produces a `FATAL` violation and
a law score of 0.0. Zero unlabelled allocations scores 1.0.

**Manifest rule:** MFT-0001 requires `spec_version = "1.0.0"`, which
activates label enforcement from the first allocation.

---

## Law 1 — Label Integrity (weight 0.10)

**Purpose:** Ensures allocation labels are non-blank, within the maximum
length (`QX_LEAF_LABEL_MAX`), and follow the dot-separated namespace
convention (e.g. `model.weights.layer3`).

**Input:** Per-leaf label inspection during the evaluation pass.

**Scoring:** Each invalid label reduces the law score proportionally.
A blank label on any active leaf is `CRITICAL`. Exceeding
`QX_LEAF_LABEL_MAX` is `WARNING`.

---

## Law 2 — Segment Limits (weight 0.20)

**Purpose:** Enforces the per-segment soft and hard memory budgets
declared in the manifest.

**Input:** `segments[]` array in `QXLawEvaluationInput`, each entry
carrying `used_bytes`, `soft_limit_bytes`, and `hard_limit_bytes`.

**Scoring:** Any segment over its hard limit is `FATAL` (score 0.0 for
that segment). Any segment over its soft limit is `CRITICAL`. The law
score is the mean of all nine per-segment scores.

---

## Law 3 — Ethics Compliance (weight 0.15)

**Purpose:** Ensures the engine's ethics subsystem is fully active.
All four ethics flags must be true at all times.

**Input:** `bias_detection_enabled`, `harm_prevention_enabled`,
`transparency_enabled`, `consent_enforcement_enabled`.

**Scoring:** Each false flag is a `FATAL` violation. All four true
scores 1.0. Partial compliance is not accepted — any false flag
drives the law score to 0.0.

---

## Law 4 — Knowledge Score (weight 0.15)

**Purpose:** Ensures the engine's cognitive intelligence remains above
the minimum threshold declared in the manifest.

**Input:** `knowledge_score` (0.0 – 100.0) from `QXLawEvaluationInput`,
derived from the most recent `QXCognitiveSnapshot`.

**Scoring and thresholds:**

| Score range | Severity | Law score |
|---|---|---|
| ≥ 90.0 | COMPLIANT | 1.0 |
| 70.0 – 89.9 | WARNING | 0.7 |
| 50.0 – 69.9 | CRITICAL | 0.4 |
| < 50.0 | FATAL | 0.0 |

---

## Law 5 — Native-First Policy (weight 0.10)

**Purpose:** Mandates that native-layer allocations account for the
majority of memory usage, preventing interpreted or scripted layers
from dominating the heap.

**Input:** `native_first_policy_active` flag and
`native_utilisation_ratio` (0.0 – 1.0).

**Scoring:** Policy inactive is `FATAL`. Ratio below 0.50 is `FATAL`.
Ratio below `min_native_utilisation` (default 0.70) is `CRITICAL`.
Ratio at or above the threshold scores 1.0.

---

## Law 6 — Battery and Network (weight 0.10)

**Purpose:** Prevents the engine from consuming excessive battery power
and ensures network redundancy is enabled for resilient operation.

**Input:** `battery_drain_pct_per_10min` and
`network_redundancy_enabled`.

**Scoring:** Battery drain above 10 %/10 min is `FATAL`. Drain above
`max_battery_drain` config threshold is `CRITICAL`.
`network_redundancy_enabled = false` is `WARNING`.

---

## Law 7 — Economic Equilibrium (weight 0.05)

**Purpose:** Ensures memory resources are distributed equitably across
all nine segments, preventing overload or starvation in any one segment.

**Input:** `overload_segments` (count of segments over soft limit) and
`starved_segments` (count of segments with near-zero utilisation).

**Scoring:** Combined overload + starvation count above 8 is `FATAL`.
Above 4 is `CRITICAL`. Above 2 is `WARNING`. Zero scores 1.0.

---

## Health Score Formula

The composite health score is computed as the weighted sum of all eight
law scores, scaled to 0 – 100:

$$\text{health\_score} = 100 \times \sum_{i=0}^{7} w_i \times s_i$$

Where $$w_i$$ is the weight for law $$i$$ and $$s_i$$ is the per-law
score in range [0.0, 1.0]. Weights sum to exactly 1.0, so the maximum
health score is 100.

Any `FATAL` violation in any law caps the health score at a maximum of
59.9, ensuring the engine cannot be certified while a fatal violation
is active.

---

## Certification Tiers

| Tier | Min health score | Label |
|---|---|---|
| UNCERTIFIED | 0.0 | Not certified |
| PROVISIONAL | 60.0 | Provisional certification |
| STANDARD | 75.0 | Standard certification |
| ADVANCED | 88.0 | Advanced certification |
| SOVEREIGN | 96.0 | Sovereign certification |

Certification tier is recalculated on every `qx_engine_evaluate` call.
The tier is accessible via `qx_law_enforcer_certification` and is
included in every `QXLawReport`.

---

## Related Documents

- [`overview.md`](overview.md) — High-level architecture
- [`memory-model.md`](memory-model.md) — How memory data feeds Law 2
- [`../api/law.md`](../api/law.md) — Law enforcer API reference
- [`../api/intelligence.md`](../api/intelligence.md) — Knowledge score computation

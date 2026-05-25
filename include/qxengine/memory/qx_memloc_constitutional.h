/* ============================================================
 * qx_memloc_constitutional.h
 * QXMemloc — Constitutional Master Header
 *
 * The single include for the complete constitutional
 * memory allocation system.
 *
 * Four layers. Four elements. One constitutional system.
 *
 *   Th [Tanah]  → qx_memloc_pool.h     Structure
 *   Ar [Air]    → qx_memloc_flow.h     Flow
 *   An [Angin]  → qx_memloc_signal.h   Signal
 *   Ai [Api]    → qx_memloc_enforce.h  Enforcement
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#ifndef QX_MEMLOC_CONSTITUTIONAL_H
#define QX_MEMLOC_CONSTITUTIONAL_H

#include "qxengine/memory/qx_memloc_pool.h"
#include "qxengine/memory/qx_memloc_flow.h"
#include "qxengine/memory/qx_memloc_signal.h"
#include "qxengine/memory/qx_memloc_enforce.h"
#include "qxengine/law/qx_law_report.h"

/* ── Constitutional certification ──────────────────────────── */

#define QX_SCORE_SOVEREIGN    95.0
#define QX_SCORE_PROFESSIONAL 80.0
#define QX_SCORE_STANDARD     60.0
#define QX_SCORE_MINIMUM      0.0

typedef enum {
    QX_CERTIFICATION_NONE         = QX_CERT_INVALID,
    QX_CERTIFICATION_STANDARD     = QX_CERT_STANDARD,
    QX_CERTIFICATION_PROFESSIONAL = QX_CERT_PROFESSIONAL,
    QX_CERTIFICATION_SOVEREIGN    = QX_CERT_SOVEREIGN
} QXCertificationTier;

/* ── User-facing request and report types ──────────────────── */

typedef struct {
    QXSize      soft_budget_bytes;
    QXSize      hard_budget_bytes;
    double      device_scale;
    double      declared_x;
    double      tolerance_pct;
    const char* instance_label;
} QXConstitutionalConfig;

typedef struct {
    const char*     label;
    const char*     segment_id;
    const char*     slot_id;
    QXSize          size_bytes;
    QXLeafClassId   leaf_class;
    const char*     purpose;
} QXAllocationRequest;

typedef struct {
    QXLeafHandle leaf_handle;
    void*        ptr;
    QXSize       usable_bytes;
    const char*  segment_id;
    QXTimestamp  allocated_at_ms;
    double       declared_x;
} QXConstitutionalAllocation;

typedef struct {
    QXSequence              cycle_number;
    QXTimestamp             cycle_timestamp_ms;
    uint64_t                cycle_duration_ms;
    QXLawReport             law_report;
    QXEngineABAMeasurement  aba_measurement;
    double                  universe_law_score;
    double                  humanity_law_score;
    double                  health_score;
    QXCertificationTier     certification;
    double                  pool_utilisation_pct;
    uint32_t                active_leaf_count;
    uint32_t                orphan_leaf_count;
} QXConstitutionalReport;

/*
 * QXMemlocConstitutional
 *
 * The complete constitutional allocator.
 * All four layers composed into one governing system.
 *
 *   pool     — Th [Tanah]  — the earth, the structure
 *   flow     — Ar [Air]    — the water, the distribution
 *   signal   — An [Angin]  — the wind, the measurement
 *   enforce  — Ai [Api]    — the fire, the enforcement
 *
 * These four are inseparable. Together they form
 * QXMemloc — the first constitutional memory allocator
 * derived from ALAMTOLOGI Quranic Science.
 */
typedef struct {
    QXMemlocPool          pool;     /* Th — structure layer               */
    QXMemlocFlowHandle    flow;     /* Ar — flow layer                    */
    QXMemlocSignalHandle  signal;   /* An — signal layer                  */
    QXMemlocEnforceHandle enforce;  /* Ai — enforcement layer             */
    char                  instance_id[64];
    double                declared_x;
    double                tolerance_pct;
    QXSequence            cycle_count;
    QXTimestamp           created_at_ms;
    QXTimestamp           last_cycle_ms;
    double                health_score;
    QXCertificationTier   certification;
} QXMemlocConstitutional;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * qx_memloc_constitutional_create
 *
 * Creates the complete constitutional allocator.
 * Initialises all four layers in the correct order:
 *   1. Th [Tanah]  — pool reserved from OS
 *   2. Ar [Air]    — flow controller created over pool
 *   3. An [Angin]  — signal layer created over pool
 *   4. Ai [Api]    — enforcement layer created over flow+signal
 *
 * Y → Z → X → UNSUR → ILMU → APLIKASI
 *
 * Thread safety: NOT thread-safe. Call once at startup.
 */
QX_API QXResult qx_memloc_constitutional_create(
    QXSize                   soft_limit_bytes,
    QXSize                   hard_limit_bytes,
    double                   device_scale,
    double                   declared_x,
    double                   x_tolerance_pct,
    QXMemlocConstitutional*  out_memloc
);

/*
 * qx_memloc_constitutional_create_config
 *
 * Convenience constructor that accepts the full user-facing config.
 * It preserves the caller-owned struct model used by the current ABI.
 */
QX_API QXResult qx_memloc_constitutional_create_config(
    const QXConstitutionalConfig* config,
    QXMemlocConstitutional*       out_memloc
);

/*
 * qx_memloc_constitutional_destroy
 *
 * Destroys all four layers in reverse order.
 * Z.3 [Pairs] — created together, destroyed together.
 *
 * Thread safety: NOT thread-safe. Call once at shutdown.
 */
QX_API void qx_memloc_constitutional_destroy(
    QXMemlocConstitutional* memloc
);

/* ── Allocation API ─────────────────────────────────────────── */

QX_API QXResult qx_memloc_constitutional_allocate(
    QXMemlocConstitutional*      memloc,
    const QXAllocationRequest*   request,
    QXConstitutionalAllocation*  out_allocation
);

QX_API QXResult qx_memloc_constitutional_deallocate(
    QXMemlocConstitutional* memloc,
    QXLeafHandle            leaf_handle
);

/*
 * qx_memloc_constitutional_cycle
 *
 * The complete cognitive cycle for the constitutional
 * allocator. Called every 5 seconds by the engine.
 *
 * Executes in ABA order:
 *   1 → measure signals    (An [Angin])
 *   2 → evaluate X         (An [Angin])
 *   4 → enforce laws       (Ai [Api])
 *   4 → check equilibrium  (Ai [Api])
 *   2 → update budgets     (Ar [Air])
 *   1 → record snapshot    (An [Angin])
 *
 * Thread safety: Thread-safe.
 */
QX_API QXResult qx_memloc_constitutional_cycle(
    QXMemlocConstitutional*  memloc,
    QXTimestamp              now_ms,
    QXEngineABAMeasurement*  out_measurement,
    QXEnforcementResult*     out_enforcement
);

QX_API QXResult qx_memloc_constitutional_report(
    QXMemlocConstitutional*  memloc,
    QXTimestamp              now_ms,
    QXConstitutionalReport*  out_report
);

/* ── OS signal forwarding ──────────────────────────────────── */

QX_API QXResult qx_memloc_constitutional_notify_pressure(
    QXMemlocConstitutional* memloc,
    uint8_t                 pressure_tier
);

QX_API QXResult qx_memloc_constitutional_notify_lifecycle(
    QXMemlocConstitutional* memloc,
    uint8_t                 lifecycle_state
);

QX_API QXResult qx_memloc_constitutional_notify_screen(
    QXMemlocConstitutional* memloc,
    const char*             screen_profile_id
);

/* ── Query API ──────────────────────────────────────────────── */

QX_API double qx_memloc_constitutional_health_score(
    const QXMemlocConstitutional* memloc
);

QX_API QXCertificationTier qx_memloc_constitutional_certification(
    const QXMemlocConstitutional* memloc
);

QX_API const char* qx_memloc_constitutional_instance_id(
    const QXMemlocConstitutional* memloc
);

#ifdef __cplusplus
}
#endif

#endif /* QX_MEMLOC_CONSTITUTIONAL_H */

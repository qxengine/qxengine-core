/* ============================================================
 * qx_memloc_constitutional.c
 * QXMemloc — Constitutional Master
 *
 * The living composition of all four elemental layers:
 *
 *   Th [Tanah]  — qx_memloc_pool      — structure
 *   Ar [Air]    — qx_memloc_flow      — flow
 *   An [Angin]  — qx_memloc_signal    — measurement
 *   Ai [Api]    — qx_memloc_enforce   — enforcement
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qxengine/memory/qx_memloc_constitutional.h"
#include "qx_memloc_signal_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define QX_CONSTITUTIONAL_DEFAULT_X       1.0
#define QX_CONSTITUTIONAL_DEFAULT_TOL_PCT 15.0

/* ── Lifecycle ──────────────────────────────────────────────── */

static QXTimestamp constitutional_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (QXTimestamp)(
        (uint64_t)ts.tv_sec * 1000u +
        (uint64_t)ts.tv_nsec / 1000000u
    );
}

static QXCertificationTier constitutional_certify(double score)
{
    if (score >= QX_SCORE_SOVEREIGN) {
        return QX_CERTIFICATION_SOVEREIGN;
    }
    if (score >= QX_SCORE_PROFESSIONAL) {
        return QX_CERTIFICATION_PROFESSIONAL;
    }
    if (score >= QX_SCORE_STANDARD) {
        return QX_CERTIFICATION_STANDARD;
    }
    return QX_CERTIFICATION_NONE;
}

static QXFlowLeafRecord* constitutional_find_leaf(
    QXMemlocFlowHandle flow,
    QXLeafHandle       leaf)
{
    uint32_t i;

    if (flow == NULL || leaf == NULL) {
        return NULL;
    }

    for (i = 0u; i < QX_FLOW_MAX_LEAVES; ++i) {
        if ((QXLeafHandle)&flow->leaves[i] == leaf) {
            return &flow->leaves[i];
        }
    }

    return NULL;
}

static const char* constitutional_segment_id(const char* segment_id)
{
    if (segment_id == NULL) {
        return NULL;
    }
    if (strncmp(segment_id, "QLM-", 4u) == 0) {
        return segment_id;
    }
    if (strncmp(segment_id, "CORE", 5u) == 0) {
        return QX_SEGMENT_CORE;
    }
    if (strncmp(segment_id, "UI", 3u) == 0) {
        return QX_SEGMENT_UI;
    }
    if (strncmp(segment_id, "DATA", 5u) == 0) {
        return QX_SEGMENT_DATA;
    }
    if (strncmp(segment_id, "IMG", 4u) == 0) {
        return QX_SEGMENT_IMG;
    }
    if (strncmp(segment_id, "NET", 4u) == 0) {
        return QX_SEGMENT_NET;
    }
    if (strncmp(segment_id, "AI", 3u) == 0) {
        return QX_SEGMENT_AI;
    }
    if (strncmp(segment_id, "SEC", 4u) == 0) {
        return QX_SEGMENT_SEC;
    }
    if (strncmp(segment_id, "LOG", 4u) == 0) {
        return QX_SEGMENT_LOG;
    }
    if (strncmp(segment_id, "TEMP", 5u) == 0) {
        return QX_SEGMENT_TEMP;
    }
    return segment_id;
}

QXResult qx_memloc_constitutional_create(
    QXSize                   soft_limit_bytes,
    QXSize                   hard_limit_bytes,
    double                   device_scale,
    double                   declared_x,
    double                   x_tolerance_pct,
    QXMemlocConstitutional*  out_memloc)
{
    QXResult r;

    if (out_memloc == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(out_memloc, 0, sizeof(QXMemlocConstitutional));

    /*
     * 1 → 2 → 4 expansion:
     * one public create call awakens the four elemental layers
     * in their constitutional order.
     */
    r = qx_pool_create(
        soft_limit_bytes,
        hard_limit_bytes,
        device_scale,
        &out_memloc->pool
    );
    if (r != QX_OK) {
        memset(out_memloc, 0, sizeof(QXMemlocConstitutional));
        return r;
    }

    r = qx_flow_create(&out_memloc->pool, &out_memloc->flow);
    if (r != QX_OK) {
        qx_pool_destroy(&out_memloc->pool);
        memset(out_memloc, 0, sizeof(QXMemlocConstitutional));
        return r;
    }

    r = qx_signal_create(
        &out_memloc->pool,
        declared_x,
        x_tolerance_pct,
        &out_memloc->signal
    );
    if (r != QX_OK) {
        qx_flow_destroy(out_memloc->flow);
        qx_pool_destroy(&out_memloc->pool);
        memset(out_memloc, 0, sizeof(QXMemlocConstitutional));
        return r;
    }

    qx_signal_attach_flow(out_memloc->signal, out_memloc->flow);

    r = qx_enforce_create(
        out_memloc->flow,
        out_memloc->signal,
        &out_memloc->enforce
    );
    if (r != QX_OK) {
        qx_signal_destroy(out_memloc->signal);
        qx_flow_destroy(out_memloc->flow);
        qx_pool_destroy(&out_memloc->pool);
        memset(out_memloc, 0, sizeof(QXMemlocConstitutional));
        return r;
    }

    out_memloc->declared_x = declared_x;
    out_memloc->tolerance_pct = x_tolerance_pct;
    out_memloc->created_at_ms = constitutional_now_ms();
    out_memloc->health_score = 100.0;
    out_memloc->certification = QX_CERTIFICATION_SOVEREIGN;
    snprintf(
        out_memloc->instance_id,
        sizeof(out_memloc->instance_id),
        "qxmemloc-constitutional-%llu",
        (unsigned long long)out_memloc->created_at_ms
    );

    return QX_OK;
}

QXResult qx_memloc_constitutional_create_config(
    const QXConstitutionalConfig* config,
    QXMemlocConstitutional*       out_memloc)
{
    double declared_x;
    double tolerance_pct;
    QXResult r;

    if (config == NULL || out_memloc == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    declared_x = (config->declared_x > 0.0)
        ? config->declared_x
        : QX_CONSTITUTIONAL_DEFAULT_X;
    tolerance_pct = (config->tolerance_pct > 0.0)
        ? config->tolerance_pct
        : QX_CONSTITUTIONAL_DEFAULT_TOL_PCT;

    r = qx_memloc_constitutional_create(
        config->soft_budget_bytes,
        config->hard_budget_bytes,
        config->device_scale,
        declared_x,
        tolerance_pct,
        out_memloc
    );

    if (r == QX_OK && config->instance_label != NULL &&
        strnlen(config->instance_label, sizeof(out_memloc->instance_id)) > 0u) {
        snprintf(
            out_memloc->instance_id,
            sizeof(out_memloc->instance_id),
            "%s",
            config->instance_label
        );
    }

    return r;
}

void qx_memloc_constitutional_destroy(
    QXMemlocConstitutional* memloc)
{
    if (memloc == NULL) {
        return;
    }

    /*
     * 4 → 2 → 1 return:
     * destroy in reverse order so every created layer returns
     * through its pair before the pool releases the earth.
     */
    qx_enforce_destroy(memloc->enforce);
    qx_signal_destroy(memloc->signal);
    qx_flow_destroy(memloc->flow);
    qx_pool_destroy(&memloc->pool);

    memset(memloc, 0, sizeof(QXMemlocConstitutional));
}

/* ── Allocation ─────────────────────────────────────────────── */

QXResult qx_memloc_constitutional_allocate(
    QXMemlocConstitutional*      memloc,
    const QXAllocationRequest*   request,
    QXConstitutionalAllocation*  out_allocation)
{
    QXResult r;
    const char* segment_id;
    QXFlowAllocation flow_allocation;

    if (memloc == NULL || request == NULL || out_allocation == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (request->label == NULL || request->segment_id == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (request->size_bytes == 0u ||
        request->leaf_class > QX_LEAF_CLASS_D) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_allocation, 0, sizeof(QXConstitutionalAllocation));
    memset(&flow_allocation, 0, sizeof(flow_allocation));

    segment_id = constitutional_segment_id(request->segment_id);

    r = qx_flow_allocate(
        memloc->flow,
        request->label,
        segment_id,
        request->leaf_class,
        request->size_bytes,
        &flow_allocation
    );
    if (r != QX_OK) {
        return r;
    }

    r = qx_signal_register_leaf(
        memloc->signal,
        request->label,
        flow_allocation.usable_bytes,
        flow_allocation.allocated_at_ms
    );
    if (r != QX_OK && r != QX_ERR_INVALID_ARGUMENT) {
        QXFlowRelease release;
        memset(&release, 0, sizeof(release));
        qx_flow_deallocate(memloc->flow, flow_allocation.leaf, &release);
        return r;
    }

    out_allocation->leaf_handle = flow_allocation.leaf;
    out_allocation->ptr = flow_allocation.ptr;
    out_allocation->usable_bytes = flow_allocation.usable_bytes;
    out_allocation->segment_id = segment_id;
    out_allocation->allocated_at_ms = flow_allocation.allocated_at_ms;
    out_allocation->declared_x = memloc->declared_x;

    return QX_OK;
}

QXResult qx_memloc_constitutional_deallocate(
    QXMemlocConstitutional* memloc,
    QXLeafHandle            leaf_handle)
{
    QXFlowLeafRecord* rec;
    char label[128];
    QXFlowRelease release;

    if (memloc == NULL || leaf_handle == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    rec = constitutional_find_leaf(memloc->flow, leaf_handle);
    if (rec == NULL) {
        return QX_ERR_LEAF_NOT_FOUND;
    }

    if (!rec->is_active) {
        return QX_ERR_DOUBLE_FREE;
    }

    memset(label, 0, sizeof(label));
    strncpy(label, rec->label, sizeof(label) - 1u);

    qx_signal_deregister_leaf(memloc->signal, label, NULL);

    memset(&release, 0, sizeof(release));
    return qx_flow_deallocate(memloc->flow, leaf_handle, &release);
}

/* ── Cognitive cycle ───────────────────────────────────────── */

QXResult qx_memloc_constitutional_cycle(
    QXMemlocConstitutional*  memloc,
    QXTimestamp              now_ms,
    QXEngineABAMeasurement*  out_measurement,
    QXEnforcementResult*     out_enforcement)
{
    QXResult r;
    QXSegmentABAMeasurement segments[QX_POOL_SEGMENT_COUNT];
    QXEnforcementResult cycle_enforcement;
    QXEnforcementResult equilibrium_enforcement;

    if (memloc == NULL) {
        return QX_ERR_NULL_HANDLE;
    }
    if (out_measurement == NULL || out_enforcement == NULL) {
        return QX_ERR_NULL_HANDLE;
    }
    if (!memloc->pool.is_initialised ||
        memloc->flow == NULL ||
        memloc->signal == NULL ||
        memloc->enforce == NULL) {
        return QX_ERR_NULL_HANDLE;
    }

    memset(segments, 0, sizeof(segments));
    memset(out_measurement, 0, sizeof(QXEngineABAMeasurement));
    memset(out_enforcement, 0, sizeof(QXEnforcementResult));
    memset(&cycle_enforcement, 0, sizeof(cycle_enforcement));
    memset(&equilibrium_enforcement, 0, sizeof(equilibrium_enforcement));

    /*
     * Phase 1 — An [Angin]:
     * measure X = m/t across active leaves and segments.
     */
    r = qx_signal_measure_cycle(
        memloc->signal,
        now_ms,
        out_measurement,
        segments,
        QX_POOL_SEGMENT_COUNT
    );
    if (r != QX_OK) {
        return r;
    }

    /*
     * Phase 2 — Ai [Api]:
     * enforce fatal X and Pairs violations from the signal truth.
     */
    r = qx_enforce_cycle(
        memloc->enforce,
        out_measurement,
        &cycle_enforcement
    );
    if (r != QX_OK) {
        return r;
    }

    /*
     * Phase 3 — Z.4 [Equilibrium]:
     * rebalance overloaded/starved segment asymmetry.
     */
    r = qx_enforce_equilibrium(
        memloc->enforce,
        &equilibrium_enforcement
    );
    if (r != QX_OK) {
        return r;
    }

    *out_enforcement = cycle_enforcement;
    if (equilibrium_enforcement.evicted_count >
        out_enforcement->evicted_count) {
        out_enforcement->action = equilibrium_enforcement.action;
    }
    out_enforcement->evicted_count +=
        equilibrium_enforcement.evicted_count;
    out_enforcement->bytes_released +=
        equilibrium_enforcement.bytes_released;
    out_enforcement->pages_decommitted +=
        equilibrium_enforcement.pages_decommitted;
    out_enforcement->orphans_resolved +=
        equilibrium_enforcement.orphans_resolved;
    out_enforcement->equilibrium_restored =
        equilibrium_enforcement.equilibrium_restored;
    if (out_enforcement->enforced_at_ms == 0u) {
        out_enforcement->enforced_at_ms =
            equilibrium_enforcement.enforced_at_ms;
    }

    memloc->cycle_count++;
    memloc->last_cycle_ms = now_ms;
    memloc->health_score = out_measurement->is_constant ? 100.0 : 80.0;
    if (out_measurement->fatal_leaves > 0u) {
        memloc->health_score = 55.0;
    }
    memloc->certification = constitutional_certify(memloc->health_score);

    return QX_OK;
}

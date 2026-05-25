/* ============================================================
 * qx_memloc_constitutional_luman.c
 * QXMemloc — Constitutional Master LUMAN Create Path
 *
 * Composes all four memory layers using LUMAN-derived pool
 * budgets.
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

static QXTimestamp luman_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (QXTimestamp)(
        (uint64_t)ts.tv_sec * 1000u +
        (uint64_t)ts.tv_nsec / 1000000u
    );
}

QXResult qx_memloc_constitutional_create_luman(
    const QXConstitutionalConfig* config,
    const QXLumanInitResult*      luman_result,
    QXMemlocConstitutional*       out_memloc)
{
    QXResult r;
    double declared_x;
    double tolerance_pct;

    if (config == NULL || luman_result == NULL || out_memloc == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }
    if (luman_result->initialized != QX_TRUE) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    memset(out_memloc, 0, sizeof(QXMemlocConstitutional));

    declared_x = (config->declared_x > 0.0) ? config->declared_x : 1.0;
    tolerance_pct = (config->tolerance_pct > 0.0)
        ? config->tolerance_pct
        : 15.0;

    r = qx_pool_create_luman(luman_result, &out_memloc->pool);
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
        tolerance_pct,
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
    out_memloc->tolerance_pct = tolerance_pct;
    out_memloc->created_at_ms = luman_now_ms();
    out_memloc->health_score = 100.0;
    out_memloc->certification = QX_CERTIFICATION_SOVEREIGN;
    snprintf(
        out_memloc->instance_id,
        sizeof(out_memloc->instance_id),
        "%s-%s",
        (config->instance_label != NULL) ? config->instance_label : "qxmemloc",
        luman_result->device_profile.peringkat_name
    );

    return QX_OK;
}

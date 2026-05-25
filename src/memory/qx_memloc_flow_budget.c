/* ============================================================
 * qx_memloc_flow_budget.c
 * QXMemloc — Ar [Air] — Budget Push and Callbacks
 *
 * The irrigation controller that notifies each field
 * of its water allocation. Subsystems register here
 * to receive budget updates. QXMemloc pushes new limits
 * when conditions change.
 *
 * ALAMTOLOGI — Quranic Science
 * Founded by Masa Bayu
 *
 * Owner : Masa Bayu
 * Date  : 2026-05-25
 * Repo  : https://github.com/qxengine/qxengine-core
 * ============================================================ */

#include "qx_memloc_flow_internal.h"

/* ── Internal: fire budget callbacks ───────────────────────── */

/*
 * flow_fire_callbacks
 *
 * Fires all matching registered callbacks with a budget
 * update. Must be called with callback_mutex UNLOCKED.
 * Unlocks and relocks around each callback invocation
 * to prevent deadlock if callback calls report_usage.
 */
void flow_fire_callbacks(
    struct QXMemlocFlow_s* flow,
    const char*            segment_id,
    QXSize                 new_limit_bytes,
    QXSize                 previous_limit_bytes,
    uint8_t                pressure_tier,
    uint8_t                lifecycle_state,
    QXTimestamp            updated_at_ms)
{
    QXBudgetUpdate update;
    uint32_t       i;

    memset(&update, 0, sizeof(update));
    update.new_limit_bytes      = new_limit_bytes;
    update.previous_limit_bytes = previous_limit_bytes;
    update.pressure_tier        = pressure_tier;
    update.lifecycle_state      = lifecycle_state;
    update.updated_at_ms        = updated_at_ms;

    if (segment_id != NULL) {
        strncpy(update.segment_id, segment_id,
                sizeof(update.segment_id) - 1u);
    }

    pthread_mutex_lock(&flow->callback_mutex);

    for (i = 0u; i < QX_FLOW_MAX_CALLBACKS; ++i) {
        QXFlowCallbackRecord* rec = &flow->callbacks[i];

        if (!rec->is_registered || rec->callback == NULL) {
            continue;
        }

        if (segment_id == NULL ||
            strncmp(rec->slot_id,
                    segment_id,
                    strnlen(segment_id, 64u)) == 0) {

            strncpy(update.slot_id, rec->slot_id,
                    sizeof(update.slot_id) - 1u);
            rec->current_limit = new_limit_bytes;

            /*
             * Unlock before calling — the callback must
             * be non-blocking and must be able to call
             * qx_flow_report_usage without deadlock.
             */
            pthread_mutex_unlock(&flow->callback_mutex);
            rec->callback(&update, rec->user_data);
            pthread_mutex_lock(&flow->callback_mutex);
        }
    }

    pthread_mutex_unlock(&flow->callback_mutex);
}

/* ── qx_flow_register_budget_callback ──────────────────────── */

QXResult qx_flow_register_budget_callback(
    QXMemlocFlowHandle flow,
    const char*        slot_id,
    QXBudgetCallback   callback,
    void*              user_data)
{
    uint32_t i;
    uint32_t free_slot = QX_FLOW_MAX_CALLBACKS;

    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (slot_id == NULL || callback == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&flow->callback_mutex);

    for (i = 0u; i < QX_FLOW_MAX_CALLBACKS; ++i) {
        if (!flow->callbacks[i].is_registered) {
            if (free_slot == QX_FLOW_MAX_CALLBACKS) {
                free_slot = i;
            }
            continue;
        }

        /*
         * Z.1 [Pattern] — one callback per slot_id.
         * Duplicate registration violates identity.
         */
        if (strncmp(flow->callbacks[i].slot_id,
                    slot_id, 64u) == 0) {
            pthread_mutex_unlock(&flow->callback_mutex);
            return QX_ERR_INVALID_ARGUMENT;
        }
    }

    if (free_slot == QX_FLOW_MAX_CALLBACKS) {
        pthread_mutex_unlock(&flow->callback_mutex);
        return QX_ERR_NO_SLOT;
    }

    strncpy(flow->callbacks[free_slot].slot_id,
            slot_id,
            sizeof(flow->callbacks[free_slot].slot_id) - 1u);
    flow->callbacks[free_slot].callback      = callback;
    flow->callbacks[free_slot].user_data     = user_data;
    flow->callbacks[free_slot].current_limit = 0u;
    flow->callbacks[free_slot].is_registered = QX_TRUE;
    flow->callback_count++;

    pthread_mutex_unlock(&flow->callback_mutex);
    return QX_OK;
}

/* ── qx_flow_deregister_budget_callback ─────────────────────── */

QXResult qx_flow_deregister_budget_callback(
    QXMemlocFlowHandle flow,
    const char*        slot_id)
{
    uint32_t i;

    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (slot_id == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&flow->callback_mutex);

    for (i = 0u; i < QX_FLOW_MAX_CALLBACKS; ++i) {
        if (flow->callbacks[i].is_registered &&
            strncmp(flow->callbacks[i].slot_id,
                    slot_id, 64u) == 0) {

            memset(&flow->callbacks[i], 0,
                   sizeof(QXFlowCallbackRecord));

            if (flow->callback_count > 0u) {
                flow->callback_count--;
            }

            pthread_mutex_unlock(&flow->callback_mutex);
            return QX_OK;
        }
    }

    pthread_mutex_unlock(&flow->callback_mutex);
    return QX_ERR_LEAF_NOT_FOUND;
}

/* ── qx_flow_report_usage ───────────────────────────────────── */

QXResult qx_flow_report_usage(
    QXMemlocFlowHandle   flow,
    const QXUsageReport* report)
{
    uint32_t i;
    uint32_t free_slot = QX_FLOW_MAX_USAGE_REPORTS;

    if (flow == NULL || !flow->is_initialised) {
        return QX_ERR_NULL_HANDLE;
    }
    if (report == NULL) {
        return QX_ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&flow->usage_mutex);

    for (i = 0u; i < QX_FLOW_MAX_USAGE_REPORTS; ++i) {
        if (strnlen(flow->usage_reports[i].slot_id,
                    64u) > 0u &&
            strncmp(flow->usage_reports[i].slot_id,
                    report->slot_id, 64u) == 0) {
            /* Update existing report in place */
            memcpy(&flow->usage_reports[i],
                   report, sizeof(QXUsageReport));
            pthread_mutex_unlock(&flow->usage_mutex);
            return QX_OK;
        }
        if (free_slot == QX_FLOW_MAX_USAGE_REPORTS &&
            strnlen(flow->usage_reports[i].slot_id,
                    64u) == 0u) {
            free_slot = i;
        }
    }

    if (free_slot == QX_FLOW_MAX_USAGE_REPORTS) {
        pthread_mutex_unlock(&flow->usage_mutex);
        return QX_ERR_NO_SLOT;
    }

    memcpy(&flow->usage_reports[free_slot],
           report, sizeof(QXUsageReport));
    flow->usage_report_count++;

    pthread_mutex_unlock(&flow->usage_mutex);
    return QX_OK;
}

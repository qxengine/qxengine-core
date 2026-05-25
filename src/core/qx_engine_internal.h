// =============================================================================
// QXEngine Core – src/core/qx_engine_internal.h
// Owner      : Masa Bayu
// Created    : 2026-05-25
// Repository : https://github.com/qxengine/qxengine-core
// Purpose    : Private engine state shared by focused implementation units.
// =============================================================================

#ifndef QXENGINE_CORE_QX_ENGINE_INTERNAL_H
#define QXENGINE_CORE_QX_ENGINE_INTERNAL_H

#include "qxengine/qxengine.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"
#include "qxengine/manifest/qx_manifest.h"
#include "qxengine/memory/qx_memloc.h"
#include "qxengine/memory/qx_pressure.h"
#include "qxengine/law/qx_law_enforcer.h"
#include "qxengine/law/qx_law_enforcer_types.h"
#include "qxengine/intelligence/qx_snapshot.h"
#include "qxengine/telemetry/qx_telemetry.h"
#include "qx_engine_memloc_bridge.h"

#include <atomic>
#include <cstring>
#include <ctime>
#include <mutex>
#include <shared_mutex>

inline QXTimestamp qx_engine_now_ms() noexcept {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<QXTimestamp>(ts.tv_sec) * 1000ULL
         + static_cast<QXTimestamp>(ts.tv_nsec) / 1'000'000ULL;
}

struct QXCapabilityRegistry {
    static constexpr uint32_t kMax = QX_EVAL_MAX_CAPABILITIES;

    char     declared[kMax][QX_CAPABILITY_ID_MAX];
    char     active[kMax][QX_CAPABILITY_ID_MAX];
    uint32_t declared_count{0};
    uint32_t active_count{0};

    void add_declared(const char* id) noexcept {
        if (!id || declared_count >= kMax) return;
        std::strncpy(declared[declared_count], id,
                     QX_CAPABILITY_ID_MAX - 1u);
        declared[declared_count][QX_CAPABILITY_ID_MAX - 1u] = '\0';
        ++declared_count;
    }

    void mark_active(const char* id) noexcept {
        if (!id || active_count >= kMax) return;
        for (uint32_t i = 0u; i < active_count; ++i) {
            if (std::strncmp(active[i], id, QX_CAPABILITY_ID_MAX) == 0)
                return;
        }
        std::strncpy(active[active_count], id, QX_CAPABILITY_ID_MAX - 1u);
        active[active_count][QX_CAPABILITY_ID_MAX - 1u] = '\0';
        ++active_count;
    }

    double utilisation_ratio() const noexcept {
        if (declared_count == 0u) return 0.0;
        return static_cast<double>(active_count)
             / static_cast<double>(declared_count);
    }
};

struct QXEngine_s {
    QXManifestHandle             manifest;
    QXMemlocHandle               memloc;
    QXPressureCoordinatorHandle  pressure;
    QXLawEnforcerHandle          enforcer;
    QXSnapshotHistoryHandle      snapshots;
    QXTelemetryHandle            telemetry;
    QXEngineMemlocBridge         memloc_bridge;

    QXEngineState   state{QX_ENGINE_STATE_UNCONFIGURED};
    mutable std::shared_mutex state_mtx;

    std::atomic<uint64_t> alloc_count{0};
    std::atomic<uint64_t> free_count{0};
    std::atomic<uint64_t> eval_count{0};
    std::atomic<uint64_t> snapshot_count{0};
    std::atomic<uint32_t> unlabelled_alloc_count{0};

    std::atomic<QXTimestamp> last_snapshot_ms{0};
    double knowledge_score{0.0};
    mutable std::mutex eval_mtx;

    QXBool ethics_dark_patterns{QX_FALSE};
    QXBool ethics_deceptive_flows{QX_FALSE};
    QXBool ethics_manipulative_ux{QX_FALSE};
    QXBool ethics_privacy_first{QX_FALSE};
    QXBool ethics_transparent_data{QX_FALSE};

    QXBool  native_first_policy{QX_FALSE};
    QXCapabilityRegistry capabilities;
    mutable std::mutex cap_mtx;

    double  battery_drain_pct{0.0};
    double  network_redundancy_pct{0.0};
    mutable std::mutex economy_mtx;

    QXTimestamp     created_at_ms{0};
    QXTimestamp     last_eval_ms{0};
    QXLawReport     last_report{};
    bool            last_report_valid{false};
};

static_assert(
    QX_EVAL_MAX_CAPABILITIES == QXCapabilityRegistry::kMax,
    "capability registry size must match QX_EVAL_MAX_CAPABILITIES"
);

#endif // QXENGINE_CORE_QX_ENGINE_INTERNAL_H

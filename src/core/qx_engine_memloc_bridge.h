// =============================================================================
// qx_engine_memloc_bridge.h
// QXEngine Core – Constitutional Memory Bridge
// =============================================================================
// Owner : Masa Bayu
// Created: 2026-05-25
// Repo   : https://github.com/qxengine/qxengine-core
// Path   : src/core/qx_engine_memloc_bridge.h
// License: Apache 2.0
//

#ifndef QXENGINE_CORE_QX_ENGINE_MEMLOC_BRIDGE_H
#define QXENGINE_CORE_QX_ENGINE_MEMLOC_BRIDGE_H

#include "qxengine/manifest/qx_manifest.h"
#include "qxengine/memory/qx_memloc_constitutional.h"
#include "qxengine/memory/qx_luman_init.h"

#define QX_ENGINE_MEMLOC_BINDING_CAPACITY 512u
#define QX_ENGINE_LEAF_ID_BYTES           37u

struct QXEngineMemlocBinding {
    char         legacy_leaf_id[QX_ENGINE_LEAF_ID_BYTES];
    QXLeafHandle constitutional_leaf{nullptr};
    QXBool       active{QX_FALSE};
};

struct QXEngineMemlocBridge {
    QXMemlocConstitutional  authority{};
    QXLumanInitResult       luman_result{};
    QXConstitutionalReport  last_report{};
    QXEngineMemlocBinding   bindings[QX_ENGINE_MEMLOC_BINDING_CAPACITY]{};
    uint8_t                 last_pressure_tier{0u};
    uint64_t                total_allocations{0u};
    uint64_t                total_deallocations{0u};
    QXBool                  initialized{QX_FALSE};
};

QXResult qx_bridge_create(
    QXEngineMemlocBridge* bridge,
    QXManifestHandle      manifest,
    QXSize                device_ram_bytes
);

void qx_bridge_destroy(QXEngineMemlocBridge* bridge);

QXResult qx_bridge_run_cycle(
    QXEngineMemlocBridge* bridge,
    QXTimestamp           now_ms,
    QXLawReport*          law_report
);

QXResult qx_bridge_notify_pressure(
    QXEngineMemlocBridge* bridge,
    uint8_t               pressure_tier
);

QXResult qx_bridge_notify_lifecycle(
    QXEngineMemlocBridge* bridge,
    uint8_t               lifecycle_state
);

QXResult qx_bridge_notify_screen(
    QXEngineMemlocBridge* bridge,
    const char*           screen_profile_id
);

QXResult qx_bridge_allocate(
    QXEngineMemlocBridge*       bridge,
    const QXAllocationRequest*  request,
    QXConstitutionalAllocation* out_allocation
);

QXResult qx_bridge_bind_legacy_leaf(
    QXEngineMemlocBridge* bridge,
    const char*           legacy_leaf_id,
    QXLeafHandle          constitutional_leaf
);

QXResult qx_bridge_deallocate_legacy(
    QXEngineMemlocBridge* bridge,
    const char*           legacy_leaf_id
);

double qx_bridge_health_score(const QXEngineMemlocBridge* bridge);

QXCertificationTier qx_bridge_certification(
    const QXEngineMemlocBridge* bridge
);

#endif // QXENGINE_CORE_QX_ENGINE_MEMLOC_BRIDGE_H

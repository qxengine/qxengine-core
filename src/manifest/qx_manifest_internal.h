// =============================================================================
// QXEngine Core – src/manifest/qx_manifest_internal.h
// Owner : Masa Bayu
// Created: 2026-05-25
// Repo   : https://github.com/qxengine/qxengine-core
// Purpose: Private manifest state and helper boundaries for split sources.
// =============================================================================

#ifndef QXENGINE_MANIFEST_QX_MANIFEST_INTERNAL_H
#define QXENGINE_MANIFEST_QX_MANIFEST_INTERNAL_H

#include "qxengine/manifest/qx_manifest.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_constants.h"
#include "qxengine/core/qx_error.h"

#include <mutex>

struct QXManifest_s {
    QXManifestIdentity      identity;
    QXManifestLawCompliance law_compliance;
    QXManifestLimit         limit;
    QXManifestEthics        ethics;
    QXManifestCreativity    creativity;
    QXManifestKnowledge     knowledge;
    QXManifestEconomy       economy;
    QXManifestVerticals     verticals;
    QXManifestMeta          meta;
    mutable std::mutex      mtx;
};

bool qx_manifest_parse_json(
    const char* json,
    uint32_t    len,
    QXManifest_s* manifest
);

void qx_manifest_run_all_rules(
    const QXManifest_s* manifest,
    QXManifestValidationResult* result
);

#endif // QXENGINE_MANIFEST_QX_MANIFEST_INTERNAL_H

// =============================================================================
// test_manifest_golden.cpp
// QXEngine Core — Golden Manifest Conformance Tests
// =============================================================================
// Owner : Masa Bayu
// Created: 2026-05-24
// Repo   : https://github.com/qxengine/qxengine-core
// Description: Verifies canonical example manifests against the core parser.
// =============================================================================

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "qxengine/core/qx_error.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/manifest/qx_manifest.h"

namespace {

std::string read_manifest(const char* relative_path) {
    std::ifstream input(relative_path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

QXResult parse_manifest_file(
    const char* relative_path,
    QXManifestValidationResult* out_result)
{
    const std::string json = read_manifest(relative_path);
    if (json.empty()) {
        return QX_ERR_MANIFEST_NOT_FOUND;
    }

    QXManifestHandle manifest = nullptr;
    const QXResult rc = qx_manifest_parse(
        json.c_str(),
        static_cast<uint32_t>(json.size()),
        &manifest,
        out_result);
    qx_manifest_destroy(manifest);
    return rc;
}

} // namespace

TEST(QXManifestGolden, QiubbxReferenceManifestPasses) {
    QXManifestValidationResult result{};
    EXPECT_EQ(parse_manifest_file(
        "../qxengine-examples/manifests/qiubbx.qxmanifest",
        &result), QX_OK);
    EXPECT_EQ(result.is_valid, QX_TRUE);
}

TEST(QXManifestGolden, MinimalReferenceManifestPasses) {
    QXManifestValidationResult result{};
    EXPECT_EQ(parse_manifest_file(
        "../qxengine-examples/manifests/minimal-app.qxmanifest",
        &result), QX_OK);
    EXPECT_EQ(result.is_valid, QX_TRUE);
}

TEST(QXManifestGolden, FailingReferenceManifestFails) {
    QXManifestValidationResult result{};
    EXPECT_NE(parse_manifest_file(
        "../qxengine-examples/manifests/failing-example.qxmanifest",
        &result), QX_OK);
    EXPECT_EQ(result.is_valid, QX_FALSE);
    EXPECT_GT(result.error_count, 0u);
}

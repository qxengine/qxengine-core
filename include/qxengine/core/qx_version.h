/* ============================================================================
 * QXEngine Core – qx_version.h  (auto-generated, do not edit)
 * Owner:      Masa Bayu
 * Created:    2026-05-24
 * Generated:  /Users/masabayu/Desktop/qxengine/qxengine-core
 * Repository: https://github.com/qxengine/qxengine-core
 * ============================================================================
 */

#ifndef QX_VERSION_H
#define QX_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define QX_VERSION_MAJOR    1
#define QX_VERSION_MINOR    0
#define QX_VERSION_PATCH    0
#define QX_VERSION_TWEAK    0

#define QX_VERSION_STRING   "1.0.0"
#define QX_SPEC_VERSION     "1.0.0"
#define QX_SCHEMA_URL       "https://qxengine.io/manifest/v1.0/schema.json"
#define QX_OWNER            "Masa Bayu"
#define QX_HOMEPAGE         "https://qxengine.io"
#define QX_ABI_VERSION      1

/**
 * @brief Runtime version check.
 * Returns 1 if the runtime engine version matches the compile-time version.
 */
static inline int qx_version_check(int major, int minor, int patch) {
    return (major == QX_VERSION_MAJOR &&
            minor == QX_VERSION_MINOR &&
            patch == QX_VERSION_PATCH) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* QX_VERSION_H */

# QXEngine Core — Integration Guide: CLI Bridge

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

The CLI bridge describes how to consume the QXEngine C ABI from a
command-line tool, scripting environment, or test harness. Because every
public QXEngine symbol is `extern "C"`, the engine shared library can be
loaded directly by any language with a C FFI — no wrapper generator or
binding tool is required.

---

## Use Cases

- **CI/CD validation** — run manifest validation and law evaluation as
  part of a build pipeline without deploying to a device.
- **Python tooling** — drive QXEngine from a Python script using
  `ctypes` or `cffi` for rapid prototyping and data collection.
- **C test harness** — link `libqxengine.so` directly from a C driver
  for integration testing outside of GoogleTest.
- **Cross-language bindings** — the same pattern applies to Ruby, Rust
  (`bindgen`), Go (`cgo`), and any other language with a C FFI.

---

## Prerequisites

| Requirement | Notes |
|---|---|
| Built `libqxengine.so` / `libqxengine.a` | From root `CMakeLists.txt` |
| `include/qxengine/` headers on include path | For C callers |
| Python 3.10+ | For Python `ctypes` example |
| `ctypes` module | Bundled with CPython |

---

## Building the Shared Library

Build QXEngine as a shared library from the repo root:

```bash
cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON

cmake --build build --config Release
```

The output is `build/libqxengine.so` (Linux/Android) or
`build/libqxengine.dylib` (macOS/iOS simulator).

---

## C Driver Example

A minimal C driver that parses a manifest, creates an engine, runs one
evaluation, and prints the health score:

```c
#include <stdio.h>
#include <string.h>
#include "qxengine/qxengine.h"
#include "qxengine/manifest/qx_manifest.h"
#include "qxengine/law/qx_law_report.h"

static const char* load_file(const char* path);   /* implementation omitted */

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: qx_cli <manifest.json>\n");
        return 1;
    }

    const char* json = load_file(argv[1]);

    QXManifestHandle manifest = NULL;
    QXManifestValidationResult result;
    memset(&result, 0, sizeof(result));

    if (qx_manifest_parse(json, &manifest, &result) != QX_OK
        || !result.is_valid) {
        fprintf(stderr, "manifest invalid: %u error(s)\n", result.error_count);
        for (uint32_t i = 0; i < result.error_count; ++i)
            fprintf(stderr, "  [%s] %s\n",
                    result.errors[i].rule_id,
                    result.errors[i].message);
        return 2;
    }

    QXEngineConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.manifest = manifest;

    QXEngineHandle engine = NULL;
    if (qx_engine_create(&cfg, &engine) != QX_OK) {
        fprintf(stderr, "engine create failed\n");
        qx_manifest_destroy(manifest);
        return 3;
    }

    QXLawReport report;
    memset(&report, 0, sizeof(report));
    qx_engine_evaluate(engine, &report);

    printf("health_score  : %.2f\n", report.health_score);
    printf("certified     : %s\n",
           qx_report_is_certified(&report) ? "yes" : "no");
    printf("certification : %s\n",
           qx_certification_label(report.certification));

    qx_engine_destroy(engine);
    qx_manifest_destroy(manifest);
    return 0;
}
```

Compile and link:

```bash
gcc -std=c11 qx_cli.c \
    -I qxengine-core/include \
    -L build -lqxengine \
    -o qx_cli
```

---

## Python ctypes Example

Load `libqxengine.so` dynamically and call the C ABI from Python:

```python
import ctypes
import json
import pathlib

lib = ctypes.CDLL("build/libqxengine.so")

# --- type declarations ---
lib.qx_manifest_parse.argtypes  = [ctypes.c_char_p,
                                    ctypes.POINTER(ctypes.c_void_p),
                                    ctypes.c_void_p]
lib.qx_manifest_parse.restype   = ctypes.c_uint32

lib.qx_engine_create.argtypes   = [ctypes.c_void_p,
                                    ctypes.POINTER(ctypes.c_void_p)]
lib.qx_engine_create.restype    = ctypes.c_uint32

lib.qx_engine_evaluate.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
lib.qx_engine_evaluate.restype  = ctypes.c_uint32

lib.qx_engine_destroy.argtypes  = [ctypes.c_void_p]
lib.qx_engine_destroy.restype   = None

lib.qx_manifest_destroy.argtypes = [ctypes.c_void_p]
lib.qx_manifest_destroy.restype  = None

# --- load manifest ---
manifest_json = pathlib.Path("qxengine_manifest.json").read_bytes()

manifest_handle = ctypes.c_void_p(0)
result_buf      = (ctypes.c_uint8 * 4096)()   # QXManifestValidationResult

err = lib.qx_manifest_parse(manifest_json,
                              ctypes.byref(manifest_handle),
                              result_buf)
assert err == 0, f"manifest parse failed: err={err}"

# --- create engine ---
class QXEngineConfig(ctypes.Structure):
    _fields_ = [("manifest", ctypes.c_void_p)]

cfg = QXEngineConfig(manifest=manifest_handle.value)
engine_handle = ctypes.c_void_p(0)
err = lib.qx_engine_create(ctypes.byref(cfg), ctypes.byref(engine_handle))
assert err == 0, f"engine create failed: err={err}"

# --- evaluate ---
report_buf = (ctypes.c_uint8 * 8192)()   # QXLawReport
err = lib.qx_engine_evaluate(engine_handle, report_buf)
assert err == 0

# health_score is the first double in QXLawReport
health_score = ctypes.c_double.from_buffer(report_buf).value
print(f"health_score: {health_score:.2f}")

# --- teardown ---
lib.qx_engine_destroy(engine_handle)
lib.qx_manifest_destroy(manifest_handle)
```

---

## Continuous Evaluation Loop

For CI/CD pipelines that need repeated evaluation:

```bash
#!/usr/bin/env bash
# run_eval.sh — evaluate QXEngine health score N times
MANIFEST=$1
ITERATIONS=${2:-10}

for i in $(seq 1 "$ITERATIONS"); do
    ./qx_cli "$MANIFEST" | grep health_score
    sleep 1
done
```

Run and capture output:

```bash
chmod +x run_eval.sh
./run_eval.sh qxengine_manifest.json 5
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `cannot open shared object file` | Library not on `LD_LIBRARY_PATH` | `export LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH` |
| `manifest invalid: 0 error(s)` | Parse error before validation | Check JSON syntax with a linter |
| Python `OSError` on `CDLL` | Wrong library path | Use absolute path or `pathlib.Path.resolve()` |
| Health score is 0.0 | Engine not evaluated | Call `qx_engine_evaluate` before reading report |
| Segfault in Python ctypes | Wrong struct layout | Verify `QXEngineConfig` field order matches header |

---

## Related Documents

- [`../architecture/c-abi-boundary.md`](../architecture/c-abi-boundary.md) — C ABI rules
- [`../api/core.md`](../api/core.md) — Error codes and types
- [`../api/manifest.md`](../api/manifest.md) — Manifest format and MFT rules
- [`android-ndk.md`](android-ndk.md) — Android NDK integration
- [`ios-xcframework.md`](ios-xcframework.md) — iOS XCFramework integration

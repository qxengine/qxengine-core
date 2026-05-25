# QXEngine Core — Integration Guide: iOS XCFramework

**Repository:** https://github.com/qxengine/qxengine-core
**Owner:** Masa Bayu
**Created:** 2026-05-24

---

## Overview

This guide explains how to build QXEngine Core as an XCFramework for
iOS and integrate it into an Xcode project. The Swift bridge
(`qx_swift_bridge.cpp`) exposes the engine C ABI to Swift callers via
a thin C wrapper layer consumed through a bridging header.

---

## Prerequisites

| Requirement | Minimum version |
|---|---|
| Xcode | 15.0 |
| CMake | 3.22 |
| iOS deployment target | 14.0 |
| Swift | 5.9 |
| Target architectures | `arm64` (device), `arm64` + `x86_64` (simulator) |

---

## Recommended Project Structure

```
MyApp/
├── MyApp.xcodeproj
├── MyApp/
│   ├── QXEngine/
│   │   ├── QXEngineWrapper.swift      ← Swift wrapper
│   │   └── QXEngine-Bridging-Header.h ← C bridging header
│   └── Resources/
│       └── qxengine_manifest.json
└── qxengine-core/                     ← git submodule
    └── platforms/ios/
        ├── CMakeLists.txt
        └── bridge/qx_swift_bridge.cpp
```

---

## CMake Build

Build the static library for device and simulator separately, then
package both into an XCFramework using the provided shell script.

**Device build (arm64):**

```bash
cmake -S qxengine-core/platforms/ios \
      -B build/ios-device \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
      -DCMAKE_BUILD_TYPE=Release

cmake --build build/ios-device --config Release
```

**Simulator build (arm64 + x86_64):**

```bash
cmake -S qxengine-core/platforms/ios \
      -B build/ios-simulator \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
      -DCMAKE_OSX_SYSROOT=iphonesimulator \
      -DCMAKE_BUILD_TYPE=Release

cmake --build build/ios-simulator --config Release
```

---

## XCFramework Packaging

After building both slices, create the XCFramework:

```bash
xcodebuild -create-xcframework \
    -library build/ios-device/lib/ios/libqxengine_ios_bridge.a \
    -headers qxengine-core/include \
    -library build/ios-simulator/lib/ios/libqxengine_ios_bridge.a \
    -headers qxengine-core/include \
    -output QXEngine.xcframework
```

The resulting `QXEngine.xcframework` can be dragged directly into your
Xcode project.

---

## Xcode Integration

1. Drag `QXEngine.xcframework` into your Xcode project navigator.
2. In **Target → General → Frameworks, Libraries, and Embedded Content**,
   set the framework to **Do Not Embed** (it is a static library).
3. In **Target → Build Settings → Swift Compiler — General**, set
   **Objective-C Bridging Header** to
   `MyApp/QXEngine/QXEngine-Bridging-Header.h`.
4. Add `qxengine-core/include` to **Header Search Paths**.

---

## Bridging Header

Create `MyApp/QXEngine/QXEngine-Bridging-Header.h`:

```c
#ifndef QXEngine_Bridging_Header_h
#define QXEngine_Bridging_Header_h

#include "qxengine/qxengine.h"
#include "qxengine/core/qx_types.h"
#include "qxengine/core/qx_error.h"
#include "platforms/ios/bridge/qx_swift_bridge.h"

#endif
```

This exposes all `qx_ios_*` functions and the core QXEngine types
directly to Swift without any ObjC runtime involvement.

---

## Swift Wrapper

```swift
import Foundation

final class QXEngineWrapper {

    private var handle: OpaquePointer?

    init(manifestJSON: String) throws {
        guard let h = manifestJSON.withCString({ qx_ios_engine_create($0) }) else {
            throw QXError.engineCreateFailed
        }
        handle = OpaquePointer(h)
    }

    deinit {
        if let h = handle {
            qx_ios_engine_destroy(UnsafeMutableRawPointer(h))
            handle = nil
        }
    }

    func evaluate() -> Double {
        guard let h = handle else { return -1.0 }
        return qx_ios_engine_evaluate(UnsafeMutableRawPointer(h))
    }

    func snapshot() -> Double {
        guard let h = handle else { return -1.0 }
        return qx_ios_engine_snapshot(UnsafeMutableRawPointer(h))
    }

    func handleMemoryWarning(ramUsedRatio: Double) {
        guard let h = handle else { return }
        _ = qx_ios_engine_pressure_warning(UnsafeMutableRawPointer(h),
                                            ramUsedRatio)
    }

    enum QXError: Error {
        case engineCreateFailed
    }
}
```

---

## Loading the Manifest

Load the manifest JSON from the app bundle before initialising the
wrapper:

```swift
guard let url = Bundle.main.url(forResource: "qxengine_manifest",
                                 withExtension: "json"),
      let json = try? String(contentsOf: url, encoding: .utf8) else {
    fatalError("QXEngine: missing manifest JSON in bundle")
}
let engine = try QXEngineWrapper(manifestJSON: json)
```

---

## Handling Memory Pressure

Respond to iOS memory warnings by forwarding the RAM usage ratio:

```swift
NotificationCenter.default.addObserver(
    forName: UIApplication.didReceiveMemoryWarningNotification,
    object: nil, queue: .main
) { [weak engine] _ in
    let ratio = Double(os_proc_available_memory()) /
                Double(ProcessInfo.processInfo.physicalMemory)
    let used  = max(0.0, 1.0 - ratio)
    engine?.handleMemoryWarning(ramUsedRatio: used)
}
```

QXEngine maps the RAM used ratio to pressure tiers automatically:

| RAM used ratio | QXEngine tier |
|---|---|
| < 0.85 | Tier 3 — High |
| ≥ 0.85 | Tier 4 — Critical |

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `qx_ios_engine_create` returns nil | Manifest validation failed | Check MFT rules in `docs/api/manifest.md` |
| Linker error: symbol not found | Missing bridging header include | Confirm all headers in bridging header |
| Simulator build fails | Architecture mismatch | Build simulator slice with `arm64;x86_64` |
| Memory warning not triggering eviction | RAM ratio too low | Test with ratio ≥ 0.85 for tier 4 |
| XCFramework not found at runtime | Embed setting wrong | Set to **Do Not Embed** for static library |

---

## Related Documents

- [`../architecture/overview.md`](../architecture/overview.md) — Engine architecture
- [`../api/manifest.md`](../api/manifest.md) — Manifest format and MFT rules
- [`../api/memory.md`](../api/memory.md) — Memory pressure API
- [`android-ndk.md`](android-ndk.md) — Android NDK integration guide
- [`../../platforms/ios/bridge/qx_swift_bridge.cpp`](../../platforms/ios/bridge/qx_swift_bridge.cpp) — Swift bridge source

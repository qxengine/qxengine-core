# QXEngine Core

QXEngine Core is the C++ reference implementation of the QXEngine Universal
Integrity Engine.

It provides:

- A C++17 core runtime.
- A stable C ABI for platform bindings.
- Manifest parsing and validation.
- Constitutional law evaluation.
- Memory, telemetry, and integrity flow evidence.
- iOS and Android bridge sources.
- Unit and integration tests.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DQX_BUILD_TESTS=ON
cmake --build build --config Debug
```

## Test

```sh
ctest --test-dir build --output-on-failure
ctest --test-dir build -L integration --output-on-failure
```

## Public API

The main public entry point is:

```text
include/qxengine/qxengine.h
```

This header exposes the C ABI used by iOS, Android, CLI, and future language
bindings.

## Repository Scope

This repository is for the core engine only.

Platform packages and tools should live in separate repositories:

- `qxengine-ios`
- `qxengine-android`
- `qxengine-cli`
- `qxengine`

## Standard

The core follows QXEngine v1.0:

- Eight constitutional laws.
- Nine canonical memory segments.
- Seven-stage integrity flow.
- Caller-owned C ABI output structs.
- Files kept at or below 500 lines.

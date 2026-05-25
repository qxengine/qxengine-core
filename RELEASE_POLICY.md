# QXEngine Release Policy

## Purpose

QXEngine is distributed openly first.

The release system is designed to make every public artifact traceable,
versioned, and verifiable before any commercial distribution layer exists.
QIUBBX remains the first proof point for the engine.

## Distribution Rule

QXEngine releases are published through GitHub Releases.

Each release must include:

- A semantic version tag.
- A built artifact or source package for the repository.
- `SHA256SUMS.txt` for artifact verification.
- CI evidence that the package builds before publication.

## Versioning

Release tags must follow semantic versioning:

```text
vMAJOR.MINOR.PATCH
```

Pre-release tags are allowed for release candidates:

```text
v1.0.0-rc.1
```

Version meaning:

- `MAJOR` changes when public APIs or manifest contracts break.
- `MINOR` changes when compatible features are added.
- `PATCH` changes when compatible fixes are shipped.

## Release Artifacts

`qxengine-core` publishes a release install tree containing the core library,
headers, and CMake package files.

Every artifact is accompanied by SHA-256 checksums.

## Verification

Users should verify downloaded artifacts before use:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

On Linux, this is also valid:

```sh
sha256sum -c SHA256SUMS.txt
```

## Commercial Layer

Commercial distribution is not part of the first release path.

The engine must prove itself openly through QIUBBX first. Paid packaging,
enterprise services, hosted dashboards, or certification products can be built
later only after the open engine is stable, verifiable, and trusted.

## Release Gates

Before publishing a release:

- Core must build and pass tests.
- Release artifacts must be checksumed.
- The GitHub Release must be created from the exact Git tag.


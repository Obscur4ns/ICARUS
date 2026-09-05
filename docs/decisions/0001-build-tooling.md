# 0001 - Build tooling

Status: Accepted

## Decision

ICARUS uses HEMTT for ArmA addon work and CMake/MSVC for native Windows components.

HEMTT is the supported path for addon linting, development builds, PBO construction, ArmA launch profiles, signing and ArmA release packaging.

CMake is the supported path for the shared native core, ArmA extension, TeamSpeak plugin and native tests.

## Reasons

The ArmA and native parts of ICARUS have different build requirements. Keeping their build ownership separate gives each tool a narrow job and makes local and CI builds reproducible.

The project does not maintain parallel Mikero, Addon Builder and HEMTT build definitions.

## Consequences

- Contributors need HEMTT for ArmA work.
- Native contributors need a supported C++20 toolchain.
- CI validates both build paths.
- Native release packaging must explicitly combine CMake outputs with the HEMTT release rather than relying on implicit local files.

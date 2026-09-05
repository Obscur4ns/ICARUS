# Building

ICARUS uses HEMTT for the ArmA addon and CMake/MSVC for native components.

## Requirements

### ArmA

- HEMTT 1.21.0.
- ArmA 3 Tools for local binarized builds.
- Git.

### Native

- Visual Studio 2022 with the Desktop development with C++ workload.
- CMake 3.25 or newer.
- Git.
- TeamSpeak 3 Plugin SDK API 26 when building the TeamSpeak plugin.

## Project version

`addons/main/script_version.hpp` is the project version source.

HEMTT reads it directly. CMake also reads the same file during configuration so the ArmA addon, extension and TeamSpeak plugin do not maintain separate version numbers.

## ArmA validation

Fast validation:

```powershell
hemtt check --pedantic
```

Development build:

```powershell
hemtt dev
```

Local test build:

```powershell
hemtt build
```

Launch ArmA with the development build:

```powershell
hemtt launch
```

Public release packaging will use `hemtt release` once the native packaging step is in place.

## TeamSpeak Plugin SDK

The TeamSpeak SDK is not committed to the repository.

Fetch the pinned API 26 revision:

```powershell
.\tools\fetch-ts3-sdk.ps1
```

It is stored under `.deps/ts3client-pluginsdk`, which is ignored by Git.

## Native build

Build the core and ArmA extension:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug
```

Build the core, ArmA extension and TeamSpeak plugin:

```powershell
.\tools\fetch-ts3-sdk.ps1
cmake --preset windows-x64-ts3
cmake --build --preset windows-x64-ts3-debug
ctest --preset windows-x64-ts3-debug
```

Release builds use the matching `-release` presets.

## Current outputs

The ArmA extension is built as:

```text
icarus_x64.dll
```

The TeamSpeak plugin is built as:

```text
icarus.dll
```

Native packaging into the HEMTT mod output is intentionally not automated yet. The first native milestone is to prove that each binary builds and loads before the release pipeline starts moving binaries around automatically.

## Extension smoke test

Once `icarus_x64.dll` is available to ArmA and the `icarus_main` addon is loaded:

```sqf
[] call ICARUS_fnc_extensionPing
```

should return:

```text
pong
```

The shared project version is available with:

```sqf
[] call ICARUS_fnc_extensionVersion
```

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

## Standard development workflow

The normal ICARUS development workflow is:

```powershell
.\tools\deploy-dev.ps1
# Development

## Branches

`main` is the stable project baseline.

`development` is the permanent integration branch.

Normal work is done on short-lived branches created from an up-to-date `development` branch:

```text
feature/<name>
fix/<name>
refactor/<name>
docs/<name>
```

A branch returns to `development` after its build, tests and review pass. Tested integration checkpoints are promoted from `development` to `main`.

Direct work on `main` should be exceptional.

## Commits

Keep commits narrow and testable. A commit should normally contain one coherent change.

Commit messages describe the change, not the development process.

Examples:

```text
Add HEMTT project configuration
Add native extension smoke-test entry points
Fix radio identity replication after JIP
```

## Build ownership

HEMTT owns:

- ArmA addon validation.
- PBO construction.
- Development links.
- ArmA launch profiles.
- Signing and ArmA release packaging.

CMake owns:

- Shared native code.
- ArmA extension builds.
- TeamSpeak plugin builds.
- Native tests and compiler configuration.

Neither build system should be stretched into replacing the other.

## Code

- C++20 for native components.
- SQF/config for ArmA-side code.
- CMake with MSVC for Windows native builds.
- Compiler warnings are build failures.
- Prefer specific names over generic manager or handler classes.
- Keep interfaces narrow.
- Do not put blocking work in real-time audio callbacks.
- Validate data crossing ArmA, native and TeamSpeak boundaries.
- Keep one term for one concept throughout code and documentation.
- Comments explain constraints or non-obvious reasons rather than restating code.

## State ownership

Authoritative gameplay state belongs to the ArmA simulation/server side.

UI, programming tools, the native extension and TeamSpeak plugin consume or request changes to that state; they do not maintain competing authoritative radio databases.

## Third-party work

Reference implementations may be inspected when useful.

Copied or adapted code and assets retain required attribution, notices and licence terms. Third-party provenance is not obscured.

Dependencies should be external or isolated rather than copied into unrelated ICARUS source directories.

## Testing

Every cross-process or replicated state path needs failure testing as well as a successful-path test.

Relevant cases include:

- Dedicated server.
- JIP.
- Disconnect and reconnect.
- Respawn.
- Equipment transfer.
- TeamSpeak disconnect and reconnect.
- Plugin reload.
- Version mismatch.
- Invalid or stale IPC data.
- Missing native extension.
- Missing TeamSpeak plugin.

Performance-sensitive systems should expose diagnostics before complex optimisation is introduced.

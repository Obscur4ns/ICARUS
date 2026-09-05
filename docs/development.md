# Development

## Repository

`main` is the authoritative project state.

Changes should be made in small, testable commits. A commit should normally introduce one subsystem, one refactor or one documented decision rather than several unrelated changes.

## Code style

- Prefer specific names over generic manager/handler classes.
- Comments explain constraints, reasons and non-obvious behaviour.
- Do not comment code that is already self-explanatory.
- Avoid abstractions that have no current architectural purpose.
- Keep interfaces narrow.
- Keep real-time audio paths allocation-light and non-blocking.
- Validate all data crossing ArmA/native/TeamSpeak boundaries.
- Use one term consistently for one concept.

## Languages and tools

- ArmA: SQF and config.
- Native: C++20.
- Native build: CMake with MSVC on Windows x64.
- ArmA packaging scripts will be added once the initial addon layout exists.

## Third-party work

Reference implementations may be inspected when useful.

Any copied or adapted code or assets must retain required attribution, notices and licence terms. New ICARUS code should not disguise third-party provenance.

Third-party source should live in a clearly identified location or be brought in through an explicit dependency rather than copied into unrelated project files.

## Testing

Every cross-process or replicated state path needs failure testing, not only the successful path.

At minimum, new systems should consider:

- Dedicated server.
- JIP.
- Disconnect/reconnect.
- Respawn.
- Equipment transfer.
- TeamSpeak disconnect/reconnect.
- Plugin reload.
- Version mismatch.
- Invalid or stale IPC data.
- Missing native extension.
- Missing TeamSpeak plugin.

Performance-sensitive systems should have diagnostics before they have complex optimisation.

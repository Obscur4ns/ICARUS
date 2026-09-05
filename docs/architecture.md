# Architecture

ICARUS is split into systems with deliberately narrow ownership. UI code, programming tools, RF simulation and voice processing do not keep competing copies of authoritative radio state.

## Components

### ArmA addon

Responsible for:

- Physical equipment and inventory integration.
- Player, vehicle and world state.
- Radio and audio-device definitions.
- Input and UI.
- Mission modules.
- Server-authoritative radio identities and gameplay state.
- Multiplayer replication.
- Calls into the native extension.

The ArmA server is authoritative for gameplay state that affects other players.

### Native core

A shared C++ library for code that must behave identically in the ArmA extension and voice backend.

Expected responsibilities include:

- Shared data structures.
- IPC protocol types and versioning.
- RF calculation primitives.
- Waveform and signal-quality models that are safe to share.
- Audio/DSP utilities that do not depend on TeamSpeak callbacks.
- Serialization and validation.

The native core does not become a second authoritative multiplayer database.

### Native IPC

A Windows-native library shared by the ArmA extension and voice backend.

The process bridge uses named shared memory for process presence, protocol negotiation, heartbeat and health state.

Session state uses a separate named shared-memory mapping with independently versioned snapshot layouts.

Transport-specific code remains behind the `icarus::ipc` interface so later data paths can use a different mechanism without changing radio state ownership.

Neither mapping is a voice transport.

### ArmA extension

Responsible for high-cost or native-only work requested by the ArmA addon.

Expected responsibilities include:

- RF calculation work that is unsuitable for SQF.
- Local IPC with the voice backend.
- Native platform services.
- Bounded, validated exchange of state between ArmA and the local voice plugin.

The extension must not stream player voice through ArmA.

### TeamSpeak plugin

TeamSpeak 3 is the first voice backend.

The plugin is responsible for:

- TeamSpeak client identity mapping.
- Positional voice handling.
- Radio receive/transmit audio processing.
- Channel muting/routing required by the simulation.
- DSP.
- Ear routing.
- Sidetone.
- Safe restoration of ordinary TeamSpeak behaviour when ICARUS is inactive.

Real-time audio callbacks must not wait on ArmA, the network or expensive RF calculations.

## State ownership

The logical radio state has one source of authority in the game simulation.

```text
UI / KDU / laptop / module
            |
            v
      ICARUS radio state
            |
      +-----+-----+
      |           |
      v           v
 RF/link state   voice/audio state
```

Programming tools submit changes to the radio model. They do not own a separate copy of the radio.

The voice backend consumes the local state it needs to render audio. It does not decide multiplayer radio ownership, key possession or inventory state.

## Local process transport

ICARUS uses two independently versioned local transports.

```text
Local\ICARUS.Bridge.1
    process discovery
    heartbeat
    health
    session generation

Local\ICARUS.SessionState.1
    ArmA player snapshot
    voice-backend snapshot
    state acknowledgement
```

The ArmA extension owns the session generation and creates the session-state mapping.

Each state direction has a single writer and uses sequence-validated snapshots so readers do not accept partially-written state.

TeamSpeak transports voice between clients. Voice samples are processed on the TeamSpeak side rather than being routed through SQF or the shared-memory state mappings.

## Environmental audio pickup

Environmental pickup is part of the microphone model.

The implementation must avoid feeding received ICARUS/TeamSpeak audio back into outgoing radio transmissions. The audio source and filtering approach therefore needs to be designed separately from ordinary TeamSpeak playback.

If a physical in-world speaker is intentionally audible to a nearby microphone, that is a simulated acoustic path rather than an accidental software feedback loop.

## RF processing

RF calculations produce link state. Audio code consumes that state.

A receiver should receive information such as signal level, interference, SNR/SINR and decode quality rather than a simple in-range boolean. Waveform logic then determines whether and how the transmission is intelligible.

## Extensibility

Hardware definitions should be data-driven where practical. Core code must not contain branches for individual radio models unless the behaviour is genuinely framework-level.

Third-party equipment should use public definitions and APIs rather than patching private implementation details.

## Initial platform

- ArmA 3, Windows x64.
- TeamSpeak 3 as the initial voice backend.
- SQF/config for ArmA-side code.
- C++ for native components.
- CMake/MSVC for native builds.

Another voice backend may be added later without replacing the radio state model.

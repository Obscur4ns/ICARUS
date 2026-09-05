# Architecture

ICARUS is split into systems with deliberately narrow ownership. UI code, programming tools, RF simulation and voice processing do not keep competing copies of authoritative radio state.

## Components

### ArmA addon

Responsible for physical equipment, player and world state, UI/input, mission integration, multiplayer replication and the authoritative gameplay-side radio model.

The ArmA server remains authoritative for gameplay state that affects other players.

### Native core

Shared C++ types and calculations used by both native processes. This includes protocol definitions, validation, RF primitives and DSP utilities that do not depend on TeamSpeak callbacks.

The native core is not a second multiplayer authority.

### Native IPC

Windows-local transport shared by the ArmA extension and voice backend.

Current mappings are independently versioned:

```text
Local\ICARUS.Bridge.1
Local\ICARUS.SessionState.1
Local\ICARUS.SpatialScene.1
```

The process bridge carries discovery, heartbeat, process health and session generation. Session state carries the local player's detailed state and voice-backend acknowledgements. Spatial scene carries a bounded local view of ArmA player actors for later positional audio processing.

None of these mappings carry voice samples.

### ArmA extension

Responsible for native work requested by the ArmA addon and bounded local exchange with the voice backend. It does not stream player voice through SQF or ArmA networking.

### TeamSpeak plugin

TeamSpeak 3 is the initial voice backend. The plugin owns real-time voice behaviour, TeamSpeak client identity mapping, positional playback, DSP, ear routing and safe restoration of ordinary TeamSpeak behaviour when ICARUS is inactive.

Real-time audio callbacks must never wait on ArmA, IPC, network I/O or expensive RF work.

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

Programming tools submit changes to the radio model. They do not own separate copies of the radio.

The voice backend consumes state required to render audio. It does not decide inventory ownership, key possession or multiplayer radio authority.

Direct-voice identity mapping never uses display nicknames. ArmA player UID and network identity are joined to TeamSpeak's callback-provided client ID and unique identity. TeamSpeak identity discovery is not game authority.

## Local spatial scene

Each ArmA client already receives remote-player movement through normal ArmA replication. ICARUS samples that local view and publishes a bounded scene to its local extension at approximately 10 Hz.

A spatial actor contains:

- ArmA player UID and network ID.
- Eye position and head direction.
- World velocity.
- Alive and in-vehicle state.
- Direct-voice level.

The initial capacity is 256 actors. ICARUS targets 120-player sessions, leaving headroom without making the shared-memory page unbounded.

Velocity is transported with position so the later acoustic renderer can interpolate or extrapolate between ArmA snapshots rather than coupling audio placement to the 10 Hz scene rate.

## Voice transport

TeamSpeak transports voice between clients. Voice samples are processed on the TeamSpeak side rather than being routed through SQF or shared memory.

TeamSpeak plugin commands are used only for low-frequency ICARUS identity discovery. They are not used for player positions, per-frame state or audio samples.

## Environmental audio pickup

Environmental pickup is part of the microphone model. The implementation must avoid feeding received ICARUS/TeamSpeak audio back into outgoing transmissions. Deliberate physical speaker-to-microphone pickup is a simulated acoustic path, not a software feedback loop.

## RF processing

RF calculations produce link state. Audio code consumes signal level, interference, SNR/SINR, decode quality and related metrics rather than a simple in-range boolean.

## Extensibility

Hardware definitions should be data-driven where practical. Third-party equipment should use public definitions and APIs rather than patching private implementation details.

## Initial platform

- ArmA 3, Windows x64.
- TeamSpeak 3 as the initial voice backend.
- SQF/config for ArmA-side code.
- C++ for native components.
- CMake/MSVC for native builds.

Another voice backend may be added later without replacing the radio state model.

# 0006 - Direct voice acoustics

Status: Accepted

## Decision

Direct voice uses TeamSpeak's native 3D listener/source positioning and the custom client rolloff callback.

ArmA remains authoritative for player positions, head direction and selected voice level. The TeamSpeak plugin consumes the local spatial scene and renders the resulting acoustic state.

## Coordinate model

The listener is positioned at the TeamSpeak origin. Remote actors are expressed relative to the local ArmA actor.

This avoids carrying large terrain coordinates into the audio engine while preserving distance and direction.

The coordinate conversion is:

```text
TS X = ArmA X
TS Y = ArmA Z
TS Z = -ArmA Y
```

## Update model

ArmA spatial scenes arrive at approximately 10 Hz.

The acoustic worker runs at approximately 50 Hz. It uses actor velocity for at most 250 ms of prediction and smooths corrections between snapshots.

A scene that does not advance for 750 ms is stale and disables ICARUS acoustic control until fresh state returns.

## Attenuation

The initial vocal-effort profiles use reference distances that double between successive levels:

```text
whisper  0.75 m
quiet    1.5 m
normal   3 m
raised   6 m
shout    12 m
```

They are not maximum ranges.

The base gain is continuous and combines inverse-distance geometric spreading with a second far-field damping term. Occlusion, environment and hearing systems remain separate future layers.

## Real-time safety

The TeamSpeak rolloff callback does not acquire locks, allocate memory, access shared memory, log, or call TeamSpeak APIs.

A worker thread resolves ArmA identities and spatial state and publishes only a small atomic voice-level value per controlled TeamSpeak client. The audio callback reads that value and evaluates the shared native gain function.

Unmatched clients are left unmanaged so ICARUS does not alter unrelated TeamSpeak audio.

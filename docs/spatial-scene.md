# Spatial scene transport

The spatial scene is the local ArmA-to-voice-backend player snapshot consumed by direct-voice acoustics.

The TeamSpeak plugin uses this scene for listener orientation, remote speaker placement, velocity-based short-term prediction and per-actor voice level.

## Transport

```text
Local\ICARUS.SpatialScene.1
```

The mapping is tied to the ArmA-owned process-bridge generation and is versioned independently from the health and local-session mappings.

The initial scene capacity is 256 actors.

## Actor state

Each actor contains:

- Player UID.
- ArmA network ID.
- Eye position in world space.
- Eye/head direction.
- World velocity.
- Alive state.
- In-vehicle state.
- Direct-voice level.

The ArmA addon samples `allPlayers` at approximately 10 Hz and excludes headless clients.

The scene uses ArmA's existing replicated player objects. ICARUS does not broadcast player positions through TeamSpeak and does not add a second multiplayer position network.

## Voice level replication

The selected direct-voice level is stored on the local player object as `ICARUS_voiceLevel` and broadcast only when that value changes.

This allows other ArmA clients to include the selected level in their local spatial scene without sending continuous voice-level updates.

The value is clamped to the public direct-voice levels `1` through `5`.

## Snapshot acknowledgement

The voice backend publishes:

- Last observed scene sequence.
- Number of actors observed.
- Number of scene actors currently matched to known TeamSpeak identities.

This acknowledgement proves that the voice process is consuming the same scene before audio positioning depends on it.

## Diagnostics

With TeamSpeak and ArmA running:

```sqf
[] call ICARUS_fnc_spatialSceneStatus
```

A healthy single-player/local test resembles:

```text
transport=ready;protocol=1.0;generation=...;sceneValid=1;sceneSeq=...;actors=1;localFound=1;localPosition=[...];localVelocity=[...];localVoiceLevel=normal;ackValid=1;ackSceneSeq=...;observedActors=1;matchedActors=1
```

`ackSceneSeq` may trail the current `sceneSeq` by one snapshot because the ArmA scene and TeamSpeak acknowledgement both run at approximately 10 Hz.

## Runtime checks

Before merging:

1. Scene protocol reports `1.0`.
2. A local player appears as one observed actor.
3. `localFound=1` and local position changes when moving.
4. Local velocity changes while moving and returns near zero while stationary.
5. Changing direct-voice level changes `localVoiceLevel`.
6. TeamSpeak acknowledges the scene and reports one matched actor for the local client.
7. TeamSpeak restart resumes scene acknowledgement without restarting ArmA.
8. ArmA restart creates a new generation and the voice backend consumes the new scene.
9. Existing process-bridge, session-state and identity tests remain green.

Remote actor and multi-client identity matching require a later multiplayer validation with at least two human clients.

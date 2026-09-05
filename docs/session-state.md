# Session state

The session-state transport is the first data path layered on top of the ICARUS process bridge.

It does not contain radio state yet.

## ArmA to voice backend

The initial ArmA snapshot contains:

- ICARUS session ID.
- Player UID.
- ArmA network ID.
- Eye position in world space.
- Eye/head direction.
- Alive state.
- Spectator state.
- In-vehicle state.
- Vehicle network ID.
- Vehicle role.
- Current ICARUS voice level.

The snapshot sequence and native update timestamp are maintained by the shared-memory transport.

## Voice backend to ArmA

The initial voice-backend snapshot contains:

- Connection ID.
- Local client ID.
- Connection state.
- Plugin/backend health.
- Session-state protocol version.
- Acknowledged ArmA session ID.
- Acknowledged ArmA snapshot sequence.

For TeamSpeak 3, the generic connection ID currently contains the TeamSpeak server connection handler ID.

## Spectator integration

ICARUS detects the built-in End Game spectator state when its spectator script is active.

Mission frameworks may explicitly override this with:

```sqf
missionNamespace setVariable ["ICARUS_isSpectator", true];
```

and clear it with:

```sqf
missionNamespace setVariable ["ICARUS_isSpectator", false];
```

A later spectator integration layer will provide adapters for common mission frameworks.

## Voice level

Until the direct-voice control layer is implemented, ICARUS defaults to voice level `3`, which is `normal`.

The current value is stored in:

```sqf
missionNamespace getVariable ["ICARUS_voiceLevel", 3]
```

Protocol values are:

```text
0 unknown
1 whisper
2 quiet
3 normal
4 raised
5 shout
```

## Vehicle role

Protocol values are:

```text
0 none
1 driver
2 commander
3 gunner
4 turret
5 cargo
6 unknown
```

## Diagnostics

With ArmA and TeamSpeak running:

```sqf
[] call ICARUS_fnc_sessionStatus
```

A healthy result resembles:

```text
transport=ready;protocol=1.0;generation=...;armaValid=1;armaSeq=...;playerUid=...;networkId=...;position=[...];alive=1;spectator=0;inVehicle=0;voiceLevel=normal;vehicleRole=none;voiceValid=1;voiceSeq=...;voiceConnection=established;voiceConnectionId=...;voiceClientId=...;voiceHealth=ready;ackSession=...;ackArmaSeq=...
```

The acknowledgement session should match the current generation.

`ackArmaSeq` should advance as ArmA publishes new snapshots.

## Runtime checks

Test the following before merging this subsystem:

1. Player movement changes the reported position.
2. `armaSeq` advances while the player session is active.
3. TeamSpeak reports `established` and a non-zero local client ID while connected.
4. `ackSession` matches the current ArmA generation.
5. `ackArmaSeq` follows the ArmA sequence.
6. Entering and leaving a vehicle changes `inVehicle`, vehicle ID and role.
7. Closing TeamSpeak makes the voice-backend snapshot stop advancing.
8. Restarting TeamSpeak resumes acknowledgement without restarting ArmA.
9. Restarting ArmA creates a new session generation and TeamSpeak acknowledges it.

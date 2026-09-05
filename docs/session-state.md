# Session state

The session-state transport is the first data path layered on top of the ICARUS process bridge.

It does not contain radio state yet.

## ArmA to voice backend

The ArmA snapshot contains:

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

The voice-backend snapshot contains:

- Connection ID.
- Local client ID.
- Connection state.
- Plugin/backend health.
- Session-state protocol version.
- Acknowledged ArmA session ID.
- Acknowledged ArmA snapshot sequence.
- Compatible feature flags for speaking and identity state.

For TeamSpeak 3, the generic connection ID currently contains the TeamSpeak server connection handler ID.

## Spectator integration

Spectator state defaults to `false`.

Mission or spectator-framework integrations may set it explicitly:

```sqf
missionNamespace setVariable ["ICARUS_isSpectator", true];
```

and clear it with:

```sqf
missionNamespace setVariable ["ICARUS_isSpectator", false];
```

A later spectator integration layer will provide adapters for supported spectator frameworks.

## Voice level

Direct voice defaults to level `3`, `normal`.

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

The public direct-voice setter limits selectable values to `1` through `5`.

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

The session protocol is currently `1.1`.

The acknowledgement session should match the current generation.

`ackArmaSeq` should advance as ArmA publishes new snapshots.

Direct-voice-specific diagnostics are available through:

```sqf
[] call ICARUS_fnc_directVoiceStatus
```

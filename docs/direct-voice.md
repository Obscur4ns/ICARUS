# Direct voice state

This stage establishes direct-voice state and TeamSpeak identity mapping. It does not yet alter playback volume, 3D position, occlusion or radio audio.

## Voice levels

ArmA owns the selected direct-voice level.

```text
1 whisper
2 quiet
3 normal
4 raised
5 shout
```

`0` remains reserved as `unknown` in the native protocol and is not selected by the public SQF setter.

Read the current level:

```sqf
[] call ICARUS_fnc_getVoiceLevel
```

Set it:

```sqf
[1] call ICARUS_fnc_setVoiceLevel
```

Cycle it:

```sqf
[] call ICARUS_fnc_cycleVoiceLevel
```

No attenuation distances are assigned at this stage. Those belong to the later direct-voice acoustic model.

## Speaking state

TeamSpeak speaking state is captured from its talk-status callbacks and published back through the existing session-state snapshot.

The shared session protocol is now `1.1`. The layout is unchanged from `1.0`; the previously reserved final 32-bit word in the voice-backend snapshot now carries compatible feature flags.

Current flags are:

- Local client is actively talking.
- Local ArmA identity is ready for mapping.
- Local identity announcement has been submitted to TeamSpeak.

## Identity mapping

ICARUS never uses TeamSpeak nicknames as player identity.

The local ArmA snapshot supplies:

- Player UID.
- ArmA network ID.

TeamSpeak supplies the actual sender client ID and unique identity.

ICARUS plugin instances exchange a compact plugin command when an identity becomes available:

```text
ICARUS-ID    1    H    <player UID>    <network ID>
```

Fields are tab separated.

`H` is a hello. Existing ICARUS clients in the same TeamSpeak channel record the sender mapping and reply directly with an `I` identity message. Identity replies do not trigger another reply.

This gives a late-joining client mappings for existing ICARUS users without periodic broadcast traffic.

Mappings are scoped to the current TeamSpeak connection and client ID. They are removed on moves, kicks and connection changes.

A future ArmA multiplayer roster will decide which mapped TeamSpeak identities correspond to players in the current game session. TeamSpeak identity discovery does not become game authority.

## Diagnostics

With ArmA and TeamSpeak running:

```sqf
[] call ICARUS_fnc_directVoiceStatus
```

Expected idle result:

```text
state=ready;protocol=1.1;level=normal;talking=0;identityReady=1;identityAnnounced=1;clientId=...;connectionId=...
```

While TeamSpeak is transmitting local speech:

```text
talking=1
```

Changing the ArmA voice level should immediately change the `level` field.

## Runtime checks

Before merging:

1. Direct-voice status reports protocol `1.1`.
2. `identityReady=1` after both ArmA and TeamSpeak are connected.
3. `identityAnnounced=1` after plugin registration and identity publication.
4. `talking` changes to `1` while TeamSpeak is transmitting and returns to `0`.
5. Setting each ArmA voice level is reflected in the diagnostic output.
6. TeamSpeak restart restores identity readiness and announcement without restarting ArmA.
7. ArmA restart creates a new generation and the identity is announced again.
8. Existing process-bridge and session-state tests continue to pass.

Remote player-to-TeamSpeak mapping needs a later multiplayer/two-client validation because one TeamSpeak client can only prove the local side of the handshake.

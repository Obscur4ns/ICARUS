# Direct voice

Direct voice uses ArmA player state to place TeamSpeak speakers in 3D space and applies a continuous distance attenuation model based on the speaker's selected vocal effort.

This stage does not yet model occlusion, interiors, vehicle isolation, languages, hearing protection or radios.

## Voice levels

ArmA owns the selected direct-voice level. The owning client stores it on the player object for remote spatial-scene sampling and only broadcasts that object variable when the selected level changes.

```text
1 whisper
2 quiet
3 normal
4 raised
5 shout
```

The current acoustic profiles use these reference distances:

| Level | Reference distance |
| --- | ---: |
| Whisper | 0.75 m |
| Quiet | 1.5 m |
| Normal | 3 m |
| Raised | 6 m |
| Shout | 12 m |

These values are calibration parameters, not hard hearing ranges. The attenuation curve remains continuous beyond them and never applies an arbitrary range cutoff.

The first model combines geometric spreading with additional far-field damping:

```text
effectiveDistance = max(distance - 0.5 m, 0)
geometric = reference / (reference + effectiveDistance)
farField = 1 / (1 + effectiveDistance / (reference * 8))
gain = geometric * farField
```

The 0.5 m near field prevents a speaker directly beside the listener from being unnecessarily attenuated. Later environment, occlusion and hearing systems will modify the result rather than being baked into these base vocal-effort curves.

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

## 3D positioning

The TeamSpeak listener is kept at the local origin. Remote ArmA actors are converted to positions relative to the local player, which avoids precision loss from large terrain world coordinates.

ArmA axes are mapped into TeamSpeak coordinates as:

```text
TeamSpeak X = ArmA X
TeamSpeak Y = ArmA Z
TeamSpeak Z = -ArmA Y
```

The listener orientation follows the ArmA head-direction vector. The forward and up vectors are normalised and kept perpendicular as required by TeamSpeak's 3D API.

The spatial scene arrives at approximately 10 Hz. The acoustic renderer runs at approximately 50 Hz, extrapolates each actor for at most 250 ms using ArmA velocity, and smooths positional corrections between scene snapshots.

A spatial scene that stops advancing for 750 ms is stale. ICARUS then removes its custom rolloff state and recentres previously controlled TeamSpeak sources rather than leaving stale positional audio active.

## Rolloff callback

The TeamSpeak custom 3D rolloff callback is deliberately small. It performs no locks, allocation, logging, shared-memory reads or TeamSpeak API calls.

The acoustic worker publishes only the voice level required by the callback into fixed atomic client slots. The callback then evaluates the shared native acoustic curve for the distance TeamSpeak provides.

Clients without a valid ICARUS ArmA identity match are not given an ICARUS rolloff level, so their TeamSpeak volume is left untouched.

## Speaking and identity state

TeamSpeak speaking state is captured from talk-status callbacks and published through session state.

ICARUS never uses TeamSpeak nicknames as player identity. ArmA player UID and network ID are joined to TeamSpeak's callback-provided client ID and unique identity using the versioned plugin-command handshake.

Session-state protocol `1.2` adds compatible flags for:

- Direct-voice acoustics active.
- Spatial scene fresh.

The binary session-state layout is unchanged from `1.1`.

## Diagnostics

With ArmA and TeamSpeak running:

```sqf
[] call ICARUS_fnc_directVoiceStatus
```

A healthy local result resembles:

```text
state=ready;protocol=1.2;level=normal;talking=0;identityReady=1;identityAnnounced=1;acousticsActive=1;sceneFresh=1;clientId=...;connectionId=...
```

A single local client can validate listener setup, scene freshness and the native attenuation model. Actual remote 3D placement, stereo direction and perceived distance attenuation require at least two human TeamSpeak/ArmA clients.

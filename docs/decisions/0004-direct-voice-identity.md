# 0004 - Direct voice identity signalling

Status: Accepted

## Decision

The first TeamSpeak identity exchange uses TeamSpeak plugin commands, not nicknames and not the `CLIENT_META_DATA` field.

ArmA remains the source of player UID and ArmA network identity.

TeamSpeak remains the source of TeamSpeak client ID and unique client identity.

The plugin joins those identities locally.

## Protocol

Identity command protocol major version `1` uses tab-separated messages:

```text
ICARUS-ID    1    H    <player UID>    <network ID>
ICARUS-ID    1    I    <player UID>    <network ID>
```

`H` is a hello and requests an identity response.

`I` is an identity response and never causes another response.

Malformed messages, unsupported protocol versions and fields containing command separators are rejected.

## Discovery

When local ArmA identity becomes available, the TeamSpeak plugin sends one hello to the current channel.

A client receiving a hello:

1. Uses TeamSpeak's callback-provided sender client ID and unique identity.
2. Records the ArmA UID and network ID for that sender.
3. Sends its own identity directly back to the sender.

This avoids nickname matching and avoids periodic identity broadcasts.

A move to a different TeamSpeak channel causes the local identity to be announced again.

## Scope

This mechanism discovers voice-backend identities only.

It does not prove that a TeamSpeak client belongs to the current ArmA multiplayer session. That decision will be made by matching discovered identities against the authoritative ArmA player roster when that roster transport is added.

## Session-state compatibility

Session-state protocol `1.1` keeps the `1.0` binary layout.

The final reserved 32-bit value in `VoiceBackendSessionState` becomes a feature-flag word carrying direct-voice speaking and identity status.

This is a compatible minor-version addition and does not require a new shared-memory mapping name.

# 0003 - Session state transport

Status: Accepted

## Decision

Session and gameplay snapshots use a separate named shared-memory mapping from the process health bridge.

```text
Local\ICARUS.Bridge.1
Local\ICARUS.SessionState.1
```

The bridge mapping remains responsible for process discovery, protocol health, session generation and heartbeat.

The session-state mapping is responsible for current-state snapshots moving between ArmA and the active voice backend.

## Reasons

Process health and gameplay state change at different rates and have different compatibility requirements.

Keeping them separate means adding radio, player or audio state does not require changing the proven heartbeat page or its lifecycle code.

The session-state mapping can also be versioned independently of the process bridge.

## Ownership

ArmA owns:

- Mapping creation.
- Session generation.
- Local player identity.
- Position and head direction.
- Life and spectator state.
- Vehicle state.
- Voice-level selection.
- Future authoritative local radio state.

The voice backend owns:

- Backend connection state.
- Backend-local client identity.
- Backend health.
- Acknowledgement of the current ArmA session and snapshot sequence.

TeamSpeak 3 is the first voice backend, but the shared protocol uses backend-neutral state names where the semantics are not TeamSpeak-specific.

## Snapshot consistency

Each direction has one writer.

Snapshots use an odd/even sequence counter:

1. Writer moves the sequence to an odd value.
2. Writer updates the payload.
3. Writer moves the sequence to an even value.
4. Reader accepts a snapshot only when the same even sequence is observed before and after the copy.

This prevents consumers from accepting a partially-written snapshot without placing blocking locks in a future audio path.

## Protocol

The initial session-state protocol is `1.0`.

The mapping name contains the protocol major version.

Breaking layout changes require a new major version and mapping name.

## Update rates

ArmA publishes the initial local session snapshot at approximately 20 Hz.

The TeamSpeak backend publishes backend state at approximately 10 Hz.

These rates are intentionally independent of render frame rate and TeamSpeak audio callbacks.

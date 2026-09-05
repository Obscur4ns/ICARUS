# 0002 - Process bridge transport

Status: Accepted

## Decision

The first ArmA-extension to TeamSpeak-plugin process bridge uses a named shared-memory page in the local Windows session.

The mapping name includes the bridge protocol major version:

```text
Local\ICARUS.Bridge.1
```

The bridge currently carries only:

- Protocol identity.
- Session generation.
- ArmA process identity.
- TeamSpeak process identity.
- Endpoint lifecycle state.
- Heartbeats.
- Health state.

Voice does not cross this bridge.

## Reasons

The first bridge needs a low-overhead way for two local processes to expose small pieces of current state without making either side wait on the other.

Shared memory gives the bridge a simple snapshot model and keeps blocking IPC away from TeamSpeak audio callbacks.

The transport is isolated behind `icarus::ipc`. A later high-volume or message-oriented path may use a different mechanism without changing radio or audio ownership.

## Protocol

The initial bridge protocol is `1.0`.

Breaking changes require a new protocol major version and a new mapping name.

Compatible additions may increment the minor version while preserving the existing shared-memory prefix.

## Lifecycle

The ArmA extension owns the bridge generation.

When a new ArmA process claims the mapping, it creates a new generation and resets endpoint state.

The TeamSpeak plugin may start before ArmA. It waits for the mapping and registers when a valid generation appears.

Both endpoints update a heartbeat every 250 ms.

A peer is considered stale after 1500 ms without a heartbeat.

The bridge must recover when TeamSpeak or ArmA is restarted without requiring the other process to restart.

# 0005 - Spatial scene transport

Status: Accepted

## Decision

Remote-player spatial state uses a dedicated local shared-memory mapping:

```text
Local\ICARUS.SpatialScene.1
```

The mapping carries a bounded snapshot of the local ArmA client's current player scene from the ArmA extension to the voice backend.

## Reasons

The existing session-state mapping intentionally describes one local player and the voice backend. Expanding it into a variable multiplayer roster would mix two different update rates and responsibilities.

The spatial scene changes independently, is substantially larger and will later feed positional audio calculations. Giving it a separate protocol keeps process health and local session state stable.

Player positions are not exchanged with other clients through TeamSpeak. Each ArmA client samples the remote player objects it already receives from normal game replication.

## Capacity

The initial scene supports 256 actors.

The project performance target is 120 players. A fixed 256-actor bound leaves headroom while keeping shared memory predictable and preventing unbounded input from crossing into the voice process.

## Update rate

ArmA publishes the scene at approximately 10 Hz.

Position, head direction and velocity are transported together. The future acoustic renderer may interpolate or extrapolate between snapshots without requiring ArmA to publish at audio-callback frequency.

## Consistency

The mapping uses the same odd/even single-writer snapshot sequence scheme as session state.

The TeamSpeak side acknowledges the exact scene sequence it consumed and reports observed and identity-matched actor counts.

## Voice level

Direct-voice level remains ArmA state. The owning client broadcasts its selected value on the player object only when the level changes. Other clients sample that replicated value into their local spatial scene.

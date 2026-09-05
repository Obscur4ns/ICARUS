# ICARUS

**Intelligent Communications And Radio User System**

ICARUS is a communications framework for ArmA 3 with TeamSpeak integration. It is intended to provide direct voice, radio, intercom, signal propagation, waveform, COMSEC, programming and audio-device simulation behind one consistent API.

The project is pre-alpha. There is no working release yet.

## Design goals

- Physical radios with persistent identity and state.
- Any number of independent transceivers per device.
- Waveforms with meaningful operating characteristics and trade-offs.
- COMSEC and TRANSEC as first-class systems.
- Radio programming through front panels, KDUs, fill devices, laptops, tablets, vehicle systems and mission tooling.
- RF behaviour based on signal conditions rather than fixed range checks.
- Direct speech, hearing protection, microphones, intercoms and radio audio handled as parts of one communications system.
- Data-driven hardware definitions and a public API for third-party radios and equipment.
- No hard dependency on ACE.
- TeamSpeak 3 as the first voice backend, without making the radio model dependent on TeamSpeak-specific state.
- Server-authoritative multiplayer state with JIP, reconnect and equipment-transfer behaviour treated as normal cases rather than exceptions.

The intended feature set is recorded in [docs/features.md](docs/features.md). The system boundaries are recorded in [docs/architecture.md](docs/architecture.md).

## Development

`main` is the stable baseline. `development` is the integration branch. Normal work is done on short-lived branches from `development`.

ArmA addon work uses HEMTT. Native code uses CMake and MSVC.

See [docs/building.md](docs/building.md) and [docs/development.md](docs/development.md).

## Status

Project foundation.

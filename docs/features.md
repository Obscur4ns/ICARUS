# Feature contract

This document records the intended ICARUS feature set. It exists to prevent major systems being added late without considering their architectural cost.

Not every item is a first-release requirement. Release gates will be assigned separately. The architecture must not make any item listed here impractical to add later.

## Direct voice

- True positional direct speech.
- Whisper, quiet, normal, raised and shout voice levels.
- Smooth distance attenuation.
- Speaker and listener head orientation.
- World-object and building occlusion.
- Indoor/outdoor transitions.
- Vehicle interior/exterior attenuation with seat and hatch state support.
- Multiple nearby speakers mixed normally.
- Physical speech remains audible locally while a player transmits on radio.
- Optional language simulation with players able to know and speak multiple mission-defined languages.
- Spectator, Zeus and remote-control handling.
- AI hearing integration where practical.

## Physical radios

- Every radio is a distinct physical device with a persistent unique identity.
- State follows the radio when it is dropped, transferred, looted or stored.
- Radios retain presets, waveform settings, keys, programming and power state unless explicitly cleared.
- Radios are not divided into framework-level short-range and long-range categories.
- Hardware capabilities are data-driven.

## Multi-transceiver operation

- Any number of radio transceivers or receive-only services per physical device.
- Independent frequency, waveform, COMSEC, TRANSEC, power, volume and audio routing per service.
- Simultaneous monitoring of multiple services.
- Dedicated PTT bindings per service.
- Selected-service PTT.
- Guard receivers, scan receivers and priority receivers.
- Half-duplex and full-duplex behaviour where supported by hardware and waveform.
- Cross-band operation.

## Channels and presets

A channel is a complete operating configuration, not just a frequency.

A channel may define:

- Receive and transmit frequency.
- Simplex or split operation.
- Waveform.
- Modulation.
- Channel bandwidth.
- Network identity.
- COMSEC assignment.
- TRANSEC assignment.
- Transmit power.
- Squelch.
- Scan and guard behaviour.
- Display name and other hardware-specific parameters.

## Waveforms

Waveforms are first-class definitions with actual operating differences.

A waveform may define:

- Supported frequency ranges.
- Modulation and bandwidth.
- Voice and data capability.
- Acquisition and synchronisation requirements.
- Latency.
- Error correction behaviour.
- Weak-signal behaviour.
- Interference tolerance.
- Multipath tolerance.
- Jamming resistance.
- Network capacity.
- Duplex capability.
- COMSEC and TRANSEC support.

Analog and digital links must degrade differently. Digital operation must not be represented only by a different sound effect.

## RF propagation

Signal calculations must use link conditions rather than a fixed maximum range.

Inputs may include:

- Frequency.
- Transmit power.
- Antenna gain, pattern, orientation and polarisation.
- Feedline loss.
- Terrain and elevation.
- Line of sight and diffraction.
- Multipath.
- Building and urban attenuation.
- Vehicle attenuation.
- Receiver sensitivity, selectivity and noise floor.

The RF layer should expose useful link metrics such as received signal level, SNR/SINR and link quality for other systems to consume.

## Interference

- Co-channel interference.
- Adjacent-channel interference where appropriate.
- Capture effect.
- Near/far behaviour.
- Receiver overload.
- Multiple overlapping transmissions.
- Waveform-specific interference tolerance.
- Jammers represented as RF emitters rather than scripted range debuffs.

## COMSEC

- Multiple stored keys.
- Key identifiers and metadata.
- Clear and secure operation.
- Key assignment per channel or transceiver.
- Key loading, deletion and zeroise.
- Captured equipment retains loaded keys unless cleared.
- Missing or incompatible key material prevents intelligible decode.
- Validity periods and rollover support.
- External key-loading and programming interfaces.
- Server-authoritative key state.

ICARUS will simulate operational key possession and compatibility. It will not attempt to reproduce restricted real-world cryptographic implementations.

## TRANSEC

- Network or hopset identity.
- Time synchronisation.
- Acquisition, loss of sync and re-entry.
- Fixed-frequency fallback where supported.
- Frequency-hopping and other transmission-security behaviours at a gameplay/simulation level.
- Waveform-dependent resistance to interception and jamming.

ICARUS will simulate operational effects rather than reproduce restricted real-world hopping algorithms.

## Programming

All programming methods write to the same underlying radio configuration model.

Supported programming clients may include:

- Radio front panel.
- KDU.
- Fill device.
- Laptop.
- Tablet.
- Vehicle terminal.
- Arsenal tooling.
- Eden modules.
- Zeus modules.
- Mission scripts.
- Third-party addons.

Programming packages may contain channels, mission plans, network plans, waveform parameters, COMSEC assignments, TRANSEC assignments, scan lists, guard settings and device-specific options.

Programming devices may themselves be persistent physical equipment with stored plans or keys.

## PTT and audio routing

- Primary and secondary radio PTT.
- Arbitrary additional PTT bindings.
- Per-radio and per-transceiver PTT.
- Intercom PTT.
- Priority or emergency PTT where hardware supports it.
- Press-and-hold transmission with stale-TX protection.
- Independent left, right and both-ear routing.
- Headset, handset, speaker, intercom and other audio destinations.
- Simultaneous reception from multiple sources.

## Radio audio

Radio audio processing may depend on:

- Waveform.
- Modulation.
- Bandwidth.
- Signal strength.
- SNR/SINR.
- Interference.
- Receiver hardware.
- Microphone.
- Headset, handset or speaker.

Supported effects include band limiting, compression, clipping, noise, analog degradation, digital breakup, squelch behaviour, key-up/key-down sounds and sidetone.

## Hearing protection and audio devices

Hearing protection is a proper audio subsystem rather than a flat volume reduction.

Equipment may define:

- Passive attenuation by frequency.
- Active hearing protection.
- Quiet-sound amplification.
- Loud-sound compression.
- Impulse protection.
- Sustained-noise protection.
- Independent left/right behaviour.
- Seal quality.
- Radio inputs.
- External microphones.
- Power requirements.
- Headset and helmet compatibility.

Temporary hearing effects such as ringing or short-term muffling may be simulated. Permanent hearing damage should be optional.

ACE hearing compatibility is desirable but ICARUS must not require ACE.

## Microphones and environmental pickup

Radio transmissions include microphone pickup rather than voice alone.

Microphones may define:

- Pickup pattern.
- Voice gain.
- Environmental gain.
- Noise rejection.
- Compression and clipping.
- Wind rejection.
- Position or fit quality.

Nearby environmental sound such as gunfire, engines, aircraft and explosions should be capable of bleeding into a live transmission.

Received ICARUS/TeamSpeak audio must not accidentally feed back into outgoing transmissions. Deliberate speaker-to-microphone pickup may be simulated when the physical conditions make it appropriate.

## Sidetone

Compatible equipment may feed a controlled amount of the user's microphone signal back into their headset during transmission.

Sidetone may expose poor microphone position, clipping or excessive environmental noise to the operator.

## Antennas

- Replaceable antennas.
- Frequency suitability.
- Gain.
- Radiation pattern.
- Polarisation.
- Physical orientation.
- Multiple antenna ports.
- Vehicle antennas.
- Remote antennas.
- Masts and ground spikes.
- Directional antennas.
- Feedline and cable loss.
- Damaged or missing antenna behaviour.
- Third-party antenna definitions.

## Speakers and shared equipment

- Internal radio speakers.
- External speakers.
- Handsets and remote speaker microphones.
- Dropped radios continue receiving when powered.
- Speaker audio is spatial in the game world.
- Nearby players may operate configured external radios.
- Shared vehicle radios and mounted equipment.

## Vehicles and intercom

- Any number of vehicle intercom networks.
- Crew, passenger, command and mission-defined circuits.
- Hot mic, voice activation, PTT and listen-only modes.
- Seat-specific access.
- Shared vehicle radios.
- Rack-mounted radios.
- Vehicle power and antennas.
- Independent headset routing.
- External infantry telephone/intercom support.
- Dismount connections and external PA systems where configured.

## Repeaters and gateways

- Same-band repeaters.
- Cross-band repeaters.
- Vehicle retransmission.
- Manpack and fixed retransmission.
- Airborne relays.
- SATCOM-style gateways.
- Waveform conversion where explicitly supported.
- Independent link calculation for each hop.
- Processing delay and compatibility checks.

## Power and batteries

- Internal and replaceable batteries.
- Battery capacity and state of charge.
- Different receive and transmit power consumption.
- Transmit-power-dependent drain.
- External battery packs.
- Vehicle and external DC power.
- Persistent power state.
- Optional simplified or disabled battery simulation.

## Damage

Framework support for:

- Damaged radio.
- Damaged transmitter or receiver.
- Damaged antenna.
- Damaged headset, handset or microphone.
- Reduced transmitter power.
- Reduced receiver sensitivity.
- Battery damage.

Damage simulation must be configurable.

## Electronic warfare

Architectural support is required for later implementation of:

- Spectrum monitoring.
- Transmission detection.
- Signal classification.
- Direction finding.
- Bearing and signal-strength estimation.
- Triangulation.
- Spot jamming.
- Barrage jamming.
- Directional jamming.
- Waveform-dependent resistance.
- Encrypted traffic detection.
- Network fingerprinting.

## Data services

The radio model must be able to distinguish voice, data and combined services.

Future data functions may include:

- Text messages.
- Position reports.
- Telemetry.
- Situational-awareness traffic.
- Remote programming.
- Network status and device control.

ICARUS provides communications transport and simulation; it is not intended to become a full mission-command or navigation application.

## TeamSpeak integration

- Automatic ArmA-to-TeamSpeak identity association.
- Optional channel management.
- Version mismatch detection.
- Reconnect and plugin-reload recovery.
- Safe restoration of normal TeamSpeak behaviour outside ArmA.
- Multiple ArmA servers using one TeamSpeak server.
- TeamSpeak 3 is the initial backend.
- The radio state model must not depend on TeamSpeak-specific concepts so another voice backend can be added later.

## Multiplayer and reliability

- Dedicated server support.
- JIP.
- Respawn.
- Team switch.
- Disconnect/reconnect.
- Radio transfer and dropping.
- Arsenal and scripted loadouts.
- Vehicle entry, exit and seat changes.
- Zeus possession and remote-controlled units.
- Stale transmission recovery.
- Server-authoritative radio identity.
- State validation and diagnostics.

## Performance

The design target is large organised multiplayer sessions, not small local tests.

Requirements include:

- Native handling for expensive RF or audio work.
- No unnecessary per-frame SQF work.
- Spatial culling and caching.
- Adaptive update rates.
- Priority processing for active transmitters.
- No blocking work in real-time audio callbacks.
- Profiling and diagnostics in development builds.

A reliable 120-player session is the initial scale target.

## User interface

ICARUS should support both fast operation and detailed equipment simulation.

- Keybind-driven quick controls.
- Compact generic radio controls.
- Full device-specific interfaces.
- Physical knobs, buttons, displays and menu logic where an addon implements them.
- Detailed simulation must not make routine net changes unnecessarily slow.

## Difficulty and accessibility

Server presets may include simplified, standard, simulation and custom configurations.

Individual systems such as RF complexity, interference, batteries, damage, antenna orientation, COMSEC and programming complexity should be independently configurable where practical.

## Modding API

Third-party addons must be able to add or extend:

- Radios.
- Transceivers.
- Waveforms.
- Antennas.
- Batteries.
- Microphones.
- Headsets and hearing protection.
- Programming devices.
- Intercoms.
- Repeaters.
- Jammers.
- Audio profiles.

Public events and APIs should expose meaningful state changes without requiring third-party code to replace ICARUS internals.

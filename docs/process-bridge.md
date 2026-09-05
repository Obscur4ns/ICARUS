# Process bridge

The process bridge proves local communication between the ArmA extension and TeamSpeak plugin before radio state or voice processing is added.

## Development deployment

Close TeamSpeak before deploying the plugin:

```powershell
.\tools\deploy-dev.ps1
```

Start TeamSpeak and ArmA in either order.

## ArmA status

In the ArmA debug console:

```sqf
[] call ICARUS_fnc_bridgeStatus
```

A healthy bridge returns a line similar to:

```text
state=connected;protocol=1.0;generation=123456;localPid=1000;peerPid=2000;peerHeartbeatAgeMs=42
```

The process IDs and generation will vary.

If TeamSpeak is not running or the plugin has not connected:

```text
state=waiting_for_peer
```

## TeamSpeak log

Plugin startup should include:

```text
ICARUS plugin loaded.
ICARUS process bridge service started.
```

## Restart test

With both applications running, confirm the bridge reports `connected`.

Close TeamSpeak and wait at least two seconds.

The ArmA status should move to:

```text
state=waiting_for_peer
```

Restart TeamSpeak.

The bridge should return to:

```text
state=connected
```

without restarting ArmA.

Repeat the test in the other direction by fully closing ArmA while leaving TeamSpeak running, then starting ArmA again.

## Native test

The native test suite contains a bridge lifecycle test that verifies:

- Initial connection.
- Peer loss detection.
- TeamSpeak-side restart.
- Reconnection.

Run it with the normal native test preset:

```powershell
ctest --preset windows-x64-ts3-debug
```

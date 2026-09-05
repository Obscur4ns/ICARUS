if (!hasInterface || {isNull player}) exitWith {false};

private _position = eyePos player;
private _headDirection = eyeDirection player;

private _vehicle = vehicle player;
private _inVehicle = _vehicle isNotEqualTo player;
private _vehicleNetworkId = if (_inVehicle) then {netId _vehicle} else {""};

private _vehicleRole = 0;

if (_inVehicle) then {
    private _assignedRole = assignedVehicleRole player;

    if (_assignedRole isNotEqualTo []) then {
        _vehicleRole = switch (toLower (_assignedRole # 0)) do {
            case "driver": {1};
            case "commander": {2};
            case "gunner": {3};
            case "turret": {4};
            case "cargo": {5};
            default {6};
        };
    };
};

private _spectator =
    missionNamespace getVariable ["ICARUS_isSpectator", false];

private _voiceLevel =
    floor ((missionNamespace getVariable ["ICARUS_voiceLevel", 3]) max 0 min 5);

private _extensionResult = "icarus" callExtension [
    "session_publish",
    [
        getPlayerUID player,
        netId player,
        _position # 0,
        _position # 1,
        _position # 2,
        _headDirection # 0,
        _headDirection # 1,
        _headDirection # 2,
        parseNumber (alive player),
        parseNumber _spectator,
        parseNumber _inVehicle,
        _vehicleNetworkId,
        _vehicleRole,
        _voiceLevel
    ]
];

_extensionResult params ["_result", "_returnCode", "_errorCode"];

(_returnCode isEqualTo 0) &&
(_errorCode isEqualTo 0) &&
(_result isEqualTo "ok")

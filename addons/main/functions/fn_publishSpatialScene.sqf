if (!hasInterface || {isNull player}) exitWith {false};

private _separator = toString [9];
private _encodedActors = [];

{
    if (!(_x isKindOf "HeadlessClient_F")) then {
        private _playerUid = getPlayerUID _x;
        private _networkId = netId _x;

        if (_playerUid isNotEqualTo "" && _networkId isNotEqualTo "") then {
            private _position = eyePos _x;
            private _headDirection = eyeDirection _x;
            private _velocity = velocity _x;
            private _inVehicle = vehicle _x isNotEqualTo _x;
            private _flags = parseNumber (alive _x);

            if (_inVehicle) then {
                _flags = _flags + 2;
            };

            private _voiceLevel =
                floor (((_x getVariable ["ICARUS_voiceLevel", 3]) max 1) min 5);

            private _fields = [_playerUid, _networkId]
                + (_position apply {str _x})
                + (_headDirection apply {str _x})
                + (_velocity apply {str _x})
                + [str _flags, str _voiceLevel];

            _encodedActors pushBack (_fields joinString _separator);
        };
    };
} forEach allPlayers;

if ((count _encodedActors) > 256) then {
    _encodedActors resize 256;
};

private _extensionResult = "icarus" callExtension [
    "spatial_scene_publish",
    _encodedActors
];

_extensionResult params ["_result", "_returnCode", "_errorCode"];

(_returnCode isEqualTo 0) &&
(_errorCode isEqualTo 0) &&
(_result isEqualTo "ok")

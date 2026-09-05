if (!hasInterface) exitWith {};

private _result = "icarus" callExtension "bridge_start";
diag_log format ["[ICARUS] Process bridge: %1", _result];

if (missionNamespace getVariable ["ICARUS_sessionPublisherRunning", false]) exitWith {};

missionNamespace setVariable ["ICARUS_sessionPublisherRunning", true];

[] spawn {
    waitUntil {
        uiSleep 0.1;
        !isNull player
    };

    [missionNamespace getVariable ["ICARUS_voiceLevel", 3]] call ICARUS_fnc_setVoiceLevel;

    private _nextSpatialUpdate = 0;

    while {hasInterface} do {
        [] call ICARUS_fnc_publishSessionState;

        if (diag_tickTime >= _nextSpatialUpdate) then {
            [] call ICARUS_fnc_publishSpatialScene;
            _nextSpatialUpdate = diag_tickTime + 0.1;
        };

        uiSleep 0.05;
    };
};

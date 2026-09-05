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

    while {hasInterface} do {
        [] call ICARUS_fnc_publishSessionState;
        uiSleep 0.05;
    };
};

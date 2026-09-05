if (!hasInterface) exitWith {};

private _result = "icarus" callExtension "bridge_start";
diag_log format ["[ICARUS] Process bridge: %1", _result];

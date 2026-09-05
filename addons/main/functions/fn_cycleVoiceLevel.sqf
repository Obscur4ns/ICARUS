private _level =
    floor (((missionNamespace getVariable ["ICARUS_voiceLevel", 3]) max 1) min 5);

_level = (_level mod 5) + 1;
missionNamespace setVariable ["ICARUS_voiceLevel", _level];
_level

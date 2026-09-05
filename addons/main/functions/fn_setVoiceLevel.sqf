params [["_level", 3, [0]]];

_level = floor ((_level max 1) min 5);
missionNamespace setVariable ["ICARUS_voiceLevel", _level];
_level

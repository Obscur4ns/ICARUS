private _level =
    floor (((missionNamespace getVariable ["ICARUS_voiceLevel", 3]) max 1) min 5);

[((_level mod 5) + 1)] call ICARUS_fnc_setVoiceLevel

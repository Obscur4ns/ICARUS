class CfgPatches
{
    class icarus_main
    {
        name = "ICARUS - Main";
        author = "ICARUS Project";
        requiredVersion = 2.20;
        requiredAddons[] = {"A3_Data_F"};
        units[] = {};
        weapons[] = {};
    };
};

class CfgFunctions
{
    class ICARUS
    {
        class Main
        {
            file = "\z\icarus\addons\main\functions";

            class extensionPing {};
            class extensionVersion {};
            class bridgeStatus {};
            class publishSessionState {};
            class sessionStatus {};
            class directVoiceStatus {};
            class getVoiceLevel {};
            class setVoiceLevel {};
            class cycleVoiceLevel {};
            class publishSpatialScene {};
            class spatialSceneStatus {};

            class initBridge
            {
                postInit = 1;
            };
        };
    };
};

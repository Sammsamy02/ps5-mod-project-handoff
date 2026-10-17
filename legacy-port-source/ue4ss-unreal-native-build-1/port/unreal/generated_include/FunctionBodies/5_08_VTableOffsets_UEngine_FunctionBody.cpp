if (auto it = UEngine::VTableLayoutMap.find(STR("__vecDelDtor")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("WorldAdded")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("WorldAdded"), 0x2A0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("WorldAdded__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("WorldAdded__Ptr_UWorld"), 0x2A0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("WorldDestroyed")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("WorldDestroyed"), 0x2A8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("WorldDestroyed__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("WorldDestroyed__Ptr_UWorld"), 0x2A8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsInitialized")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsInitialized"), 0x2B0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsInitialized_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsInitialized_C"), 0x2B0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetDefaultWorldFeatureLevel")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetDefaultWorldFeatureLevel"), 0x2B8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetDefaultWorldFeatureLevel_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetDefaultWorldFeatureLevel_C"), 0x2B8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Init")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Init"), 0x2C0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Init__Ptr_IEngineLoop")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Init__Ptr_IEngineLoop"), 0x2C0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Start")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Start"), 0x2C8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("PreExit")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("PreExit"), 0x2D0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ReleaseAudioDeviceManager")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ReleaseAudioDeviceManager"), 0x2D8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Tick")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Tick"), 0x2E0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Tick__float__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Tick__float__bool"), 0x2E0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("UpdateTimeAndHandleMaxTickRate")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("UpdateTimeAndHandleMaxTickRate"), 0x2E8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CorrectNegativeTimeDelta")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CorrectNegativeTimeDelta"), 0x2F0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CorrectNegativeTimeDelta__double")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CorrectNegativeTimeDelta__double"), 0x2F0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetMaxTickRate")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetMaxTickRate"), 0x2F8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetMaxTickRate_C__float__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetMaxTickRate_C__float__bool"), 0x2F8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetMaxFPS")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetMaxFPS"), 0x300);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetMaxFPS_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetMaxFPS_C"), 0x300);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("SetMaxFPS")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("SetMaxFPS"), 0x308);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("SetMaxFPS__C_float")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("SetMaxFPS__C_float"), 0x308);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("UpdateRunningAverageDeltaTime")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("UpdateRunningAverageDeltaTime"), 0x310);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("UpdateRunningAverageDeltaTime__float__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("UpdateRunningAverageDeltaTime__float__bool"), 0x310);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsAllowedFramerateSmoothing")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsAllowedFramerateSmoothing"), 0x318);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsAllowedFramerateSmoothing_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsAllowedFramerateSmoothing_C"), 0x318);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsRenderingSuspended")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsRenderingSuspended"), 0x320);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsRenderingSuspended_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsRenderingSuspended_C"), 0x320);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("OnLostFocusPause")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("OnLostFocusPause"), 0x328);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("OnLostFocusPause__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("OnLostFocusPause__bool"), 0x328);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldThrottleCPUUsage")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldThrottleCPUUsage"), 0x330);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldThrottleCPUUsage_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldThrottleCPUUsage_C"), 0x330);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsControllerIdUsingPlatformUserId")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsControllerIdUsingPlatformUserId"), 0x338);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsControllerIdUsingPlatformUserId_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsControllerIdUsingPlatformUserId_C"), 0x338);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldDrawBrushWireframe")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldDrawBrushWireframe"), 0x340);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldDrawBrushWireframe__Ptr_AActor")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldDrawBrushWireframe__Ptr_AActor"), 0x340);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetMapBuildCancelled")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetMapBuildCancelled"), 0x348);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetMapBuildCancelled_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetMapBuildCancelled_C"), 0x348);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("SetMapBuildCancelled")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("SetMapBuildCancelled"), 0x350);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("SetMapBuildCancelled__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("SetMapBuildCancelled__bool"), 0x350);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("AllowSelectTranslucent")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("AllowSelectTranslucent"), 0x358);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("AllowSelectTranslucent_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("AllowSelectTranslucent_C"), 0x358);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("OnlyLoadEditorVisibleLevelsInPIE")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("OnlyLoadEditorVisibleLevelsInPIE"), 0x360);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("OnlyLoadEditorVisibleLevelsInPIE_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("OnlyLoadEditorVisibleLevelsInPIE_C"), 0x360);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetUnifiedTimeBudgetForStreaming")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetUnifiedTimeBudgetForStreaming"), 0x368);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetUnifiedTimeBudgetForStreaming__float__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetUnifiedTimeBudgetForStreaming__float__bool"), 0x368);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleUnifiedStreaming")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleUnifiedStreaming"), 0x370);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleUnifiedStreaming__float")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleUnifiedStreaming__float"), 0x370);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("PreferToStreamLevelsInPIE")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("PreferToStreamLevelsInPIE"), 0x378);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("PreferToStreamLevelsInPIE_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("PreferToStreamLevelsInPIE_C"), 0x378);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetSpriteCategoryIndex")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetSpriteCategoryIndex"), 0x380);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetSpriteCategoryIndex__Ref_C_FName")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetSpriteCategoryIndex__Ref_C_FName"), 0x380);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetTimeBetweenGarbageCollectionPasses_C__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetTimeBetweenGarbageCollectionPasses_C__bool"), 0x388);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetTimeBetweenGarbageCollectionPasses")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetTimeBetweenGarbageCollectionPasses"), 0x388);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("StartFPSChart")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("StartFPSChart"), 0x390);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("StartFPSChart__Ref_C_FString__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("StartFPSChart__Ref_C_FString__bool"), 0x390);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("StopFPSChart")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("StopFPSChart"), 0x398);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("StopFPSChart__Ref_C_FString")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("StopFPSChart__Ref_C_FString"), 0x398);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ProcessToggleFreezeCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ProcessToggleFreezeCommand"), 0x3A0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ProcessToggleFreezeCommand__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ProcessToggleFreezeCommand__Ptr_UWorld"), 0x3A0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ProcessToggleFreezeStreamingCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ProcessToggleFreezeStreamingCommand"), 0x3A8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ProcessToggleFreezeStreamingCommand__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ProcessToggleFreezeStreamingCommand__Ptr_UWorld"), 0x3A8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldForceGarbageCollection")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldForceGarbageCollection"), 0x3B0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetIncrementalGCTimePerFrame")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetIncrementalGCTimePerFrame"), 0x3B8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HasMultipleLocalPlayers")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HasMultipleLocalPlayers"), 0x3C0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HasMultipleLocalPlayers__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HasMultipleLocalPlayers__Ptr_UWorld"), 0x3C0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsSettingUpPlayWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsSettingUpPlayWorld"), 0x3C8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsSettingUpPlayWorld_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsSettingUpPlayWorld_C"), 0x3C8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetGameViewportWidget")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetGameViewportWidget"), 0x3D0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetGameViewportWidget_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetGameViewportWidget_C"), 0x3D0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("FocusNextPIEWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("FocusNextPIEWorld"), 0x3D8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("FocusNextPIEWorld__Ptr_UWorld__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("FocusNextPIEWorld__Ptr_UWorld__bool"), 0x3D8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ResetPIEAudioSetting")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ResetPIEAudioSetting"), 0x3E0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ResetPIEAudioSetting__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ResetPIEAudioSetting__Ptr_UWorld"), 0x3E0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetNextPIEViewport")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetNextPIEViewport"), 0x3E8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("GetNextPIEViewport__Ptr_UGameViewportClient")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("GetNextPIEViewport__Ptr_UGameViewportClient"), 0x3E8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("RemapGamepadControllerIdForPIE")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("RemapGamepadControllerIdForPIE"), 0x3F0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("RemapGamepadControllerIdForPIE__Ptr_UGameViewportClient__Ref_int32")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("RemapGamepadControllerIdForPIE__Ptr_UGameViewportClient__Ref_int32"), 0x3F0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("NotifyToolsOfObjectReplacement")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("NotifyToolsOfObjectReplacement"), 0x3F8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("NotifyToolsOfObjectReplacement__Ref_C_TMap_UObject_*_UObject_*_")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("NotifyToolsOfObjectReplacement__Ref_C_TMap_UObject_*_UObject_*_"), 0x3F8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("UseSound")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("UseSound"), 0x400);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("UseSound_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("UseSound_C"), 0x400);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CreatePIEWorldByDuplication")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CreatePIEWorldByDuplication"), 0x408);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CreatePIEWorldByDuplication__Ref_FWorldContext__Ptr_UWorld__Ref_FString")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CreatePIEWorldByDuplication__Ref_FWorldContext__Ptr_UWorld__Ref_FString"), 0x408);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("PostCreatePIEWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("PostCreatePIEWorld"), 0x410);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("PostCreatePIEWorld__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("PostCreatePIEWorld__Ptr_UWorld"), 0x410);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Experimental_ShouldPreDuplicateMap")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Experimental_ShouldPreDuplicateMap"), 0x418);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Experimental_ShouldPreDuplicateMap_C__C_FName")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Experimental_ShouldPreDuplicateMap_C__C_FName"), 0x418);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("InitializeAudioDeviceManager")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("InitializeAudioDeviceManager"), 0x420);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("InitializeHMDDevice")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("InitializeHMDDevice"), 0x428);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("InitializeEyeTrackingDevice")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("InitializeEyeTrackingDevice"), 0x430);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("RecordHMDAnalytics")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("RecordHMDAnalytics"), 0x438);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("InitializeObjectReferences")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("InitializeObjectReferences"), 0x440);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("InitializePortalServices")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("InitializePortalServices"), 0x448);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("InitializeRunningAverageDeltaTime")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("InitializeRunningAverageDeltaTime"), 0x450);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("SpawnServerActors")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("SpawnServerActors"), 0x458);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("SpawnServerActors__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("SpawnServerActors__Ptr_UWorld"), 0x458);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleNetworkFailure")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleNetworkFailure"), 0x460);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleNetworkFailure__Ptr_UWorld__Ptr_UNetDriver__ENetworkFailure_Type__Ref_C_FString")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleNetworkFailure__Ptr_UWorld__Ptr_UNetDriver__ENetworkFailure_Type__Ref_C_FString"), 0x460);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleTravelFailure")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleTravelFailure"), 0x468);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleTravelFailure__Ptr_UWorld__ETravelFailure_Type__Ref_C_FString")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleTravelFailure__Ptr_UWorld__ETravelFailure_Type__Ref_C_FString"), 0x468);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleNetworkLagStateChanged")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleNetworkLagStateChanged"), 0x470);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleNetworkLagStateChanged__Ptr_UWorld__Ptr_UNetDriver__ENetworkLagState_Type")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleNetworkLagStateChanged__Ptr_UWorld__Ptr_UNetDriver__ENetworkLagState_Type"), 0x470);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("NetworkRemapPath__Ptr_UPendingNetGame__Ref_FString__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("NetworkRemapPath__Ptr_UPendingNetGame__Ref_FString__bool"), 0x478);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("NetworkRemapPath__Ptr_UNetConnection__Ref_FString__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("NetworkRemapPath__Ptr_UNetConnection__Ref_FString__bool"), 0x480);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleOpenCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleOpenCommand"), 0x488);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleOpenCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleOpenCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld"), 0x488);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleTravelCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleTravelCommand"), 0x490);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleTravelCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleTravelCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld"), 0x490);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleStreamMapCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleStreamMapCommand"), 0x498);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleStreamMapCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleStreamMapCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld"), 0x498);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleServerTravelCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleServerTravelCommand"), 0x4A0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleServerTravelCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleServerTravelCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld"), 0x4A0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleDisconnectCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleDisconnectCommand"), 0x4A8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleDisconnectCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleDisconnectCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld"), 0x4A8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleReconnectCommand")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleReconnectCommand"), 0x4B0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleReconnectCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleReconnectCommand__Ptr_C_wchar_t__Ref_FOutputDevice__Ptr_UWorld"), 0x4B0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Browse")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Browse"), 0x4B8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("Browse__Ref_FWorldContext__FURL__Ref_FString")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("Browse__Ref_FWorldContext__FURL__Ref_FString"), 0x4B8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("TickWorldTravel")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("TickWorldTravel"), 0x4C0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("TickWorldTravel__Ref_FWorldContext__float")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("TickWorldTravel__Ref_FWorldContext__float"), 0x4C0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("LoadMap")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("LoadMap"), 0x4C8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("LoadMap__Ref_FWorldContext__FURL__Ptr_UPendingNetGame__Ref_FString")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("LoadMap__Ref_FWorldContext__FURL__Ptr_UPendingNetGame__Ref_FString"), 0x4C8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("RedrawViewports")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("RedrawViewports"), 0x4D0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("RedrawViewports__bool")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("RedrawViewports__bool"), 0x4D0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("TriggerStreamingDataRebuild")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("TriggerStreamingDataRebuild"), 0x4D8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("LoadMapRedrawViewports")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("LoadMapRedrawViewports"), 0x4E0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CancelAllPending")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CancelAllPending"), 0x4E8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CancelPending__Ptr_UNetDriver")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CancelPending__Ptr_UNetDriver"), 0x4F0);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CancelPending__Ref_FWorldContext")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CancelPending__Ref_FWorldContext"), 0x4F8);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CancelPending__Ptr_UWorld__Ptr_UPendingNetGame")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CancelPending__Ptr_UWorld__Ptr_UPendingNetGame"), 0x500);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("WorldIsPIEInNewViewport")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("WorldIsPIEInNewViewport"), 0x508);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("WorldIsPIEInNewViewport__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("WorldIsPIEInNewViewport__Ptr_UWorld"), 0x508);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CheckAndHandleStaleWorldObjectReferences")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CheckAndHandleStaleWorldObjectReferences"), 0x510);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CheckAndHandleStaleWorldObjectReferences__Ptr_FWorldContext")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CheckAndHandleStaleWorldObjectReferences__Ptr_FWorldContext"), 0x510);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("DestroyWorldContext")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("DestroyWorldContext"), 0x518);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("DestroyWorldContext__Ptr_UWorld")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("DestroyWorldContext__Ptr_UWorld"), 0x518);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("AreEditorAnalyticsEnabled")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("AreEditorAnalyticsEnabled"), 0x520);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("AreEditorAnalyticsEnabled_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("AreEditorAnalyticsEnabled_C"), 0x520);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CreateStartupAnalyticsAttributes")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CreateStartupAnalyticsAttributes"), 0x528);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("CreateStartupAnalyticsAttributes_C__Ref_TArray_FAnalyticsEventAttribute_FDefaultAllocator_")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("CreateStartupAnalyticsAttributes_C__Ref_TArray_FAnalyticsEventAttribute_FDefaultAllocator_"), 0x528);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsAutosaving")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsAutosaving"), 0x530);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("IsAutosaving_C__C_EPackageAutoSaveType")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("IsAutosaving_C__C_EPackageAutoSaveType"), 0x530);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldDoAsyncEndOfFrameTasks")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldDoAsyncEndOfFrameTasks"), 0x538);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldDoAsyncEndOfFrameTasks_C")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldDoAsyncEndOfFrameTasks_C"), 0x538);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("MovePendingLevel")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("MovePendingLevel"), 0x540);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("MovePendingLevel__Ref_FWorldContext")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("MovePendingLevel__Ref_FWorldContext"), 0x540);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("ShouldShutdownWorldNetDriver")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("ShouldShutdownWorldNetDriver"), 0x548);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleBrowseToDefaultMapFailure")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleBrowseToDefaultMapFailure"), 0x550);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleBrowseToDefaultMapFailure__Ref_FWorldContext__Ref_C_FString__Ref_C_FString")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleBrowseToDefaultMapFailure__Ref_FWorldContext__Ref_C_FString__Ref_C_FString"), 0x550);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleNetworkFailure_NotifyGameInstance")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleNetworkFailure_NotifyGameInstance"), 0x558);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleNetworkFailure_NotifyGameInstance__Ptr_UWorld__Ptr_UNetDriver__ENetworkFailure_Type")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleNetworkFailure_NotifyGameInstance__Ptr_UWorld__Ptr_UNetDriver__ENetworkFailure_Type"), 0x558);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleTravelFailure_NotifyGameInstance")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleTravelFailure_NotifyGameInstance"), 0x560);
}

if (auto it = UEngine::VTableLayoutMap.find(STR("HandleTravelFailure_NotifyGameInstance__Ptr_UWorld__ETravelFailure_Type")); it == UEngine::VTableLayoutMap.end())
{
    UEngine::VTableLayoutMap.emplace(STR("HandleTravelFailure_NotifyGameInstance__Ptr_UWorld__ETravelFailure_Type"), 0x560);
}


if (auto it = AGameMode::VTableLayoutMap.find(STR("__vecDelDtor")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HasMatchStarted")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HasMatchStarted"), 0x568);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HasMatchStarted_C")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HasMatchStarted_C"), 0x568);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("IsMatchInProgress")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("IsMatchInProgress"), 0x570);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("IsMatchInProgress_C")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("IsMatchInProgress_C"), 0x570);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HasMatchEnded")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HasMatchEnded"), 0x578);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HasMatchEnded_C")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HasMatchEnded_C"), 0x578);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("StartPlay")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("StartPlay"), 0x580);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PostSeamlessTravel")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PostSeamlessTravel"), 0x588);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("StartMatch")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("StartMatch"), 0x590);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("EndMatch")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("EndMatch"), 0x598);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("StartToLeaveMap")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("StartToLeaveMap"), 0x5A0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("RestartGame")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("RestartGame"), 0x5A8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ReturnToMainMenuHost")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ReturnToMainMenuHost"), 0x5B0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AbortMatch")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AbortMatch"), 0x5B8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetMatchState")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetMatchState"), 0x5C0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetMatchState__FName")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetMatchState__FName"), 0x5C0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HandleMatchIsWaitingToStart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HandleMatchIsWaitingToStart"), 0x5C8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ReadyToStartMatch")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ReadyToStartMatch"), 0x5D0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HandleMatchHasStarted")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HandleMatchHasStarted"), 0x5D8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ReadyToEndMatch")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ReadyToEndMatch"), 0x5E0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HandleMatchHasEnded")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HandleMatchHasEnded"), 0x5E8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HandleLeavingMap")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HandleLeavingMap"), 0x5F0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HandleMatchAborted")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HandleMatchAborted"), 0x5F8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetBandwidthLimit")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetBandwidthLimit"), 0x600);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetBandwidthLimit__float")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetBandwidthLimit__float"), 0x600);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ShouldReset")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ShouldReset"), 0x608);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ShouldReset__Ptr_AActor")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ShouldReset__Ptr_AActor"), 0x608);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ResetLevel")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ResetLevel"), 0x610);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("InitGame")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("InitGame"), 0x618);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("InitGame__Ref_C_FString__Ref_C_FString__Ref_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("InitGame__Ref_C_FString__Ref_C_FString__Ref_FString"), 0x618);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("InitGameState")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("InitGameState"), 0x620);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetNetworkNumber")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetNetworkNumber"), 0x628);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetNumPlayers")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetNumPlayers"), 0x630);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetPause")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetPause"), 0x638);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetPause__Ptr_APlayerController__TBaseDelegate_bool_")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetPause__Ptr_APlayerController__TBaseDelegate_bool_"), 0x638);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ClearPause")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ClearPause"), 0x640);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetDefaultGameClassPath")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetDefaultGameClassPath"), 0x648);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetDefaultGameClassPath_C__Ref_C_FString__Ref_C_FString__Ref_C_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetDefaultGameClassPath_C__Ref_C_FString__Ref_C_FString__Ref_C_FString"), 0x648);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetGameModeClass")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetGameModeClass"), 0x650);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetGameModeClass_C__Ref_C_FString__Ref_C_FString__Ref_C_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetGameModeClass_C__Ref_C_FString__Ref_C_FString__Ref_C_FString"), 0x650);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetGameSessionClass")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetGameSessionClass"), 0x658);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetGameSessionClass_C")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetGameSessionClass_C"), 0x658);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("NotifyPendingConnectionLost")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("NotifyPendingConnectionLost"), 0x660);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetTravelType")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetTravelType"), 0x668);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ProcessServerTravel")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ProcessServerTravel"), 0x670);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ProcessServerTravel__Ref_C_FString__bool")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ProcessServerTravel__Ref_C_FString__bool"), 0x670);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ProcessClientTravel")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ProcessClientTravel"), 0x678);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ProcessClientTravel__Ref_FString__FGuid__bool__bool")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ProcessClientTravel__Ref_FString__FGuid__bool__bool"), 0x678);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PreLogin")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PreLogin"), 0x680);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PreLogin__Ref_C_FString__Ref_C_FString__Ref_C_TSharedPtr_FUniqueNetId_0___Ref_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PreLogin__Ref_C_FString__Ref_C_FString__Ref_C_TSharedPtr_FUniqueNetId_0___Ref_FString"), 0x680);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("Login")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("Login"), 0x688);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("Login__Ptr_UPlayer__Ref_C_FString__Ref_C_FString__Ref_C_TSharedPtr_FUniqueNetId_0___Ref_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("Login__Ptr_UPlayer__Ref_C_FString__Ref_C_FString__Ref_C_TSharedPtr_FUniqueNetId_0___Ref_FString"), 0x688);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PostLogin")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PostLogin"), 0x690);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PostLogin__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PostLogin__Ptr_APlayerController"), 0x690);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("K2_PostLogin")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("K2_PostLogin"), 0x698);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("K2_PostLogin__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("K2_PostLogin__Ptr_APlayerController"), 0x698);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SpawnPlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SpawnPlayerController"), 0x6A0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SpawnPlayerController__Ref_C_FVector__Ref_C_FRotator")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SpawnPlayerController__Ref_C_FVector__Ref_C_FRotator"), 0x6A0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("MustSpectate")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("MustSpectate"), 0x6A8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("MustSpectate_C__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("MustSpectate_C__Ptr_APlayerController"), 0x6A8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetDefaultPawnClassForController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetDefaultPawnClassForController"), 0x6B0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetDefaultPawnClassForController__Ptr_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetDefaultPawnClassForController__Ptr_AController"), 0x6B0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("InitStartSpot")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("InitStartSpot"), 0x6B8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("InitStartSpot__Ptr_AActor__Ptr_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("InitStartSpot__Ptr_AActor__Ptr_AController"), 0x6B8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AddPlayerStart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AddPlayerStart"), 0x6C0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AddPlayerStart__Ptr_APlayerStart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AddPlayerStart__Ptr_APlayerStart"), 0x6C0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("RemovePlayerStart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("RemovePlayerStart"), 0x6C8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("RemovePlayerStart__Ptr_APlayerStart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("RemovePlayerStart__Ptr_APlayerStart"), 0x6C8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SpawnDefaultPawnFor")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SpawnDefaultPawnFor"), 0x6D0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SpawnDefaultPawnFor__Ptr_AController__Ptr_AActor")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SpawnDefaultPawnFor__Ptr_AController__Ptr_AActor"), 0x6D0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ReplicateStreamingStatus")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ReplicateStreamingStatus"), 0x6D8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ReplicateStreamingStatus__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ReplicateStreamingStatus__Ptr_APlayerController"), 0x6D8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GenericPlayerInitialization")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GenericPlayerInitialization"), 0x6E0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GenericPlayerInitialization__Ptr_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GenericPlayerInitialization__Ptr_AController"), 0x6E0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("StartNewPlayer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("StartNewPlayer"), 0x6E8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("StartNewPlayer__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("StartNewPlayer__Ptr_APlayerController"), 0x6E8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("Logout")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("Logout"), 0x6F0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("Logout__Ptr_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("Logout__Ptr_AController"), 0x6F0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetPlayerDefaults")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetPlayerDefaults"), 0x6F8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetPlayerDefaults__Ptr_APawn")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetPlayerDefaults__Ptr_APawn"), 0x6F8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("CanSpectate")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("CanSpectate"), 0x700);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("CanSpectate__Ptr_APlayerController__Ptr_APlayerState")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("CanSpectate__Ptr_APlayerController__Ptr_APlayerState"), 0x700);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ChangeName")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ChangeName"), 0x708);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ChangeName__Ptr_AController__Ref_C_FString__bool")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ChangeName__Ptr_AController__Ref_C_FString__bool"), 0x708);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SendPlayer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SendPlayer"), 0x710);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SendPlayer__Ptr_APlayerController__Ref_C_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SendPlayer__Ptr_APlayerController__Ref_C_FString"), 0x710);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("Broadcast")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("Broadcast"), 0x718);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("Broadcast__Ptr_AActor__Ref_C_FString__FName")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("Broadcast__Ptr_AActor__Ref_C_FString__FName"), 0x718);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("BroadcastLocalized")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("BroadcastLocalized"), 0x720);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("BroadcastLocalized__Ptr_AActor__TSubclassOf_ULocalMessage___int32__Ptr_APlayerState__Ptr_APlayerState__Ptr_UObject")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("BroadcastLocalized__Ptr_AActor__TSubclassOf_ULocalMessage___int32__Ptr_APlayerState__Ptr_APlayerState__Ptr_UObject"), 0x720);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ShouldSpawnAtStartSpot")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ShouldSpawnAtStartSpot"), 0x728);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ShouldSpawnAtStartSpot__Ptr_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ShouldSpawnAtStartSpot__Ptr_AController"), 0x728);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("FindPlayerStart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("FindPlayerStart"), 0x730);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("FindPlayerStart__Ptr_AController__Ref_C_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("FindPlayerStart__Ptr_AController__Ref_C_FString"), 0x730);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ChoosePlayerStart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ChoosePlayerStart"), 0x738);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("ChoosePlayerStart__Ptr_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("ChoosePlayerStart__Ptr_AController"), 0x738);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PlayerCanRestart")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PlayerCanRestart"), 0x740);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PlayerCanRestart__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PlayerCanRestart__Ptr_APlayerController"), 0x740);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("UpdateGameplayMuteList")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("UpdateGameplayMuteList"), 0x748);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("UpdateGameplayMuteList__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("UpdateGameplayMuteList__Ptr_APlayerController"), 0x748);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AllowCheats")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AllowCheats"), 0x750);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AllowCheats__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AllowCheats__Ptr_APlayerController"), 0x750);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AllowPausing")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AllowPausing"), 0x758);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AllowPausing__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AllowPausing__Ptr_APlayerController"), 0x758);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PreCommitMapChange")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PreCommitMapChange"), 0x760);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PreCommitMapChange__Ref_C_FString__Ref_C_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PreCommitMapChange__Ref_C_FString__Ref_C_FString"), 0x760);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("PostCommitMapChange")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("PostCommitMapChange"), 0x768);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AddInactivePlayer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AddInactivePlayer"), 0x770);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("AddInactivePlayer__Ptr_APlayerState__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("AddInactivePlayer__Ptr_APlayerState__Ptr_APlayerController"), 0x770);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("FindInactivePlayer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("FindInactivePlayer"), 0x778);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("FindInactivePlayer__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("FindInactivePlayer__Ptr_APlayerController"), 0x778);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("OverridePlayerState")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("OverridePlayerState"), 0x780);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("OverridePlayerState__Ptr_APlayerController__Ptr_APlayerState")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("OverridePlayerState__Ptr_APlayerController__Ptr_APlayerState"), 0x780);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetSeamlessTravelActorList")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetSeamlessTravelActorList"), 0x788);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetSeamlessTravelActorList__bool__Ref_TArray_AActor_*_FDefaultAllocator_")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetSeamlessTravelActorList__bool__Ref_TArray_AActor_*_FDefaultAllocator_"), 0x788);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetRedirectURL")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetRedirectURL"), 0x790);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("GetRedirectURL_C__Ref_C_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("GetRedirectURL_C__Ref_C_FString"), 0x790);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SwapPlayerControllers")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SwapPlayerControllers"), 0x798);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SwapPlayerControllers__Ptr_APlayerController__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SwapPlayerControllers__Ptr_APlayerController__Ptr_APlayerController"), 0x798);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HandleSeamlessTravelPlayer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HandleSeamlessTravelPlayer"), 0x7A0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("HandleSeamlessTravelPlayer__Ptr_Ref_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("HandleSeamlessTravelPlayer__Ptr_Ref_AController"), 0x7A0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetSeamlessTravelViewTarget")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetSeamlessTravelViewTarget"), 0x7A8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("SetSeamlessTravelViewTarget__Ptr_APlayerController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("SetSeamlessTravelViewTarget__Ptr_APlayerController"), 0x7A8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("MatineeCancelled")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("MatineeCancelled"), 0x7B0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("RestartPlayer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("RestartPlayer"), 0x7B8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("RestartPlayer__Ptr_AController")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("RestartPlayer__Ptr_AController"), 0x7B8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("DefaultTimer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("DefaultTimer"), 0x7C0);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("InitNewPlayer")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("InitNewPlayer"), 0x7C8);
}

if (auto it = AGameMode::VTableLayoutMap.find(STR("InitNewPlayer__Ptr_APlayerController__Ref_C_TSharedPtr_FUniqueNetId_0___Ref_C_FString__Ref_C_FString")); it == AGameMode::VTableLayoutMap.end())
{
    AGameMode::VTableLayoutMap.emplace(STR("InitNewPlayer__Ptr_APlayerController__Ref_C_TSharedPtr_FUniqueNetId_0___Ref_C_FString__Ref_C_FString"), 0x7C8);
}


if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("__vecDelDtor")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("PlayerAdded")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("PlayerAdded"), 0x1D8);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("PlayerAdded__Ptr_UGameViewportClient__int32")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("PlayerAdded__Ptr_UGameViewportClient__int32"), 0x1D8);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("InitOnlineSession")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("InitOnlineSession"), 0x1E0);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("PlayerRemoved")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("PlayerRemoved"), 0x1E8);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("SpawnPlayActor")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("SpawnPlayActor"), 0x1F0);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("SpawnPlayActor__Ref_C_FString__Ref_FString__Ptr_UWorld")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("SpawnPlayActor__Ref_C_FString__Ref_FString__Ptr_UWorld"), 0x1F0);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("SendSplitJoin")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("SendSplitJoin"), 0x1F8);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("SetControllerId")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("SetControllerId"), 0x200);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("SetControllerId__int32")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("SetControllerId__int32"), 0x200);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("GetNickname")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("GetNickname"), 0x208);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("GetNickname_C")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("GetNickname_C"), 0x208);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("GetGameLoginOptions")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("GetGameLoginOptions"), 0x210);
}

if (auto it = ULocalPlayer::VTableLayoutMap.find(STR("GetGameLoginOptions_C")); it == ULocalPlayer::VTableLayoutMap.end())
{
    ULocalPlayer::VTableLayoutMap.emplace(STR("GetGameLoginOptions_C"), 0x210);
}


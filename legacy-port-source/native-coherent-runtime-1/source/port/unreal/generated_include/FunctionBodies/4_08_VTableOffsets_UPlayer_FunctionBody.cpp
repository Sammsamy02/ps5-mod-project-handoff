if (auto it = UPlayer::VTableLayoutMap.find(STR("__vecDelDtor")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("_getUObject")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("_getUObject"), 0x1B0);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("_getUObject_C")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("_getUObject_C"), 0x1B0);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("SwitchController")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("SwitchController"), 0x1B8);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("SwitchController__Ptr_APlayerController")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("SwitchController__Ptr_APlayerController"), 0x1B8);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("HandleDisconnect")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("HandleDisconnect"), 0x1C0);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("HandleDisconnect__Ptr_UWorld__Ptr_UNetDriver")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("HandleDisconnect__Ptr_UWorld__Ptr_UNetDriver"), 0x1C0);
}


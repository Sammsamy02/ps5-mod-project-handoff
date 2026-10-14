if (auto it = UPlayer::VTableLayoutMap.find(STR("__vecDelDtor")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("SwitchController")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("SwitchController"), 0x268);
}

if (auto it = UPlayer::VTableLayoutMap.find(STR("SwitchController__Ptr_APlayerController")); it == UPlayer::VTableLayoutMap.end())
{
    UPlayer::VTableLayoutMap.emplace(STR("SwitchController__Ptr_APlayerController"), 0x268);
}


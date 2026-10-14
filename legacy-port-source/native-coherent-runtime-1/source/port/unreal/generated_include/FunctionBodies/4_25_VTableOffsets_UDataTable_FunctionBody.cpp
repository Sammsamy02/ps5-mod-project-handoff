if (auto it = UDataTable::VTableLayoutMap.find(STR("__vecDelDtor")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("__vecDelDtor"), 0x0);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("__vecDelDtor__uint32")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("__vecDelDtor__uint32"), 0x0);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("GetNonConstRowMap")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("GetNonConstRowMap"), 0x260);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("AddRowInternal")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("AddRowInternal"), 0x268);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("AddRowInternal__FName__Ptr_uint8")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("AddRowInternal__FName__Ptr_uint8"), 0x268);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("GetRowMap")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("GetRowMap"), 0x270);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("GetRowMap_C")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("GetRowMap_C"), 0x278);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("AllowDuplicateRowsOnImport")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("AllowDuplicateRowsOnImport"), 0x280);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("AllowDuplicateRowsOnImport_C")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("AllowDuplicateRowsOnImport_C"), 0x280);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("EmptyTable")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("EmptyTable"), 0x288);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("RemoveRow")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("RemoveRow"), 0x290);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("RemoveRow__FName")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("RemoveRow__FName"), 0x290);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("AddRow")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("AddRow"), 0x298);
}

if (auto it = UDataTable::VTableLayoutMap.find(STR("AddRow__FName__Ref_C_FTableRowBase")); it == UDataTable::VTableLayoutMap.end())
{
    UDataTable::VTableLayoutMap.emplace(STR("AddRow__FName__Ref_C_FTableRowBase"), 0x298);
}


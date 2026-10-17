#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Helpers/Casting.hpp>

namespace RC::Unreal
{
    // Helper functions to calculate runtime TPair sizes when FName size differs from compile-time
    // This is needed when the game has WITH_CASE_PRESERVING_NAME defined (12-byte FName vs 8-byte)

    // Calculate runtime size of TPair<FName, uint8> element
    inline int32 GetRuntimeTPairFNameUint8Size()
    {
        int32 fnameSize = FName::StaticSize();
        int32 fnameAlign = FName::StaticAlignment();
        // uint8 follows FName directly (no alignment needed for uint8)
        // Total padded to FName's alignment
        return (fnameSize + 1 + fnameAlign - 1) & ~(fnameAlign - 1);
    }

    // Calculate runtime size of TPair<FName, int64> element
    inline int32 GetRuntimeTPairFNameInt64Size()
    {
        int32 fnameSize = FName::StaticSize();
        // int64 needs 8-byte alignment, so add padding after FName
        int32 valueOffset = (fnameSize + 7) & ~7;
        // Struct alignment is 8 (max of FName alignment and int64 alignment)
        return valueOffset + 8;
    }

    // Read FName from TPair<FName, uint8/int64> at given address using runtime size
    inline FName ReadFNameFromPair(uint8* ptr)
    {
        FName result;
        std::memcpy(&result, ptr, std::min(FName::StaticSize(), static_cast<int32>(sizeof(FName))));
        return result;
    }

    // Read uint8 value from TPair<FName, uint8> at given address
    inline uint8 ReadUint8ValueFromPair(uint8* ptr)
    {
        return *reinterpret_cast<uint8*>(ptr + FName::StaticSize());
    }

    // Read int64 value from TPair<FName, int64> at given address
    inline int64 ReadInt64ValueFromPair(uint8* ptr)
    {
        int32 fnameSize = FName::StaticSize();
        int32 valueOffset = (fnameSize + 7) & ~7;  // 8-byte aligned
        return *reinterpret_cast<int64*>(ptr + valueOffset);
    }

    IMPLEMENT_EXTERNAL_OBJECT_CLASS(UEnum);
    IMPLEMENT_EXTERNAL_OBJECT_CLASS(UUserDefinedEnum);

#include <MemberVariableLayout_SrcWrapper_UEnum.hpp>

    // Removed duplicate getters - use the generated versioned ones:
    // GetNamesBase(), GetNames409(), GetNames415(), GetNames416(), GetNames507()

    void UEnum::FNameData::MakeDynamic()
    {
        if (IsDynamic()) { return; }

        const int32 Count = NumValues;
        if (Count == 0) { return; }

        // Allocate new arrays
        FName* NewNames = static_cast<FName*>(FMemory::Malloc(Count * FName::StaticSize(), FName::StaticAlignment()));
        int64* NewValues = static_cast<int64*>(FMemory::Malloc(Count * sizeof(int64), alignof(int64)));

        // Copy existing data
        const FName* OldNames = GetNames();
        const int64* OldValues = GetValues();
        for (int32 i = 0; i < Count; ++i)
        {
            new (&NewNames[i]) FName(OldNames[i]);
            NewValues[i] = OldValues[i];
        }

        // Set tagged pointers with dynamic flag
        TaggedNames = reinterpret_cast<uintptr_t>(NewNames) | TagDynamic;
        TaggedValues = reinterpret_cast<uintptr_t>(NewValues) | TagDynamic;
    }

    void UEnum::FNameData::Reallocate(int32 NewSize)
    {
        const int32 OldSize = NumValues;
        const int32 CopyCount = (OldSize < NewSize) ? OldSize : NewSize;

        // Allocate new arrays
        FName* NewNames = static_cast<FName*>(FMemory::Malloc(NewSize * FName::StaticSize(), FName::StaticAlignment()));
        int64* NewValues = static_cast<int64*>(FMemory::Malloc(NewSize * sizeof(int64), alignof(int64)));

        // Copy existing data
        const FName* OldNames = GetNames();
        const int64* OldValues = GetValues();
        for (int32 i = 0; i < CopyCount; ++i)
        {
            new (&NewNames[i]) FName(OldNames[i]);
            NewValues[i] = OldValues[i];
        }

        // Default-initialize new elements if growing
        for (int32 i = CopyCount; i < NewSize; ++i)
        {
            new (&NewNames[i]) FName();
            NewValues[i] = 0;
        }

        // Free old dynamic arrays if they were dynamic
        if (IsDynamic())
        {
            FName* OldNamesPtr = GetMutableNames();
            for (int32 i = 0; i < OldSize; ++i)
            {
                OldNamesPtr[i].~FName();
            }
            FMemory::Free(OldNamesPtr);
            FMemory::Free(GetMutableValues());
        }

        // Set tagged pointers with dynamic flag
        TaggedNames = reinterpret_cast<uintptr_t>(NewNames) | TagDynamic;
        TaggedValues = reinterpret_cast<uintptr_t>(NewValues) | TagDynamic;
        NumValues = NewSize;
    }

    auto UEnum::GetEnumNames() -> TArray<TPair<FName, int64>>
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>, value = array index
            // TArray<FName> iteration uses FName::StaticSize() automatically
            TArray<TPair<FName, int64>> ReturnArray{};
            auto& Names = GetNamesBase();
            for (int32 i = 0; i < Names.Num(); ++i)
            {
                ReturnArray.Emplace(Names[i], static_cast<int64>(i));
            }
            return ReturnArray;
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            TArray<TPair<FName, int64>> ReturnArray{};
            auto& Names = GetNames409();
            // Use manual iteration with runtime TPair size
            int32 count = Names.Num();
            int32 elementSize = GetRuntimeTPairFNameUint8Size();
            uint8* dataPtr = reinterpret_cast<uint8*>(Names.GetData());
            for (int32 i = 0; i < count; ++i)
            {
                FName name = ReadFNameFromPair(dataPtr);
                uint8 value = ReadUint8ValueFromPair(dataPtr);
                ReturnArray.Emplace(name, static_cast<int64>(value));
                dataPtr += elementSize;
            }
            return ReturnArray;
        }
        else if (Version::IsBelow(5, 7))
        {
            // 4.15 to 5.6: TArray<TPair<FName, int64>>
            TArray<TPair<FName, int64>> ReturnArray{};
            auto& Names = GetNames415();
            // Use manual iteration with runtime TPair size
            int32 count = Names.Num();
            int32 elementSize = GetRuntimeTPairFNameInt64Size();
            uint8* dataPtr = reinterpret_cast<uint8*>(Names.GetData());
            for (int32 i = 0; i < count; ++i)
            {
                FName name = ReadFNameFromPair(dataPtr);
                int64 value = ReadInt64ValueFromPair(dataPtr);
                ReturnArray.Emplace(name, value);
                dataPtr += elementSize;
            }
            return ReturnArray;
        }
        else
        {
            // UE 5.7+: FNameData uses parallel arrays, construct TArray for compatibility
            TArray<TPair<FName, int64>> ReturnArray{};
            auto& NameData = GetNames507();
            // Skip uninitialized enums (TagDynamic bit not set)
            if (!NameData.IsDynamic()) { return ReturnArray; }
            for (int32 i = 0; i < NameData.Num(); ++i)
            {
                ReturnArray.Emplace(NameData.GetNameAt(i), NameData.GetValueAt(i));
            }
            return ReturnArray;
        }
    }

    auto UEnum::GetEnumNameByIndex(int32 Index) -> TPair<FName, int64>
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>, value = array index
            // TArray<FName> uses FName::StaticSize() for access automatically
            auto& Names = GetNamesBase();
            if (!Names.IsValidIndex(Index)) { Output::send<LogLevel::Error>(STR("Enum index out of bounds: {} from an array of size {} for Enum {}"), Index, Names.Num(), this->GetName()); }
            return {Names[Index], static_cast<int64>(Index)};
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            auto& Names = GetNames409();
            if (!Names.IsValidIndex(Index)) { Output::send<LogLevel::Error>(STR("Enum index out of bounds: {} from an array of size {} for Enum {}"), Index, Names.Num(), this->GetName()); }
            // Use runtime TPair size for access
            uint8* dataPtr = reinterpret_cast<uint8*>(Names.GetData()) + Index * GetRuntimeTPairFNameUint8Size();
            FName name = ReadFNameFromPair(dataPtr);
            uint8 value = ReadUint8ValueFromPair(dataPtr);
            return {name, static_cast<int64>(value)};
        }
        else if (Version::IsBelow(5, 7))
        {
            auto& Names = GetNames415();
            if (!Names.IsValidIndex(Index)) { Output::send<LogLevel::Error>(STR("Enum index out of bounds: {} from an array of size {} for Enum {}"), Index, Names.Num(), this->GetName()); }
            // Use runtime TPair size for access
            uint8* dataPtr = reinterpret_cast<uint8*>(Names.GetData()) + Index * GetRuntimeTPairFNameInt64Size();
            FName name = ReadFNameFromPair(dataPtr);
            int64 value = ReadInt64ValueFromPair(dataPtr);
            return {name, value};
        }
        else
        {
            auto& NameData = GetNames507();
            // Skip uninitialized enums (TagDynamic bit not set)
            if (!NameData.IsDynamic()) { return {}; }
            if (!NameData.IsValidIndex(Index)) { Output::send<LogLevel::Error>(STR("Enum index out of bounds: {} from an array of size {} for Enum {}"), Index, NameData.Num(), this->GetName()); }
            return NameData[Index];
        }
    }

    void UEnum::GetEnumNamesAsVector(std::vector<std::pair<FName, int64>>& OutNames)
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>, value = array index
            auto& Names = GetNamesBase();
            for (int32 i = 0; i < Names.Num(); ++i)
            {
                OutNames.emplace_back(std::pair{Names[i], static_cast<int64>(i)});
            }
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            for (TPair<FName, uint8>& Elem : GetNames409())
            {
                OutNames.emplace_back(std::pair{Elem.Key, Elem.Value});
            }
        }
        else if (Version::IsBelow(5, 7))
        {
            for (TPair<FName, int64>& Elem : GetNames415())
            {
                OutNames.emplace_back(std::pair{Elem.Key, Elem.Value});
            }
        }
        else
        {
            auto& NameData = GetNames507();
            // Skip uninitialized enums (TagDynamic bit not set)
            if (!NameData.IsDynamic()) { return; }
            for (int32 i = 0; i < NameData.Num(); ++i)
            {
                OutNames.emplace_back(std::pair{NameData.GetNameAt(i), NameData.GetValueAt(i)});
            }
        }
    }

    auto UEnum::GetEnumFlags() -> EEnumFlags
    {
        if (Version::IsBelow(4, 26))
        {
            // EEnumFlags was added in 4.26
            // Let's assume the 'None' flag is valid for whatever code calls this function in <4.26
            return EEnumFlags::None;
        }
        else
        {
            // The offset of 'Names' + the size of 'Names' + the size of 'CppForm' = offset of 'EnumFlags'
            return GetEnumFlags_Internal();
        }
    }

    FName UEnum::GetNameByValue(int64 Value)
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>, value = array index
            auto& Names = GetNamesBase();
            if (Value >= 0 && Value < Names.Num())
            {
                return Names[static_cast<int32>(Value)];
            }
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            if (Value < std::numeric_limits<uint8_t>::min() || Value > std::numeric_limits<uint8_t>::max())
            {
                Output::send<LogLevel::Error>(STR("Invalid value used for enumerator value: {}.\n"), this->GetName());
                return {};
            }

            uint8 ValueAsU8 = static_cast<uint8>(Value);

            for (TPair<FName, uint8>& Elem : GetNames409())
            {
                if (Elem.Value == ValueAsU8)
                {
                    return Elem.Key;
                }
            }
        }
        else if (Version::IsBelow(5, 7))
        {
            for (TPair<FName, int64>& Elem : GetNames415())
            {
                if (Elem.Value == Value)
                {
                    return Elem.Key;
                }
            }
        }
        else
        {
            auto& NameData = GetNames507();
            // Skip uninitialized enums (TagDynamic bit not set)
            if (!NameData.IsDynamic()) { return {}; }
            const int64* Values = NameData.GetValues();
            const FName* Names = NameData.GetNames();
            for (int32 i = 0; i < NameData.Num(); ++i)
            {
                if (Values[i] == Value)
                {
                    return Names[i];
                }
            }
        }
        return {};
    }

    int32 UEnum::NumEnums()
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>
            return GetNamesBase().Num();
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            return GetNames409().Num();
        }
        else if (Version::IsBelow(5, 7))
        {
            return GetNames415().Num();
        }
        else
        {
            auto& NameData = GetNames507();
            // Skip uninitialized enums (TagDynamic bit not set)
            if (!NameData.IsDynamic()) { return 0; }
            return NameData.Num();
        }
    }

    StringType UEnum::GenerateEnumPrefix()
    {
        StringType enum_prefix;

        if (NumEnums() > 0)
        {
            enum_prefix = GetEnumNameByIndex(0).Key.ToString();
            
            // For each item in the enumeration, trim the prefix as much as necessary to keep it a prefix.
            // This ensures that once all items have been processed, a common prefix will have been constructed.
            // This will be the longest common prefix since as little as possible is trimmed at each step.
            for (auto [Name, Value] : ForEachName())
            {
                StringType EnumItemName = Name.ToString();

                // Find the length of the longest common prefix of Prefix and EnumItemName.
                int32 PrefixIdx = 0;
                while (PrefixIdx < enum_prefix.length() && PrefixIdx < EnumItemName.length() && enum_prefix[PrefixIdx] == EnumItemName[PrefixIdx])
                {
                    PrefixIdx++;
                }

                // Trim the prefix to the length of the common prefix.
                enum_prefix.resize(PrefixIdx);
            }

            // Find the index of the rightmost underscore in the prefix.
            const int64 UnderscoreIdx = enum_prefix.find_last_of(STR("_"));

            // If an underscore was found, trim the prefix so only the part before the rightmost underscore is included.
            if (UnderscoreIdx > 0)
            {
                enum_prefix.resize(UnderscoreIdx);
            }
            else
            {
                // no underscores in the common prefix - this probably indicates that the names
                // for this enum are not using Epic's notation, so just empty the prefix so that
                // the max item will use the full name of the enum
                enum_prefix.clear();
            }
        }
        
        // If no common prefix was found, or if the enum does not contain any entries,
        // use the name of the enumeration instead.
        if (enum_prefix.length() == 0)
        {
            enum_prefix = GetName();
        }
        return enum_prefix;
    }

    void UEnum::EditNameAt(int32 Index, FName NewName)
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>
            GetNamesBase()[Index] = NewName;
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            GetNames409()[Index].Key = NewName;
        }
        else if (Version::IsBelow(5, 7))
        {
            GetNames415()[Index].Key = NewName;
        }
        else
        {
            auto& NameData = GetNames507();
            // Skip uninitialized enums
            if (!NameData.IsDynamic()) { return; }
            NameData.MakeDynamic();  // Ensure we have mutable arrays
            NameData.GetMutableNames()[Index] = NewName;
        }
    }

    void UEnum::EditValueAt(int32 Index, int64 NewValue)
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>, value = array index (cannot be edited)
            Output::send<LogLevel::Warning>(STR("Cannot edit enum values in pre-4.9: values are implicit (value = index) for Enum {}.\n"), this->GetName());
            return;
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            if (NewValue < std::numeric_limits<uint8_t>::min() || NewValue > std::numeric_limits<uint8_t>::max())
            {
                Output::send<LogLevel::Error>(STR("Invalid value used for enumerator value: {}.\n"), this->GetName());
                return;
            }
            uint8 ValueAsU8 = static_cast<uint8>(NewValue);
            GetNames409()[Index].Value = ValueAsU8;
        }
        else if (Version::IsBelow(5, 7))
        {
            GetNames415()[Index].Value = NewValue;
        }
        else
        {
            auto& NameData = GetNames507();
            // Skip uninitialized enums
            if (!NameData.IsDynamic()) { return; }
            NameData.MakeDynamic();  // Ensure we have mutable arrays
            NameData.GetMutableValues()[Index] = NewValue;
        }
    }

    void UEnum::RemoveFromNamesAt(int32 Index, int32 Count, EAllowShrinking AllowShrinking)
    {
        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>
            auto& Names = GetNamesBase();
            Names.RemoveAt(Index, Count, AllowShrinking);
        }
        else if (Version::IsBelow(4, 15))
        {
            // 4.9 to 4.14: TArray<TPair<FName, uint8>>
            auto& Names = GetNames409();
            Names.RemoveAt(Index, Count, AllowShrinking);
        }
        else if (Version::IsBelow(5, 7))
        {
            auto& Names = GetNames415();
            Names.RemoveAt(Index, Count, AllowShrinking);
        }
        else
        {
            // UE 5.7+: FNameData uses parallel arrays, we need to reallocate
            auto& NameData = GetNames507();
            // Skip uninitialized enums
            if (!NameData.IsDynamic()) { return; }
            const int32 OldSize = NameData.Num();
            const int32 NewSize = OldSize - Count;

            if (NewSize <= 0)
            {
                // Removing all elements - just reallocate to empty
                NameData.Reallocate(0);
                return;
            }

            // Get current data before reallocation
            const FName* OldNames = NameData.GetNames();
            const int64* OldValues = NameData.GetValues();

            // Allocate new arrays
            FName* NewNames = static_cast<FName*>(FMemory::Malloc(NewSize * FName::StaticSize(), FName::StaticAlignment()));
            int64* NewValues = static_cast<int64*>(FMemory::Malloc(NewSize * sizeof(int64), alignof(int64)));

            // Copy elements before the removed range
            int32 WriteIdx = 0;
            for (int32 i = 0; i < Index; ++i)
            {
                new (&NewNames[WriteIdx]) FName(OldNames[i]);
                NewValues[WriteIdx] = OldValues[i];
                ++WriteIdx;
            }

            // Copy elements after the removed range
            for (int32 i = Index + Count; i < OldSize; ++i)
            {
                new (&NewNames[WriteIdx]) FName(OldNames[i]);
                NewValues[WriteIdx] = OldValues[i];
                ++WriteIdx;
            }

            // Free old dynamic arrays if they were dynamic
            if (NameData.IsDynamic())
            {
                FName* OldNamesPtr = NameData.GetMutableNames();
                for (int32 i = 0; i < OldSize; ++i)
                {
                    OldNamesPtr[i].~FName();
                }
                FMemory::Free(OldNamesPtr);
                FMemory::Free(NameData.GetMutableValues());
            }

            // Set tagged pointers with dynamic flag
            NameData.TaggedNames = reinterpret_cast<uintptr_t>(NewNames) | FNameData::TagDynamic;
            NameData.TaggedValues = reinterpret_cast<uintptr_t>(NewValues) | FNameData::TagDynamic;
            NameData.NumValues = NewSize;
        }
    }

    int32 UEnum::InsertIntoNames(const TPair<FName, int64>& Item, int32 Index, bool bShiftValues)
    {
        auto InsertIntoNamesArray = [](auto& NamesArray, const auto NewItem, int32 Index, bool bShiftValues) {
            if (bShiftValues)
            {
                for (auto& CheckName : NamesArray)
                {
                    if (NewItem.Value <= CheckName.Value)
                    {
                        CheckName.Value = CheckName.Value + 1;
                    }
                }
            }
            return NamesArray.Insert(NewItem, Index);
        };

        if (Version::IsBelow(4, 9))
        {
            // Pre-4.9: Names was just TArray<FName>, value = index (bShiftValues is meaningless)
            auto& Names = GetNamesBase();
            return Names.Insert(Item.Key, Index);
        }
        else if (Version::IsBelow(4, 15))
        {
            if (Item.Value < std::numeric_limits<uint8_t>::min() || Item.Value > std::numeric_limits<uint8_t>::max())
            {
                Output::send<LogLevel::Error>(STR("Value < or > uint8 maximum used for enumerator value in <4.15: {}.\n"), this->GetName());
                return -1;
            }
            const TPair<FName, uint8> NewItem{Item.Key, static_cast<uint8>(Item.Value)};
            return InsertIntoNamesArray(GetNames409(), NewItem, Index, bShiftValues);
        }
        else if (Version::IsBelow(5, 7))
        {
            return InsertIntoNamesArray(GetNames415(), Item, Index, bShiftValues);
        }
        else
        {
            // UE 5.7+: FNameData uses parallel arrays, we need to reallocate
            auto& NameData = GetNames507();
            // Skip uninitialized enums
            if (!NameData.IsDynamic()) { return -1; }
            const int32 OldSize = NameData.Num();
            const int32 NewSize = OldSize + 1;

            // Get current data before reallocation
            const FName* OldNames = NameData.GetNames();
            const int64* OldValues = NameData.GetValues();

            // Allocate new arrays
            FName* NewNames = static_cast<FName*>(FMemory::Malloc(NewSize * FName::StaticSize(), FName::StaticAlignment()));
            int64* NewValues = static_cast<int64*>(FMemory::Malloc(NewSize * sizeof(int64), alignof(int64)));

            // Copy elements before the insertion point
            for (int32 i = 0; i < Index; ++i)
            {
                new (&NewNames[i]) FName(OldNames[i]);
                NewValues[i] = bShiftValues && Item.Value <= OldValues[i] ? OldValues[i] + 1 : OldValues[i];
            }

            // Insert the new element
            new (&NewNames[Index]) FName(Item.Key);
            NewValues[Index] = Item.Value;

            // Copy elements after the insertion point
            for (int32 i = Index; i < OldSize; ++i)
            {
                new (&NewNames[i + 1]) FName(OldNames[i]);
                NewValues[i + 1] = bShiftValues && Item.Value <= OldValues[i] ? OldValues[i] + 1 : OldValues[i];
            }

            // Free old dynamic arrays if they were dynamic
            if (NameData.IsDynamic())
            {
                FName* OldNamesPtr = NameData.GetMutableNames();
                for (int32 i = 0; i < OldSize; ++i)
                {
                    OldNamesPtr[i].~FName();
                }
                FMemory::Free(OldNamesPtr);
                FMemory::Free(NameData.GetMutableValues());
            }

            // Set tagged pointers with dynamic flag
            NameData.TaggedNames = reinterpret_cast<uintptr_t>(NewNames) | FNameData::TagDynamic;
            NameData.TaggedValues = reinterpret_cast<uintptr_t>(NewValues) | FNameData::TagDynamic;
            NameData.NumValues = NewSize;

            return Index;
        }
    }
    
}

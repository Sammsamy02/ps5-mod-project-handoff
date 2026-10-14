#if defined(__PROSPERO__)
#include <thread>

#include <DynamicOutput/Output.hpp>

#include <Unreal/UAssetRegistry.hpp>
#include <Unreal/UAssetRegistryHelpers.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Hooks.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <Unreal/NativeAssetLoadingMemory.hpp>

namespace RC::Unreal
{
    std::atomic_bool UAssetRegistry::StaticStorage::bAssetsAreLoading{false};
    std::atomic_bool UAssetRegistry::StaticStorage::bShouldLoadAllAssets{false};
    std::vector<FUObjectItem*> UAssetRegistry::StaticStorage::ForcefullyLoadedAssets{};
    uint64 UAssetRegistry::StaticStorage::MaxMemoryUsageDuringAssetLoading{88};

    auto UAssetRegistry::GetAllAssets(TArray <FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets) -> bool
    {
        if (!Functions::GetAllAssets.IsValid()) { return false; }

        GetAllAssets_Params Params{OutAssetData, bIncludeOnlyOnDiskAssets};
        Functions::GetAllAssets(this, Params);

        // Copy here because we can't actually make the variable in the params struct a reference
        // This is because UE4 expects the TArray to be inlined into the struct
        // This may be a design problem with UE4SS, but it would be annoying to have to construct the params variable outside this wrapper function
        // With TArray it's not super slow because we can simply copy the pointer and the num/max integers
        // We don't own the data pointer, so it won't get deallocated by doing this
        OutAssetData.CopyFast(Params.OutAssetData);
        return Params.ReturnValue;
    }

    FAssetData UAssetRegistry::GetAssetByObjectPath(const FName ObjectPath, bool bIncludeOnlyOnDiskAssets)
    {
        if (!Functions::GetAssetByObjectPath.IsValid()) { return FAssetData{}; }

        GetAssetByObjectPath_Params Params{ObjectPath, bIncludeOnlyOnDiskAssets};
        Functions::GetAssetByObjectPath(this, Params);

        return Params.ReturnValue;
    }

    bool UAssetRegistry::GetAssetsByClass(FName ClassName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses)
    {
        if (!Functions::GetAssetsByClass.IsValid()) { return false; }

        GetAssetsByClass_Params Params{ClassName, OutAssetData, bSearchSubClasses};
        Functions::GetAssetsByClass(this, Params);

        OutAssetData.CopyFast(Params.OutAssetData);
        return Params.ReturnValue;
    }

    static auto CheckAssetLoadingMemoryBudget(uint64 maximum_percentage) -> bool
    {
        const auto check = NativeAssetLoadingMemory::CheckBudget(maximum_percentage);
        if (NativeAssetLoadingMemory::MayLoad(check)) { return true; }
        if (check == NativeAssetLoadingMemory::BudgetCheck::InvalidPercentage)
        {
            Output::send<LogLevel::Error>(STR("Did not load assets: memory-pressure budget {} is outside 0..100 percent.\n"), maximum_percentage);
        }
        else
        {
            Output::send<LogLevel::Error>(STR("Did not load assets: native system memory-pressure and current process working-set monitoring are unavailable.\n"));
        }
        return false;
    }

    auto UAssetRegistry::LoadAllAssetsThread() -> void
    {
        // Reject an unsupported budget before touching the asset registry.
        if (!CheckAssetLoadingMemoryBudget(StaticStorage::MaxMemoryUsageDuringAssetLoading)) { return; }
        SetAssetsAreLoading(true);

        UAssetRegistry* AssetRegistry = static_cast<UAssetRegistry*>(UAssetRegistryHelpers::GetAssetRegistry().ObjectPointer);
        if (!AssetRegistry)
        {
            Output::send(STR("Did not load assets because asset_registry was nullptr\n"));
            SetAssetsAreLoading(false);
            return;
        }

        TArray<FAssetData> AllAssets{nullptr, 0, 0};
        AssetRegistry->GetAllAssets(AllAssets, false);

        auto* AssetDataScriptStruct = UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr, nullptr, STR("/Script/CoreUObject.AssetData"));
        if (!AssetDataScriptStruct)
        {
            AssetDataScriptStruct = UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr, nullptr, STR("/Script/AssetRegistry.AssetData"));
        }

        if (!AssetDataScriptStruct)
        {
            Output::send(STR("Did not load assets because could not get size of AssetData ScriptStruct because the pointer was nullptr\n"));
        }

        for (FAssetData& Asset : AllAssets)
        {
            // Recheck in case configuration changed while assets were being loaded.
            if (!CheckAssetLoadingMemoryBudget(StaticStorage::MaxMemoryUsageDuringAssetLoading)) { break; }

            UObject* LoadedAsset = UAssetRegistryHelpers::GetAsset(Asset);
            if (!LoadedAsset) { continue; }

            if (auto* ObjectItem = LoadedAsset->GetObjectItem(); ObjectItem)
            {
                StaticStorage::ForcefullyLoadedAssets.emplace_back(ObjectItem);
                // TODO: Undo this, it's very important or we leak multiple gigabytes of memory
                ObjectItem->SetRootSet();
                ObjectItem->SetGCKeep();
            }
        }

        SetAssetsAreLoading(false);
    }

    auto UAssetRegistry::FreeAllForcefullyLoadedAssets() -> void
    {
        std::erase_if(StaticStorage::ForcefullyLoadedAssets, [&](FUObjectItem* ObjectItem) -> bool {
            ObjectItem->UnsetRootSet();
            ObjectItem->UnsetGCKeep();
            return true;
        });
    }

    auto UAssetRegistry::LoadAllAssets() -> void
    {
        // Do not register a tick callback or spin-wait for an unavailable monitor.
        if (!CheckAssetLoadingMemoryBudget(StaticStorage::MaxMemoryUsageDuringAssetLoading)) { return; }

        // The load is performed in an EngineTick callback which the spin-wait below waits on. A caller
        // already on the game thread would be waiting for a tick that can never fire, so load directly.
        if (IsInGameThreadRaw())
        {
            LoadAllAssetsThread();
            return;
        }

        if (!AssetsAreLoading())
        {
            SetShouldLoadAllAssets(true);
            Hook::RegisterEngineTickPostCallback([](Hook::TCallbackIterationData<void>& info, ...) {
                LoadAllAssetsThread();

                // Loading assets is an asynchronous operation
                // It's very important to not reset this bool until after the assets have been loaded
                // This is what keeps the load-requester waiting until all the assets have been loaded
                SetShouldLoadAllAssets(false);
            }, {true, true, STR("UE4SS"), STR("AssetLoaderHook")});
        }

        // Wait for all assets to load
        while (ShouldLoadAllAssets())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

#else
#include <thread>

#include <DynamicOutput/DynamicOutput.hpp>

#include <Unreal/UAssetRegistry.hpp>
#include <Unreal/UAssetRegistryHelpers.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/Core/Windows/WindowsHWrapper.hpp>
#include <Unreal/Hooks.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <psapi.h>

namespace RC::Unreal
{
    std::atomic_bool UAssetRegistry::StaticStorage::bAssetsAreLoading{false};
    std::atomic_bool UAssetRegistry::StaticStorage::bShouldLoadAllAssets{false};
    std::vector<FUObjectItem*> UAssetRegistry::StaticStorage::ForcefullyLoadedAssets{};
    uint64 UAssetRegistry::StaticStorage::MaxMemoryUsageDuringAssetLoading{88};

    auto UAssetRegistry::GetAllAssets(TArray <FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets) -> bool
    {
        if (!Functions::GetAllAssets.IsValid()) { return false; }

        GetAllAssets_Params Params{OutAssetData, bIncludeOnlyOnDiskAssets};
        Functions::GetAllAssets(this, Params);

        // Copy here because we can't actually make the variable in the params struct a reference
        // This is because UE4 expects the TArray to be inlined into the struct
        // This may be a design problem with UE4SS, but it would be annoying to have to construct the params variable outside this wrapper function
        // With TArray it's not super slow because we can simply copy the pointer and the num/max integers
        // We don't own the data pointer, so it won't get deallocated by doing this
        OutAssetData.CopyFast(Params.OutAssetData);
        return Params.ReturnValue;
    }

    FAssetData UAssetRegistry::GetAssetByObjectPath(const FName ObjectPath, bool bIncludeOnlyOnDiskAssets)
    {
        if (!Functions::GetAssetByObjectPath.IsValid()) { return FAssetData{}; }

        GetAssetByObjectPath_Params Params{ObjectPath, bIncludeOnlyOnDiskAssets};
        Functions::GetAssetByObjectPath(this, Params);

        return Params.ReturnValue;
    }

    bool UAssetRegistry::GetAssetsByClass(FName ClassName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses)
    {
        if (!Functions::GetAssetsByClass.IsValid()) { return false; }

        GetAssetsByClass_Params Params{ClassName, OutAssetData, bSearchSubClasses};
        Functions::GetAssetsByClass(this, Params);

        OutAssetData.CopyFast(Params.OutAssetData);
        return Params.ReturnValue;
    }

    static size_t GetComputerTotalMemory()
    {
        MEMORYSTATUSEX StateX;
        StateX.dwLength = sizeof(StateX);
        GlobalMemoryStatusEx(&StateX);
        return StateX.dwMemoryLoad;
    }

    auto UAssetRegistry::LoadAllAssetsThread() -> void
    {
        SetAssetsAreLoading(true);

        UAssetRegistry* AssetRegistry = static_cast<UAssetRegistry*>(UAssetRegistryHelpers::GetAssetRegistry().ObjectPointer);
        if (!AssetRegistry)
        {
            Output::send(STR("Did not load assets because asset_registry was nullptr\n"));
            SetAssetsAreLoading(false);
            return;
        }

        TArray<FAssetData> AllAssets{nullptr, 0, 0};
        AssetRegistry->GetAllAssets(AllAssets, false);

        auto* AssetDataScriptStruct = UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr, nullptr, STR("/Script/CoreUObject.AssetData"));
        if (!AssetDataScriptStruct)
        {
            AssetDataScriptStruct = UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr, nullptr, STR("/Script/AssetRegistry.AssetData"));
        }

        if (!AssetDataScriptStruct)
        {
            Output::send(STR("Did not load assets because could not get size of AssetData ScriptStruct because the pointer was nullptr\n"));
        }

        for (FAssetData& Asset : AllAssets)
        {
            auto ComputerTotalMemory = GetComputerTotalMemory();
            if (StaticStorage::MaxMemoryUsageDuringAssetLoading > 0 && ComputerTotalMemory >= StaticStorage::MaxMemoryUsageDuringAssetLoading)
            {
                static constexpr size_t megabyte = 1000000;
                size_t MemUsageInMegabytes{};
                PROCESS_MEMORY_COUNTERS MemoryCounters{};
                if (GetProcessMemoryInfo(GetCurrentProcess(), &MemoryCounters, sizeof(MemoryCounters)))
                {
                    MemUsageInMegabytes = MemoryCounters.WorkingSetSize / megabyte;
                }

                Output::send<LogLevel::Error>(STR("Ran out of memory while trying to load all assets (process usage on failure: {} megabytes)."), MemUsageInMegabytes);
                break;
            }

            UObject* LoadedAsset = UAssetRegistryHelpers::GetAsset(Asset);
            if (!LoadedAsset) { continue; }

            if (auto* ObjectItem = LoadedAsset->GetObjectItem(); ObjectItem)
            {
                StaticStorage::ForcefullyLoadedAssets.emplace_back(ObjectItem);
                // TODO: Undo this, it's very important or we leak multiple gigabytes of memory
                ObjectItem->SetRootSet();
                ObjectItem->SetGCKeep();
            }
        }

        SetAssetsAreLoading(false);
    }

    auto UAssetRegistry::FreeAllForcefullyLoadedAssets() -> void
    {
        std::erase_if(StaticStorage::ForcefullyLoadedAssets, [&](FUObjectItem* ObjectItem) -> bool {
            ObjectItem->UnsetRootSet();
            ObjectItem->UnsetGCKeep();
            return true;
        });
    }

    auto UAssetRegistry::LoadAllAssets() -> void
    {
        // The load is performed in an EngineTick callback which the spin-wait below waits on. A caller
        // already on the game thread would be waiting for a tick that can never fire, so load directly.
        if (IsInGameThreadRaw())
        {
            LoadAllAssetsThread();
            return;
        }

        if (!AssetsAreLoading())
        {
            SetShouldLoadAllAssets(true);
            Hook::RegisterEngineTickPostCallback([](Hook::TCallbackIterationData<void>& info, ...) {
                LoadAllAssetsThread();

                // Loading assets is an asynchronous operation
                // It's very important to not reset this bool until after the assets have been loaded
                // This is what keeps the load-requester waiting until all the assets have been loaded
                SetShouldLoadAllAssets(false);
            }, {true, true, STR("UE4SS"), STR("AssetLoaderHook")});
        }

        // Wait for all assets to load
        while (ShouldLoadAllAssets())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

#endif

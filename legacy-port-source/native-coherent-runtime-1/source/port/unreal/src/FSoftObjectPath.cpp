#include <algorithm>
#include <bit>
#include <cstring>

#include <Unreal/FSoftObjectPath.hpp>
#include <Unreal/PackageName.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/UAssetRegistryHelpers.hpp>
#include <Unreal/UAssetRegistry.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <DynamicOutput/Output.hpp>
namespace RC::Unreal
{
        auto FSoftObjectPath::GetLayout() -> const Layout&
        {
            static Layout Cached = []() -> Layout {
                Layout L{};
                L.NameSize = static_cast<int32>(sizeof(FName));
                try
                {
                    L.NameSize = FName::StaticSize();
                }
                catch (...)
                {
                    // Runtime FName size not resolved yet; compile-time size is the fallback
                }

                // Live reflection is the ground truth for this game, including custom engines
                // that backported the FTopLevelAssetPath change
                auto* struct_object = UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.SoftObjectPath"));
                if (struct_object)
                {
                    auto* script_struct = std::bit_cast<UScriptStruct*>(struct_object);
                    auto* sub_path_property = script_struct->GetPropertyByName(STR("SubPathString"));
                    if (sub_path_property)
                    {
                        L.TotalSize = script_struct->GetSize();
                        L.SubPathOffset = sub_path_property->GetOffset_Internal();
                        L.bUsesTopLevelAssetPath = script_struct->GetPropertyByName(STR("AssetPath")) != nullptr;
                        L.bResolvedFromReflection = true;

                        if (L.TotalSize > MaxStorageSize)
                        {
                            Output::send<LogLevel::Warning>(STR("FSoftObjectPath(1): engine size 0x{:X} exceeds storage 0x{:X}; clamping\n"),
                                                            L.TotalSize,
                                                            MaxStorageSize);
                            L.TotalSize = MaxStorageSize;
                        }
                        return L;
                    }
                }

                // Fallback: derive from FName size and engine version
                L.bUsesTopLevelAssetPath = Version::IsAtLeast(5, 1);
                const int32 asset_path_bytes = L.bUsesTopLevelAssetPath ? L.NameSize * 2 : L.NameSize;
                L.SubPathOffset = (asset_path_bytes + 7) & ~7;
                L.TotalSize = L.SubPathOffset + static_cast<int32>(sizeof(FString));
                if (L.TotalSize > MaxStorageSize)
                {
                    Output::send<LogLevel::Warning>(STR("FSoftObjectPath(2): engine size 0x{:X} exceeds storage 0x{:X}; clamping\n"),
                                                    L.TotalSize,
                                                    MaxStorageSize);
                    L.TotalSize = MaxStorageSize;
                }
                return L;
            }();
            return Cached;
        }

        auto FSoftObjectPath::StaticSize() -> int32
        {
            return GetLayout().TotalSize;
        }

        auto FSoftObjectPath::SubPathRef() -> FString&
        {
            return *std::bit_cast<FString*>(&Storage[GetLayout().SubPathOffset]);
        }

        auto FSoftObjectPath::SubPathRef() const -> const FString&
        {
            return *std::bit_cast<const FString*>(&Storage[GetLayout().SubPathOffset]);
        }

        auto FSoftObjectPath::ReadNameAt(int32 ByteOffset) const -> FName
        {
            FName Result{};
            std::memcpy(&Result, &Storage[ByteOffset], std::min(static_cast<int32>(sizeof(FName)), GetLayout().NameSize));
            return Result;
        }

        auto FSoftObjectPath::WriteNameAt(int32 ByteOffset, const FName& Name) -> void
        {
            const auto& L = GetLayout();
            std::memset(&Storage[ByteOffset], 0, L.NameSize);
            std::memcpy(&Storage[ByteOffset], &Name, std::min(static_cast<int32>(sizeof(FName)), L.NameSize));
        }

        FSoftObjectPath::FSoftObjectPath(const FSoftObjectPath& Other)
        {
            std::memcpy(Storage, Other.Storage, GetLayout().SubPathOffset);
            SubPathRef() = Other.SubPathRef();
        }

        FSoftObjectPath& FSoftObjectPath::operator=(const FSoftObjectPath& Other)
        {
            if (this == &Other)
            {
                return *this;
            }
            std::memcpy(Storage, Other.Storage, GetLayout().SubPathOffset);
            SubPathRef() = Other.SubPathRef();
            return *this;
        }

        FSoftObjectPath::FSoftObjectPath(FSoftObjectPath&& Other) noexcept
        {
            std::memcpy(Storage, Other.Storage, StaticSize());
            std::memset(Other.Storage, 0, StaticSize());
        }

        FSoftObjectPath& FSoftObjectPath::operator=(FSoftObjectPath&& Other) noexcept
        {
            if (this == &Other)
            {
                return *this;
            }
            SubPathRef().~FString();
            std::memcpy(Storage, Other.Storage, StaticSize());
            std::memset(Other.Storage, 0, StaticSize());
            return *this;
        }

        FSoftObjectPath::~FSoftObjectPath()
        {
            SubPathRef().~FString();
            std::memset(Storage, 0, StaticSize());
        }

        FSoftObjectPath::FSoftObjectPath(const UObject* InObject)
        {
            if (InObject)
            {
                SetPath(FString(InObject->GetPathName().c_str()));
            }
        }

        bool FSoftObjectPath::IsNull() const
        {
            const auto& L = GetLayout();
            if (!L.bUsesTopLevelAssetPath)
            {
                return ReadNameAt(0).IsNone();
            }
            return ReadNameAt(0).IsNone() && ReadNameAt(L.NameSize).IsNone();
        }

        FName FSoftObjectPath::GetAssetPathName() const
        {
            const auto& L = GetLayout();
            if (!L.bUsesTopLevelAssetPath)
            {
                return ReadNameAt(0);
            }
            if (IsNull())
            {
                return FName();
            }
            // 5.1+ has no single-FName representation; create one on demand
            return FName(*AssetPathToFString(), FNAME_Add);
        }

        FTopLevelAssetPath FSoftObjectPath::GetAssetPath() const
        {
            const auto& L = GetLayout();
            if (L.bUsesTopLevelAssetPath)
            {
                return FTopLevelAssetPath{ReadNameAt(0), ReadNameAt(L.NameSize)};
            }

            // Pre-5.1: split the single FName into package and asset parts
            FName AssetPathName = ReadNameAt(0);
            if (AssetPathName.IsNone())
            {
                return FTopLevelAssetPath{};
            }
            FString Path = AssetPathName.ToFString();
            int32 DotIndex;
            if (Path.FindChar('.', DotIndex))
            {
                return FTopLevelAssetPath{FName(*Path.Left(DotIndex)), FName(*Path.Mid(DotIndex + 1))};
            }
            return FTopLevelAssetPath{FName(*Path), FName()};
        }

        auto FSoftObjectPath::AssetPathToFString() const -> FString
        {
            const auto& L = GetLayout();
            if (!L.bUsesTopLevelAssetPath)
            {
                FName AssetPathName = ReadNameAt(0);
                if (AssetPathName.IsNone())
                {
                    return FString();
                }
                return AssetPathName.ToFString();
            }

            FName PackageName = ReadNameAt(0);
            FName AssetName = ReadNameAt(L.NameSize);
            if (PackageName.IsNone())
            {
                return FString();
            }
            FString Result = PackageName.ToFString();
            if (!AssetName.IsNone())
            {
                Result += '.';
                Result += AssetName.ToFString();
            }
            return Result;
        }

        FString FSoftObjectPath::GetAssetPathString() const
        {
            return AssetPathToFString();
        }

        FString FSoftObjectPath::ToString() const
        {
            if (SubPathRef().IsEmpty())
            {
                return AssetPathToFString();
            }
            auto AssetPathString = AssetPathToFString();
            FString FullPathString;
            FullPathString.Reserve(AssetPathString.Len() + SubPathRef().Len() + 1);
            FullPathString += AssetPathString;
            FullPathString += ':';
            FullPathString += SubPathRef();
            return FullPathString;
        }

        void FSoftObjectPath::Reset()
        {
            const auto& L = GetLayout();
            std::memset(Storage, 0, L.SubPathOffset);
            SubPathRef().Reset();
        }

        void FSoftObjectPath::SetPath(const FString& Path)
        {
            if (Path.IsEmpty() || Path == STR("None"))
            {
                Reset();
            }else if (ensureMsgf(!FPackageName::IsShortPackageName(*Path), TEXT("Cannot create SoftObjectPath with short package names"), Path.Len(), *Path))
            {
                // RE-UE4SS FIX (Corporalwill): [ExportTextPathToObjectPath unimplemented, would require non-array FString:Split() to be implemented]
                /*if (Path[0] != '/')
                {
                    Path = FPackageName::ExportTextPathToObjectPath(Path);
                }*/
                // RE-UE4SS FIX END

                const auto& L = GetLayout();

                FString AssetPathPart;
                int32 ColonIndex;
                if (Path.FindChar(':', ColonIndex))
                {
                    AssetPathPart = Path.Left(ColonIndex);
                    SubPathRef() = Path.Mid(ColonIndex + 1);
                }
                else
                {
                    AssetPathPart = Path;
                    SubPathRef().Empty();
                }

                if (!L.bUsesTopLevelAssetPath)
                {
                    WriteNameAt(0, FName(*AssetPathPart));
                }
                else
                {
                    int32 DotIndex;
                    if (AssetPathPart.FindChar('.', DotIndex))
                    {
                        WriteNameAt(0, FName(*AssetPathPart.Left(DotIndex)));
                        WriteNameAt(L.NameSize, FName(*AssetPathPart.Mid(DotIndex + 1)));
                    }
                    else
                    {
                        WriteNameAt(0, FName(*AssetPathPart));
                        WriteNameAt(L.NameSize, FName());
                    }
                }
            }
        }

        UObject* FSoftObjectPath::ResolveObject() const
        {
            if (IsNull())
            {
                return nullptr;
            }
            return ResolveObjectInternal();
        }

        UObject* FSoftObjectPath::ResolveObjectInternal() const
        {
            if (SubPathRef().IsEmpty())
            {
                return ResolveObjectInternal(*AssetPathToFString());
            }
            else
            {
                return  ResolveObjectInternal(*ToString());
            }
        }

        UObject* FSoftObjectPath::ResolveObjectInternal(const TCHAR* PathString) const
        {
            return UObjectGlobals::FindObject<Unreal::UObject>(nullptr, PathString);
        }

        int32 FSoftObjectPath::GetCurrentTag()
        {
            //TODO: Find way to access tag value
            return 0;
        }

        UObject* FSoftObjectPath::TryLoad(/*FUObjectSerializeContext* InLoadContext = nullptr*/) const
        {

            // RE-UE4SS FIX (Corporalwill): [StaticLoadObject not implemented, reusing code from UE4SS lua function LoadAsset]
            //                              Perhaps we should eventually implement StaticLoadObject and just wrap this code ?
            if (!Unreal::IsInGameThread())
            {
                throw std::runtime_error{"FSoftObjectPath::TryLoad can only be called from within the game thread"};
            }

            auto asset_path_and_name = FName(*this->ToString(), EFindName::FNAME_Add);

            auto* asset_registry = static_cast<UAssetRegistry*>(UAssetRegistryHelpers::GetAssetRegistry().ObjectPointer);
            if (!asset_registry)
            {
                throw std::runtime_error{"FSoftObjectPath::TryLoad did not load assets because asset_registry was nullptr\n"};
            }

            Unreal::UObject* loaded_asset{};

            Unreal::FAssetData asset_data = asset_registry->GetAssetByObjectPath(asset_path_and_name);
            if ((Unreal::Version::IsAtMost(5, 0) && asset_data.ObjectPath().GetComparisonIndex()) || asset_data.PackageName().GetComparisonIndex())
            {
                loaded_asset = Unreal::UAssetRegistryHelpers::GetAsset(asset_data);
            }
            // RE-UE4SS FIX END

            return loaded_asset;
        }

        FSoftObjectPath FSoftObjectPath::GetOrCreateIDForObject(const UObject* Object)
        {
            return FSoftObjectPath(Object);
        }
}

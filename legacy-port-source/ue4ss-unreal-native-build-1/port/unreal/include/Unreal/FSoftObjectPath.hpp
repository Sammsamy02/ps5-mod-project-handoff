#pragma once
#include <Unreal/FString.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/CoreUObject/UObject/TopLevelAssetPath.hpp>
namespace RC::Unreal
{
    // The engine layout of FSoftObjectPath varies by version:
    //   4.18 to 5.0:  FName AssetPathName;      FString SubPathString;
    //   5.1+:         FTopLevelAssetPath AssetPath (two FNames); FString SubPathString;
    // Storage below covers the largest known layout. The active layout is resolved at runtime,
    // preferring live reflection (/Script/CoreUObject.SoftObjectPath) over version rules, so
    // modified engines that backported or diverged behave correctly with no compile-time
    // branching. Accessors read and write through the resolved layout.
    struct RC_UE_API FSoftObjectPath
    {
        struct Layout
        {
            int32 TotalSize{};
            int32 SubPathOffset{};
            int32 NameSize{};
            bool bUsesTopLevelAssetPath{};
            bool bResolvedFromReflection{};
        };

        static constexpr int32 MaxStorageSize = 0x28;

        // Resolved once on first use; TypeChecker triggers resolution during init so the
        // reflection-based path wins in practice
        static auto GetLayout() -> const Layout&;

        // Size of the engine's FSoftObjectPath; usable for container striding
        static auto StaticSize() -> int32;

      private:
        alignas(8) uint8 Storage[MaxStorageSize]{};

        auto SubPathRef() -> FString&;
        auto SubPathRef() const -> const FString&;
        auto ReadNameAt(int32 ByteOffset) const -> FName;
        auto WriteNameAt(int32 ByteOffset, const FName& Name) -> void;
        // The asset part (everything before the optional ':') as a string, layout-aware
        auto AssetPathToFString() const -> FString;

      public:
        FSoftObjectPath()
        {
        }

        /** Construct from a path string. Non-explicit for backwards compatibility. */
        FSoftObjectPath(const FString& Path)
        {
            SetPath(Path);
        }

        explicit FSoftObjectPath(TYPE_OF_NULLPTR)
        {
        }

        /** Construct from an existing object in memory */
        FSoftObjectPath(const UObject* InObject);

        FSoftObjectPath(const FSoftObjectPath& Other);
        FSoftObjectPath& operator=(const FSoftObjectPath& Other);
        FSoftObjectPath(FSoftObjectPath&& Other) noexcept;
        FSoftObjectPath& operator=(FSoftObjectPath&& Other) noexcept;
        ~FSoftObjectPath();

        FSoftObjectPath& operator=(const FString& Path)
        {
            SetPath(Path);
            return *this;
        }

        /** Returns string representation of reference, in form /package/path.assetname[:subpath] */
        FString ToString() const;

        /** Returns the entire asset path as an FName, including both package and asset but not sub object.
            On 5.1+ layouts the FName is created on demand from the stored FTopLevelAssetPath. */
        FName GetAssetPathName() const;

        /** Returns the asset path as package and asset names. Synthesized by splitting on pre-5.1 layouts. */
        FTopLevelAssetPath GetAssetPath() const;

        /** Returns string version of asset path, including both package and asset but not sub object */
        FString GetAssetPathString() const;

        /** Returns the sub path, which is often empty */
        const FString& GetSubPathString() const
        {
            return SubPathRef();
        }

        FString& GetSubPathString()
        {
            return SubPathRef();
        }

        /** Sets asset path of this reference based on a string path */
        void SetPath(const FString& Path);

        /**
        * Attempts to load the asset, this will call LoadObject which can be very slow
        * @return Loaded UObject, or nullptr if the reference is null or the asset fails to load
        */
        UObject* TryLoad(/*FUObjectSerializeContext* InLoadContext = nullptr*/) const;

        /**
        * Attempts to find a currently loaded object that matches this path
        *
        * @return Found UObject, or nullptr if not currently in memory
        */
        UObject* ResolveObject() const;

        /** Resets reference to point to null */
        void Reset();

        /** Check if this could possibly refer to a real object, or was initialized to null */
        bool IsValid() const
        {
            return !IsNull();
        }

        /** Checks to see if this is initialized to null */
        bool IsNull() const;

        /** Check if this represents an asset, meaning it is not null but does not have a sub path */
        bool IsAsset() const
        {
            return !IsNull() && SubPathRef().IsEmpty();
        }

        /** Check if this represents a sub object, meaning it has a sub path */
        bool IsSubobject() const
        {
            return !IsNull() && !SubPathRef().IsEmpty();
        }

        /** Code needed by FSoftObjectPtr internals */
        static int32 GetCurrentTag();
        static FSoftObjectPath GetOrCreateIDForObject(const UObject* Object);

      private:
        UObject* ResolveObjectInternal() const;
        UObject* ResolveObjectInternal(const TCHAR* PathString) const;
    };
}

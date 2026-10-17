#include <bit>
#include <cstring>
#include <stdexcept>

#include <Unreal/FText.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/FProperty.hpp>
#include <Unreal/UnrealVersion.hpp>

namespace RC::Unreal
{
    int32 FText::StaticSize_Private{-1};
    FProperty* FText::ValueOpsProperty{nullptr};

    int32 FText::StaticSize()
    {
        if (StaticSize_Private < 0)
        {
            throw std::runtime_error{"[FText::StaticSize] StaticSize_Private is not valid"};
        }
        else
        {
            return StaticSize_Private;
        }
    }

    auto FText::ActiveSize() -> int32
    {
        if (StaticSize_Private > 0 && StaticSize_Private <= MaxStorageSize)
        {
            return StaticSize_Private;
        }
        return MaxStorageSize;
    }

    // The engine's value operations construct/copy/destroy a full-size engine FText in place,
    // so they may only be used when the engine's FText actually fits in our storage. They also
    // require a valid source: a zero-initialized FText is not a valid engine FText (its shared
    // reference is null), and the engine's copy path does not expect one.
    auto FText::CanUseValueOps() -> bool
    {
        return ValueOpsProperty && StaticSize_Private > 0 && StaticSize_Private <= MaxStorageSize;
    }

    auto FText::GetTextData() const -> ITextData*
    {
        return *std::bit_cast<ITextData* const*>(&Storage[0]);
    }

    auto FText::SetTextData(ITextData* TextData) -> void
    {
        *std::bit_cast<ITextData**>(&Storage[0]) = TextData;
    }

    auto FText::GetFlags() const -> uint32
    {
        // Flags are the last 4-byte field, 8 bytes before the end in every known layout
        // (0x20 in the 0x28 layout, 0x10 in the 0x18 layout, 0x8 in the 0x10 layout)
        return *std::bit_cast<const uint32*>(&Storage[ActiveSize() - 8]);
    }

    FText::FText(const FText& Other)
    {
        if (CanUseValueOps() && Other.GetTextData())
        {
            // Engine-correct copy: takes a reference using whatever refcounting scheme this version has.
            // The zero-initialized destination is safe for the engine's assignment in every era.
            ValueOpsProperty->CopyCompleteValue(Storage, Other.Storage);
        }
        else
        {
            // Empty source, or the property system isn't usable: borrowed bitwise copy
            std::memcpy(Storage, Other.Storage, ActiveSize());
        }
    }

    FText& FText::operator=(const FText& Other)
    {
        if (this == &Other)
        {
            return *this;
        }

        if (CanUseValueOps() && Other.GetTextData())
        {
            ValueOpsProperty->CopyCompleteValue(Storage, Other.Storage);
        }
        else
        {
            if (CanUseValueOps() && GetTextData())
            {
                ValueOpsProperty->DestroyValue(Storage);
            }
            std::memset(Storage, 0, ActiveSize());
            std::memcpy(Storage, Other.Storage, ActiveSize());
        }
        return *this;
    }

    FText::FText(FText&& Other) noexcept
    {
        std::memcpy(Storage, Other.Storage, ActiveSize());
        std::memset(Other.Storage, 0, ActiveSize());
    }

    FText& FText::operator=(FText&& Other) noexcept
    {
        if (this == &Other)
        {
            return *this;
        }

        if (CanUseValueOps() && GetTextData())
        {
            ValueOpsProperty->DestroyValue(Storage);
        }
        std::memcpy(Storage, Other.Storage, ActiveSize());
        std::memset(Other.Storage, 0, ActiveSize());
        return *this;
    }

    FText::~FText()
    {
        // Objects borrow-copied before the ops were resolved are also released here; that window
        // closes before any engine data exists, so in practice everything with live data owns it
        if (CanUseValueOps() && GetTextData())
        {
            ValueOpsProperty->DestroyValue(Storage);
            std::memset(Storage, 0, ActiveSize());
        }
    }

    auto FText::StealFromBuffer(void* SourceBuffer) -> FText
    {
        FText Result{};
        const auto Size = ActiveSize();
        std::memcpy(Result.Storage, SourceBuffer, Size);
        std::memset(SourceBuffer, 0, Size);
        return Result;
    }

    auto FText::CopyBorrowedTo(void* DestBuffer) const -> void
    {
        std::memcpy(DestBuffer, Storage, ActiveSize());
    }

    auto FText::construct_with_string(const FString&& String) -> void
    {
        *this = UKismetTextLibrary::Conv_StringToText(std::forward<const FString>(String));
    }

    FText::FText(const CharType* String)
    {
        FString Str = FString(FromCharTypePtr<TCHAR>(String));
        construct_with_string(std::forward<const FString>(Str));
    }

    auto FText::ToFString() const -> FString
    {
        if (!GetTextData())
        {
            return {};
        }
        return UKismetTextLibrary::Conv_TextToString(*this);
    }

    auto FText::ToString() const -> StringType
    {
        if (!GetTextData())
        {
            return {};
        }
        return StringType(*ToFString());
    }

    auto FText::SetString(const FString&& NewString) -> void
    {
        *this = FText(std::forward<const FString>(NewString));
    }
}

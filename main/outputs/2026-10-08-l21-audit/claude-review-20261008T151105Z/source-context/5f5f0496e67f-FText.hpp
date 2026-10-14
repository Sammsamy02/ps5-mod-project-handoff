#pragma once

#include <string>

#include <Function/Function.hpp>
#include <Unreal/Common.hpp>
#include <Unreal/Core/HAL/Platform.hpp>
#include <Unreal/ITextData.hpp>
#include <Unreal/UKismetTextLibrary.hpp>

namespace RC::Unreal
{
    class FString;
    class FProperty;

    // FText's engine layout varies by version: TSharedRef<ITextData> + flags (0x18 total)
    // before 5.4, TRefCountPtr<ITextData> + flags (0x10 total) from 5.4. Storage below covers
    // the largest known layout and StaticSize() decides how many bytes are live. Ownership
    // (copy/destroy) is routed through the engine's own FTextProperty value operations, so the
    // refcounting scheme of the running engine is used with no compile-time branching. Both
    // layouts store the ITextData pointer first, so reads need no version dispatch at all.
    class RC_UE_API FText
    {
    public:
        // These private members are public because 'friend' declarations don't work with concepts.
        // We use a concept in TypeChecker to determine whether these variables exist.
        static int32 StaticSize_Private;
        static int32 StaticSize();

        // FTextProperty used for engine-correct copy/destroy; set during TypeChecker initialization.
        // Until it is set, copies fall back to borrowed bitwise copies (the old behavior).
        static FProperty* ValueOpsProperty;

        // Largest known engine layout:
        //   4.7 to 4.10: TSharedRef<FString> DisplayString; TSharedPtr<FTextHistory> History; uint32 Flags  (0x28)
        //   4.11 to 5.3: TSharedRef<ITextData> TextData; uint32 Flags                                        (0x18)
        //   5.4+:        TRefCountPtr<ITextData> TextData; uint32 Flags                                      (0x10)
        // Storage must never be smaller than the running engine's FText, because the engine's
        // own value operations write StaticSize() bytes into it.
        static constexpr int32 MaxStorageSize = 0x30;

      private:
        alignas(8) uint8 Storage[MaxStorageSize]{};

        // Live extent of Storage: StaticSize when known, the full buffer otherwise
        static auto ActiveSize() -> int32;

        // Whether the engine's FText value operations are usable (resolved, and the engine's
        // FText fits in Storage so the engine cannot write past it)
        static auto CanUseValueOps() -> bool;

      private:
        auto construct_with_string(const FString&& String) -> void;

      public:
        FText() = default;

        explicit FText(const FString&& String)
        {
            construct_with_string(std::forward<const FString>(String));
        }

        explicit FText(const CharType* String);

        explicit FText(StringViewType String) : FText(String.data())
        {
        }

        FText(const FText& Other);
        FText& operator=(const FText& Other);
        FText(FText&& Other) noexcept;
        FText& operator=(FText&& Other) noexcept;
        ~FText();

        auto GetTextData() const -> ITextData*;
        // Raw pointer write with no refcounting; prefer assignment from another FText
        auto SetTextData(ITextData* TextData) -> void;
        auto GetFlags() const -> uint32;

        // Bitwise-takes ownership of an FText the engine wrote into a param or return buffer,
        // zeroing the buffer so nothing double-releases when the buffer is discarded
        static auto StealFromBuffer(void* SourceBuffer) -> FText;

        // Bitwise borrow into a param buffer for the duration of a call; no ownership moves
        auto CopyBorrowedTo(void* DestBuffer) const -> void;

        auto inline operator==(const FString& Other) const -> bool
        {
            return ToFString() == Other;
        }

        auto inline operator==(const CharType* Other) const -> bool
        {
            return StringType(*ToFString()) == Other;
        }

        // Beware: Unsafe to call 'ToFString' when there is no text data, call 'ToString' instead.
        auto ToFString() const -> FString;
        auto ToString() const -> StringType;

        auto SetString(const FString&&) -> void;
    };
}

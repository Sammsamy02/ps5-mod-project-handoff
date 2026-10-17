#include <Unreal/UKismetTextLibrary.hpp>
#include <Unreal/BPMacros.hpp>
#include <Unreal/FString.hpp>

namespace RC::Unreal
{
    FString UKismetTextLibrary::Conv_TextToString(const FText& InText)
    {
        UE_BEGIN_NATIVE_FUNCTION_BODY("/Script/Engine.KismetTextLibrary:Conv_TextToString")
        UE_SET_STATIC_SELF("/Script/Engine.Default__KismetTextLibrary")

        // Borrow the text into the param buffer for the duration of the call; the engine only
        // reads it, so no reference needs to be taken or released
        UE_COPY_PROPERTY_INTERNAL(InText)
        InText.CopyBorrowedTo(&ParamData[InTextProperty->GetOffset_Internal()]);

        UE_CALL_STATIC_FUNCTION()

        UE_RETURN_PROPERTY(FString)
    }

    FText UKismetTextLibrary::Conv_StringToText(const FString& InString)
    {
        UE_BEGIN_NATIVE_FUNCTION_BODY("/Script/Engine.KismetTextLibrary:Conv_StringToText")
        UE_SET_STATIC_SELF("/Script/Engine.Default__KismetTextLibrary")

        UE_COPY_PROPERTY(InString, FString)

        UE_CALL_STATIC_FUNCTION()

        // The engine constructed an owning FText into the return slot; take that reference over
        // instead of copying it, so freeing the param buffer cannot leak or double-release it
        UE_RETURN_PROPERTY_INTERNAL()
        auto RetValue = FText::StealFromBuffer(&ParamData[ReturnProperty->GetOffset_Internal()]);
        UE_FREE_PARAM_STRUCT_INTERNAL()
        return RetValue;
    }
}

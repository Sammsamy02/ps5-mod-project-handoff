#include <Unreal/UGameViewportClient.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>

namespace RC::Unreal
{
    IMPLEMENT_EXTERNAL_OBJECT_CLASS(UGameViewportClient);

    std::unordered_map<File::StringType, int32_t> UGameViewportClient::MemberOffsets{};
    std::unordered_map<File::StringType, BitfieldInfo> UGameViewportClient::BitfieldInfos{};

    Function<UGameViewportClient::TickSignature> UGameViewportClient::TickInternal;
}

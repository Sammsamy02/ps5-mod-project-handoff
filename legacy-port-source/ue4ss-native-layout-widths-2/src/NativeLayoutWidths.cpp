#include <NativeLayoutWidths.hpp>
#include <NativeLayoutEngineIncludes.hpp>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
namespace RC::Platform::NativeLayouts {
namespace {
using namespace RC::Unreal;
constexpr auto ConservativeWidth(std::initializer_list<std::optional<uint32_t>> alternatives) noexcept -> std::optional<uint32_t> {
 if(alternatives.size()==0) return std::nullopt;
 uint32_t width{};for(auto value:alternatives){if(!value)return std::nullopt;width=std::max(width,*value);}return width;
}
template<class T> constexpr auto Accessor0() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetActorCategory();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetActorCategory())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor1() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAnimUpdateRateFrameCount();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAnimUpdateRateFrameCount())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor2() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAnimUpdateRateShiftTag();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAnimUpdateRateShiftTag())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor3() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAttachmentReplication();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAttachmentReplication())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor4() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAutoReceiveInput();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAutoReceiveInput())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor5() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCachedLastRenderTime();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCachedLastRenderTime())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor6() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetChildren();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetChildren())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor7() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetControllingMatineeActors();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetControllingMatineeActors())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor8() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCreationTime();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCreationTime())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor9() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCustomTimeDilation();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCustomTimeDilation())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor10() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDefaultUpdateOverlapsMethodDuringLevelStreaming();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDefaultUpdateOverlapsMethodDuringLevelStreaming())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor11() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDetachFence();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDetachFence())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor12() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHLODLayer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHLODLayer())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor13() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHiddenEditorViews();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHiddenEditorViews())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint64>()});
}
template<class T> constexpr auto Accessor14() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInitialLifeSpan();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInitialLifeSpan())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor15() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInputComponent();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInputComponent())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor16() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInputConsumeOption_DEPRECATED();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInputConsumeOption_DEPRECATED())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor17() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInputPriority();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInputPriority())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor18() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastNetUpdateTime();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastNetUpdateTime())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor19() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastRenderTime505();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastRenderTime505())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor20() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastRenderTimeBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastRenderTimeBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor21() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLayers();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLayers())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor22() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMinNetUpdateFrequency();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMinNetUpdateFrequency())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor23() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetCullDistanceSquared();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetCullDistanceSquared())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor24() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetDormancy();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetDormancy())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor25() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetDriverName();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetDriverName())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor26() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetPriority();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetPriority())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor27() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetTag();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetTag())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor28() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetUpdateFrequency();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetUpdateFrequency())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor29() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetUpdateTime();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetUpdateTime())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor30() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnActorBeginOverlap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnActorBeginOverlap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor31() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnActorEndOverlap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnActorEndOverlap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor32() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnActorHit();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnActorHit())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor33() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnBeginCursorOver();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnBeginCursorOver())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor34() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnClicked();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnClicked())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor35() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnDestroyed();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnDestroyed())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor36() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnEndCursorOver();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnEndCursorOver())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor37() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnEndPlay();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnEndPlay())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor38() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnInputTouchBegin();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnInputTouchBegin())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor39() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnInputTouchEnd();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnInputTouchEnd())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor40() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnInputTouchEnter();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnInputTouchEnter())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor41() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnInputTouchLeave();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnInputTouchLeave())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor42() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnReleased();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnReleased())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor43() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnTakeAnyDamage();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnTakeAnyDamage())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor44() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnTakePointDamage();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnTakePointDamage())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor45() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnTakeRadialDamage();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnTakeRadialDamage())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor46() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOwner();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOwner())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor47() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetParentComponent();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetParentComponent())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor48() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetParentComponentActor408();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetParentComponentActor408())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor49() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetParentComponentActorBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetParentComponentActorBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor50() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetParentComponentActor_DEPRECATED();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetParentComponentActor_DEPRECATED())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor51() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPhysicsReplicationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPhysicsReplicationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor52() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPrimaryActorTick();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPrimaryActorTick())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor53() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRayTracingGroupId();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRayTracingGroupId())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor54() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRemoteRole();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRemoteRole())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor55() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetReplicatedComponentsInfo();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetReplicatedComponentsInfo())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor56() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetReplicatedMovement();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetReplicatedMovement())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor57() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetReplicatedSubObjects();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetReplicatedSubObjects())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor58() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRole();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRole())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor59() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRootComponent();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRootComponent())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor60() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSpawnCollisionHandlingMethod();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSpawnCollisionHandlingMethod())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor61() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor62() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTimerHandle_LifeSpanExpired();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTimerHandle_LifeSpanExpired())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor63() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUpdateOverlapsMethodDuringLevelStreaming();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUpdateOverlapsMethodDuringLevelStreaming())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor64() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentID();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentID())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor65() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDefaultPlayerName408();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDefaultPlayerName408())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor66() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDefaultPlayerNameBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDefaultPlayerNameBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor67() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEngineMessageClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEngineMessageClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor68() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameModeClassAliases();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameModeClassAliases())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor69() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameSession();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameSession())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor70() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHUDClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHUDClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor71() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInactivePlayerArray();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInactivePlayerArray())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor72() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInactivePlayerStateLifeSpan();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInactivePlayerStateLifeSpan())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor73() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMatchState();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMatchState())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor74() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMaxInactivePlayers();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMaxInactivePlayers())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor75() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMinRespawnDelay();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMinRespawnDelay())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor76() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumBots();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumBots())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor77() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumPlayers();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumPlayers())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor78() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumSpectators();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumSpectators())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor79() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumTravellingPlayers();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumTravellingPlayers())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor80() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOptionsString();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOptionsString())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor81() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPlayerStarts();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPlayerStarts())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor82() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPlayerStateClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPlayerStateClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor83() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSpectatorClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSpectatorClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor84() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTimerHandle_DefaultTimer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTimerHandle_DefaultTimer())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor85() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbHandleDedicatedServerReplays();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbHandleDedicatedServerReplays())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor86() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDefaultPlayerName();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDefaultPlayerName())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor87() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameNetDriverReplicationSystem();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameNetDriverReplicationSystem())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor88() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameSession();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameSession())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor89() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameSessionClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameSessionClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor90() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHUDClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHUDClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor91() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOptionsString();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOptionsString())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor92() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPlayerStateClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPlayerStateClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor93() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetServerStatReplicator();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetServerStatReplicator())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor94() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetServerStatReplicatorClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetServerStatReplicatorClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor95() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSpectatorClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSpectatorClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor96() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArCustomPropertyList();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArCustomPropertyList())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor97() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArEngineNetVer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArEngineNetVer())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor98() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArEngineVer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArEngineVer())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor99() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArGameNetVer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArGameNetVer())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor100() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArLicenseeUE4Ver();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArLicenseeUE4Ver())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor101() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArLicenseeUEVer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArLicenseeUEVer())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor102() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArMaxSerializeSize();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArMaxSerializeSize())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int64>()});
}
template<class T> constexpr auto Accessor103() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArPortFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArPortFlags())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor104() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArSerializingDefaults();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArSerializingDefaults())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor105() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArUE4Ver();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArUE4Ver())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor106() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArUEVer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArUEVer())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor107() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCookingTargetPlatform();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCookingTargetPlatform())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor108() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCustomVersionContainer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCustomVersionContainer())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor109() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNextProxy();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNextProxy())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor110() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSavePackageData();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSavePackageData())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor111() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSerializedProperty();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSerializedProperty())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor112() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbCustomVersionsAreReset();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbCustomVersionsAreReset())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor113() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArrayFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArrayFlags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor114() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInner();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInner())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor115() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetByteMask();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetByteMask())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor116() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetByteOffset();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetByteOffset())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor117() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFieldMask();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFieldMask())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor118() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFieldSize();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFieldSize())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor119() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEnum();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEnum())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor120() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMetaClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMetaClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor121() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSignatureFunction();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSignatureFunction())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor122() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEnum();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEnum())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor123() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUnderlyingProp();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUnderlyingProp())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor124() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassPrivate();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassPrivate())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<FFieldClass*>()});
}
template<class T> constexpr auto Accessor125() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFlagsPrivate();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFlagsPrivate())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor126() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNamePrivate();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNamePrivate())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor127() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNext();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNext())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor128() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOwner();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOwner())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor129() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCastFlags507();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCastFlags507())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor130() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCastFlagsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCastFlagsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint64>()});
}
template<class T> constexpr auto Accessor131() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassFlags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor132() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetConstructFn508();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetConstructFn508())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor133() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetConstructFnBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetConstructFnBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor134() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDefaultObject();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDefaultObject())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor135() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFName();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFName())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor136() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetId();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetId())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint64>()});
}
template<class T> constexpr auto Accessor137() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSuperClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSuperClass())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<FFieldClass*>()});
}
template<class T> constexpr auto Accessor138() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUniqueNameIndexCounter();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUniqueNameIndexCounter())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor139() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUnqiueNameIndexCounter();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUnqiueNameIndexCounter())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor140() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertyClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyClass())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<FFieldClass*>()});
}
template<class T> constexpr auto Accessor141() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInterfaceClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInterfaceClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor142() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetKeyProp();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetKeyProp())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor143() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMapFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMapFlags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor144() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMapLayout();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMapLayout())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor145() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetValueProp();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetValueProp())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor146() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSignatureFunction();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSignatureFunction())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor147() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertyClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor148() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbAutoEmitLineTerminator();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbAutoEmitLineTerminator())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor149() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbSuppressEventTag();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbSuppressEventTag())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor150() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetArrayDim();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetArrayDim())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor151() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDestructorLinkNext();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDestructorLinkNext())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor152() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetElementSize();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetElementSize())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor153() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNextRef();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNextRef())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor154() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOffset_Internal();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOffset_Internal())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor155() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPostConstructLinkNext();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPostConstructLinkNext())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor156() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertyFlags420();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyFlags420())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor157() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertyFlagsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyFlagsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint64>()});
}
template<class T> constexpr auto Accessor158() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertyLinkNext();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyLinkNext())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor159() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRepIndex();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRepIndex())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint16>()});
}
template<class T> constexpr auto Accessor160() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRepNotifyFunc();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRepNotifyFunc())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor161() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetElementProp();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetElementProp())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor162() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSetLayout507();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSetLayout507())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor163() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSetLayoutBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSetLayoutBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor164() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMetaClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMetaClass())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor165() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStruct();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStruct())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor166() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMasterSerialNumber();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMasterSerialNumber())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor167() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMaxObjectsNotConsideredByGC();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMaxObjectsNotConsideredByGC())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor168() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjAvailable();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjAvailable())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor169() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjAvailableList427();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjAvailableList427())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor170() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjAvailableListBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjAvailableListBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor171() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjAvailableListEstimateCount();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjAvailableListEstimateCount())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor172() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjFirstGCIndex();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjFirstGCIndex())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor173() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjLastNonGCIndex();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjLastNonGCIndex())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor174() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjObjects();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjObjects())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor175() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOpenForDisregardForGC411();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOpenForDisregardForGC411())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor176() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOpenForDisregardForGCBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOpenForDisregardForGCBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor177() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPrimarySerialNumber();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPrimarySerialNumber())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor178() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUObjectCreateListeners();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUObjectCreateListeners())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor179() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUObjectDeleteListeners();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUObjectDeleteListeners())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor180() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbShouldRecycleObjectIndices();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbShouldRecycleObjectIndices())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor181() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAllClassesVersion();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAllClassesVersion())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor182() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassToChildListMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassToChildListMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor183() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassToChildListMapVersion();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassToChildListMapVersion())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor184() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassToObjectListMap422();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassToObjectListMap422())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor185() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassToObjectListMapBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassToObjectListMapBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor186() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHash();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHash())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor187() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHashOuter500();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHashOuter500())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor188() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHashOuterBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHashOuterBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor189() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNativeClassesVersion();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNativeClassesVersion())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor190() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjectOuterMap416();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjectOuterMap416())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor191() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjectOuterMapBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjectOuterMapBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor192() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjectToPackageMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjectToPackageMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor193() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPackageToObjectListMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPackageToObjectListMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor194() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClusterAndFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClusterAndFlags())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor195() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClusterIndex();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClusterIndex())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor196() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClusterRootIndex();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClusterRootIndex())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor197() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFlags())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor198() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFlagsAndRefCount();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFlagsAndRefCount())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int64>()});
}
template<class T> constexpr auto Accessor199() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObject();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObject())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<UObjectBase*>()});
}
template<class T> constexpr auto Accessor200() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjectPtrLow();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjectPtrLow())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor201() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRefCount();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRefCount())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor202() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRemoteId();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRemoteId())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor203() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSerialNumber();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSerialNumber())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor204() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAudioDeviceHandle();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAudioDeviceHandle())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor205() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAudioDeviceID();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAudioDeviceID())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor206() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetContextHandle();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetContextHandle())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor207() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCustomDescription();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCustomDescription())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor208() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetExternalReferences();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetExternalReferences())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor209() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameViewport();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameViewport())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor210() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGarbageObjectsToVerify();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGarbageObjectsToVerify())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor211() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastRemoteURL();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastRemoteURL())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor212() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastURL();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastURL())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor213() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLevelsToLoadForPendingMapChange();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLevelsToLoadForPendingMapChange())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor214() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPIEAccumulatedTickSeconds();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPIEAccumulatedTickSeconds())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor215() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPIEFixedTickSeconds();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPIEFixedTickSeconds())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor216() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPIEInstance();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPIEInstance())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor217() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPIEPrefix();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPIEPrefix())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor218() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPIERemapPrefix();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPIERemapPrefix())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor219() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPendingMapChangeFailureDescription();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPendingMapChangeFailureDescription())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor220() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRunAsDedicated();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRunAsDedicated())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor221() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetThisCurrentWorld();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetThisCurrentWorld())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor222() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTravelType();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTravelType())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor223() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTravelURL();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTravelURL())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor224() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbIsPrimaryPIEInstance();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbIsPrimaryPIEInstance())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor225() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbWaitingOnOnlineSubsystem();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbWaitingOnOnlineSubsystem())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor226() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetChunks();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetChunks())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor227() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMaxChunks();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMaxChunks())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor228() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMaxElements();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMaxElements())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor229() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumChunks();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumChunks())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor230() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumElements();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumElements())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor231() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjects420();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjects420())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<FUObjectItem**>()});
}
template<class T> constexpr auto Accessor232() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjectsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjectsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<FUObjectItem*>()});
}
template<class T> constexpr auto Accessor233() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPreAllocatedObjects();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPreAllocatedObjects())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<FUObjectItem*>()});
}
template<class T> constexpr auto Accessor234() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAllFunctionsCache();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAllFunctionsCache())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor235() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAllFunctionsCacheLock();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAllFunctionsCacheLock())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor236() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassAddReferencedObjects();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassAddReferencedObjects())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor237() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassCastFlags420();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassCastFlags420())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor238() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassCastFlagsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassCastFlagsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint64>()});
}
template<class T> constexpr auto Accessor239() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassConfigName();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassConfigName())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor240() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassConstructor();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassConstructor())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor241() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassDefaultObject();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassDefaultObject())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor242() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassFlags417();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassFlags417())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor243() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassFlagsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassFlagsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor244() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassGeneratedBy();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassGeneratedBy())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<UObject*>()});
}
template<class T> constexpr auto Accessor245() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassUnique418();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassUnique418())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor246() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassUniqueBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassUniqueBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor247() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassVTableHelperCtorCaller();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassVTableHelperCtorCaller())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor248() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassWithin();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassWithin())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor249() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFirstOwnedClassRep();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFirstOwnedClassRep())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor250() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFuncMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFuncMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor251() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFuncMapLock();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFuncMapLock())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor252() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInterfaceFuncMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInterfaceFuncMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor253() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInterfaces();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInterfaces())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor254() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNativeFunctionLookupTable();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNativeFunctionLookupTable())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor255() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetFields();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetFields())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor256() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetParentFuncMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetParentFuncMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor257() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertiesStartOffset();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertiesStartOffset())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor258() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetReferenceSchema();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetReferenceSchema())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor259() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetReferenceTokenStream();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetReferenceTokenStream())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor260() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSparseClassData();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSparseClassData())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<void*>()});
}
template<class T> constexpr auto Accessor261() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSparseClassDataStruct();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSparseClassDataStruct())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor262() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSuperFuncMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSuperFuncMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor263() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUberGraphFramePointerProperty();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUberGraphFramePointerProperty())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor264() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbCooked418();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbCooked418())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor265() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbCookedBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbCookedBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor266() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbLayoutChanging();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbLayoutChanging())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor267() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbNeedsDynamicSubobjectInstancing();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbNeedsDynamicSubobjectInstancing())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor268() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbNeedsPostLoadSubobjectInstancing();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbNeedsPostLoadSubobjectInstancing())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor269() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetImportKeyField();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetImportKeyField())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor270() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetImportPath();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetImportPath())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor271() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRowMap();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRowMap())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor272() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRowStruct();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRowStruct())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor273() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCppForm();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCppForm())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor274() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCppType();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCppType())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor275() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEnumDisplayNameFn();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEnumDisplayNameFn())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor276() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEnumFlags_Internal();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEnumFlags_Internal())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor277() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEnumPackage();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEnumPackage())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor278() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNames409();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNames409())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor279() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNames415();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNames415())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor280() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNames416();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNames416())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor281() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNames507();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNames507())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor282() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNamesBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNamesBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor283() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUnderlyingType();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUnderlyingType())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor284() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNext();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNext())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor285() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEventGraphCallOffset();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEventGraphCallOffset())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor286() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEventGraphFunction();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEventGraphFunction())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor287() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFirstPropertyToInit();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFirstPropertyToInit())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor288() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFunc419();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFunc419())>();
 // Exact private declaration value type; visibility does not change its size.
 else return WidthDetail::ValueWidth<std::function<void(UObject*, FFrame&, void* const)>*>();
}
template<class T> constexpr auto Accessor289() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFuncBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFuncBase())>();
 // Exact private declaration value type; visibility does not change its size.
 else return WidthDetail::ValueWidth<void (UObject::*)(FFrame&, void* const)>();
}
template<class T> constexpr auto Accessor290() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFunctionFlags417();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFunctionFlags417())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor291() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFunctionFlagsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFunctionFlagsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor292() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumParms();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumParms())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint8>()});
}
template<class T> constexpr auto Accessor293() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetParmsSize();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetParmsSize())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint16>()});
}
template<class T> constexpr auto Accessor294() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRPCId();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRPCId())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint16>()});
}
template<class T> constexpr auto Accessor295() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRPCResponseId();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRPCResponseId())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint16>()});
}
template<class T> constexpr auto Accessor296() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRepOffset();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRepOffset())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint16>()});
}
template<class T> constexpr auto Accessor297() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetReturnValueOffset();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetReturnValueOffset())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint16>()});
}
template<class T> constexpr auto Accessor298() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetActiveSplitscreenType();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetActiveSplitscreenType())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor299() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAudioDeviceHandle();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAudioDeviceHandle())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor300() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentBufferVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentBufferVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor301() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentGroomVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentGroomVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor302() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentLumenVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentLumenVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor303() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentMegaLightsVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentMegaLightsVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor304() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentNaniteVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentNaniteVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor305() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentStrataVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentStrataVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor306() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentSubstrateVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentSubstrateVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor307() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentVirtualShadowMapVisualizationMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentVirtualShadowMapVisualizationMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor308() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCursorWidgets423();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCursorWidgets423())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor309() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCursorWidgets500();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCursorWidgets500())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor310() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCursorWidgetsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCursorWidgetsBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor311() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDebugProperties();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDebugProperties())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor312() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEngineShowFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEngineShowFlags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor313() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameLayerManager();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameLayerManager())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor314() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameLayerManagerPtr500();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameLayerManagerPtr500())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor315() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetGameLayerManagerPtrBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetGameLayerManagerPtrBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor316() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHardwareCursorCache423();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHardwareCursorCache423())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor317() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHardwareCursorCacheBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHardwareCursorCacheBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor318() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHardwareCursors423();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHardwareCursors423())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor319() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHardwareCursorsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHardwareCursorsBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor320() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHighResScreenshotDialog500();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHighResScreenshotDialog500())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor321() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetHighResScreenshotDialogBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetHighResScreenshotDialogBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor322() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMaxSplitscreenPlayers();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMaxSplitscreenPlayers())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor323() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMouseCaptureMode411();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMouseCaptureMode411())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor324() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMouseCaptureModeBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMouseCaptureModeBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor325() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMouseLockMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMouseLockMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor326() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSplitscreenInfo();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSplitscreenInfo())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor327() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStatHitchesData();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStatHitchesData())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor328() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStatThermalsData();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStatThermalsData())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor329() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStatUnitData();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStatUnitData())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor330() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTitleSafeZone();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTitleSafeZone())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor331() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewModeIndex();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewModeIndex())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor332() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewport508();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewport508())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor333() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor334() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportConsole();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportConsole())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor335() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportFrame508();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportFrame508())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor336() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportFrameBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportFrameBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor337() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportOverlayWidget500();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportOverlayWidget500())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor338() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportOverlayWidget508();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportOverlayWidget508())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor339() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportOverlayWidgetBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportOverlayWidgetBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor340() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetWindow500();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetWindow500())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor341() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetWindowBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetWindowBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor342() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetWorld();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetWorld())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor343() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbDisableSplitScreenOverride();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbDisableSplitScreenOverride())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor344() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbHasAudioFocus();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbHasAudioFocus())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor345() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbHideCursorDuringCapture();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbHideCursorDuringCapture())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor346() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbIgnoreInput();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbIgnoreInput())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor347() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbIsMouseOverClient();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbIsMouseOverClient())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor348() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbLockDuringCapture();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbLockDuringCapture())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor349() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbSuppressTransitionMessage();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbSuppressTransitionMessage())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor350() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbUseSoftwareCursorWidgets();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbUseSoftwareCursorWidgets())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor351() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAspectRatioAxisConstraint();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAspectRatioAxisConstraint())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor352() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetConnectionIdentifier();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetConnectionIdentifier())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor353() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetControllerId();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetControllerId())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor354() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastViewLocation();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastViewLocation())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor355() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOnlineSession();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOnlineSession())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor356() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewportClient();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewportClient())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor357() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbEmulateSplitscreen();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbEmulateSplitscreen())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor358() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetClassPrivate();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetClassPrivate())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<UClass*>()});
}
template<class T> constexpr auto Accessor359() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetInternalIndex_Private();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetInternalIndex_Private())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor360() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNamePrivate();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNamePrivate())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor361() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetObjectFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetObjectFlags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor362() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOuterPrivate();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOuterPrivate())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<UObject*>()});
}
template<class T> constexpr auto Accessor363() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetConfiguredInternetSpeed();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetConfiguredInternetSpeed())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor364() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetConfiguredLanSpeed();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetConfiguredLanSpeed())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor365() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCurrentNetSpeed();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCurrentNetSpeed())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor366() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCppStructOps();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCppStructOps())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor367() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStructFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStructFlags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor368() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbCppStructOpsFromBaseClass();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbCppStructOpsFromBaseClass())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor369() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbPrepareCppStructOpsCompleted();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbPrepareCppStructOpsCompleted())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor370() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAlignment();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAlignment())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor371() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFakeVPtr();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFakeVPtr())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor372() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSize();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSize())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor373() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetChildProperties();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetChildProperties())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor374() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetChildren();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetChildren())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor375() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDestructorLink();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDestructorLink())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor376() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMinAlignment506();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMinAlignment506())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int16>()});
}
template<class T> constexpr auto Accessor377() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetMinAlignmentBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetMinAlignmentBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor378() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPostConstructLink();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPostConstructLink())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor379() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertiesSize();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertiesSize())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor380() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPropertyLink();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPropertyLink())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor381() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRefLink();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRefLink())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor382() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetScript();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetScript())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor383() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetScriptAndPropertyObjectReferences();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetScriptAndPropertyObjectReferences())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor384() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetScriptObjectReferences();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetScriptObjectReferences())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor385() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStructStateFlags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStructStateFlags())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor386() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSuperStruct507();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSuperStruct507())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor387() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetSuperStructBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetSuperStructBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor388() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUnresolvedScriptProperties();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUnresolvedScriptProperties())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor389() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetActiveLevelCollectionIndex();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetActiveLevelCollectionIndex())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor390() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAsyncPreRegisterLevelStreamingTasks();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAsyncPreRegisterLevelStreamingTasks())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor391() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAudioDeviceHandle();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAudioDeviceHandle())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor392() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAudioTimeSeconds501();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAudioTimeSeconds501())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor393() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAudioTimeSecondsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAudioTimeSecondsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor394() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetAuthorityGameMode();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetAuthorityGameMode())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor395() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetBlockTillLevelStreamingCompletedEpoch();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetBlockTillLevelStreamingCompletedEpoch())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor396() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetBuildStreamingDataTimer();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetBuildStreamingDataTimer())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor397() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCleanupWorldTag();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCleanupWorldTag())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor398() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetCommittedPersistentLevelName();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetCommittedPersistentLevelName())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor399() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDebugDrawTraceTag();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDebugDrawTraceTag())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor400() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDeltaRealTimeSeconds();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDeltaRealTimeSeconds())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor401() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetDeltaTimeSeconds();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetDeltaTimeSeconds())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor402() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetEditorViews();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetEditorViews())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor403() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetExtraReferencedObjects();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetExtraReferencedObjects())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor404() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetFullPurgeTriggered();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetFullPurgeTriggered())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor405() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetIsInBlockTillLevelStreamingCompleted();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetIsInBlockTillLevelStreamingCompleted())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor406() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLWILastAssignedUID();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLWILastAssignedUID())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor407() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastRenderTime();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastRenderTime())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor408() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLastTimeUnbuiltLightingWasEncountered();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLastTimeUnbuiltLightingWasEncountered())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor409() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetLevelSequenceActors();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetLevelSequenceActors())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor410() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetworkActors409();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetworkActors409())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor411() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNetworkActorsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNetworkActorsBase())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor412() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNextSwitchCountdown();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNextSwitchCountdown())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor413() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNextURL();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNextURL())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor414() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumInvalidReflectionCaptureComponents();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumInvalidReflectionCaptureComponents())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor415() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumLightingUnbuiltObjects();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumLightingUnbuiltObjects())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor416() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumStreamingLevelsBeingLoaded();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumStreamingLevelsBeingLoaded())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint16>()});
}
template<class T> constexpr auto Accessor417() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumTextureStreamingDirtyResources();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumTextureStreamingDirtyResources())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor418() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumTextureStreamingUnbuiltComponents();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumTextureStreamingUnbuiltComponents())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor419() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetNumUnbuiltReflectionCaptures();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetNumUnbuiltReflectionCaptures())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<uint32>()});
}
template<class T> constexpr auto Accessor420() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetOriginOffsetThisFrame();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetOriginOffsetThisFrame())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor421() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPauseDelay501();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPauseDelay501())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor422() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPauseDelayBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPauseDelayBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor423() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPendingVisibilityLock();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPendingVisibilityLock())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor424() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPerModuleDataObjects();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPerModuleDataObjects())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor425() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPlayerNum();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPlayerNum())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor426() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPostProcessVolumeCachedSizes();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPostProcessVolumeCachedSizes())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor427() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetPreparingLevelNames();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetPreparingLevelNames())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor428() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRealTimeSeconds501();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRealTimeSeconds501())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor429() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetRealTimeSecondsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetRealTimeSecondsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor430() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStreamingLevelsPrefix();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStreamingLevelsPrefix())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor431() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetStreamingVolumeUpdateDelay();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetStreamingVolumeUpdateDelay())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<int32>()});
}
template<class T> constexpr auto Accessor432() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTimeSeconds501();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTimeSeconds501())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor433() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTimeSecondsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTimeSecondsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor434() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetTimeSinceLastPendingKillPurge();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetTimeSinceLastPendingKillPurge())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor435() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetURL();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetURL())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor436() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUnpausedTimeSeconds501();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUnpausedTimeSeconds501())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<double>()});
}
template<class T> constexpr auto Accessor437() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetUnpausedTimeSecondsBase();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetUnpausedTimeSecondsBase())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<float>()});
}
template<class T> constexpr auto Accessor438() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetViewLocationsRenderedLastFrame();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetViewLocationsRenderedLastFrame())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor439() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbAllowDeferredPhysicsStateCreation();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbAllowDeferredPhysicsStateCreation())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor440() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbCreateRenderStateForHiddenComponents();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbCreateRenderStateForHiddenComponents())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor441() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbDebugDrawAllTraceTags();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbDebugDrawAllTraceTags())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor442() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbHack_Force_UsesGameHiddenFlags_True();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbHack_Force_UsesGameHiddenFlags_True())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor443() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbIsBeingCleanedUp();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbIsBeingCleanedUp())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor444() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbOriginOffsetThisFrame();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbOriginOffsetThisFrame())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor445() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbShouldDelayGarbageCollect();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbShouldDelayGarbageCollect())>();
 else return ConservativeWidth({WidthDetail::ValueWidth<bool>()});
}
template<class T> constexpr auto Accessor446() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbSupportsMakingInvisibleTransactionRequests();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbSupportsMakingInvisibleTransactionRequests())>();
 else return std::nullopt;
}
template<class T> constexpr auto Accessor447() noexcept -> std::optional<uint32_t> {
 if constexpr(requires(T& value){value.GetbSupportsMakingVisibleTransactionRequests();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().GetbSupportsMakingVisibleTransactionRequests())>();
 else return std::nullopt;
}
struct WidthRecord {std::string_view class_name,key;std::optional<uint32_t> width;};
constexpr WidthRecord Records[]{
{"AActor", "ActorCategory", ConservativeWidth({Accessor0<AActor>()})},
{"AActor", "AnimUpdateRateFrameCount", ConservativeWidth({Accessor1<AActor>()})},
{"AActor", "AnimUpdateRateShiftTag", ConservativeWidth({Accessor2<AActor>()})},
{"AActor", "AttachmentReplication", ConservativeWidth({Accessor3<AActor>()})},
{"AActor", "AutoReceiveInput", ConservativeWidth({Accessor4<AActor>()})},
{"AActor", "CachedLastRenderTime", ConservativeWidth({Accessor5<AActor>()})},
{"AActor", "Children", ConservativeWidth({Accessor6<AActor>()})},
{"AActor", "ControllingMatineeActors", ConservativeWidth({Accessor7<AActor>()})},
{"AActor", "CreationTime", ConservativeWidth({Accessor8<AActor>()})},
{"AActor", "CustomTimeDilation", ConservativeWidth({Accessor9<AActor>()})},
{"AActor", "DefaultUpdateOverlapsMethodDuringLevelStreaming", ConservativeWidth({Accessor10<AActor>()})},
{"AActor", "DetachFence", ConservativeWidth({Accessor11<AActor>()})},
{"AActor", "HLODLayer", ConservativeWidth({Accessor12<AActor>()})},
{"AActor", "HiddenEditorViews", ConservativeWidth({Accessor13<AActor>()})},
{"AActor", "InitialLifeSpan", ConservativeWidth({Accessor14<AActor>()})},
{"AActor", "InputComponent", ConservativeWidth({Accessor15<AActor>()})},
{"AActor", "InputConsumeOption_DEPRECATED", ConservativeWidth({Accessor16<AActor>()})},
{"AActor", "InputPriority", ConservativeWidth({Accessor17<AActor>()})},
{"AActor", "LastNetUpdateTime", ConservativeWidth({Accessor18<AActor>()})},
{"AActor", "LastRenderTime", ConservativeWidth({Accessor19<AActor>(),Accessor20<AActor>()})},
{"AActor", "Layers", ConservativeWidth({Accessor21<AActor>()})},
{"AActor", "MinNetUpdateFrequency", ConservativeWidth({Accessor22<AActor>()})},
{"AActor", "NetCullDistanceSquared", ConservativeWidth({Accessor23<AActor>()})},
{"AActor", "NetDormancy", ConservativeWidth({Accessor24<AActor>()})},
{"AActor", "NetDriverName", ConservativeWidth({Accessor25<AActor>()})},
{"AActor", "NetPriority", ConservativeWidth({Accessor26<AActor>()})},
{"AActor", "NetTag", ConservativeWidth({Accessor27<AActor>()})},
{"AActor", "NetUpdateFrequency", ConservativeWidth({Accessor28<AActor>()})},
{"AActor", "NetUpdateTime", ConservativeWidth({Accessor29<AActor>()})},
{"AActor", "OnActorBeginOverlap", ConservativeWidth({Accessor30<AActor>()})},
{"AActor", "OnActorEndOverlap", ConservativeWidth({Accessor31<AActor>()})},
{"AActor", "OnActorHit", ConservativeWidth({Accessor32<AActor>()})},
{"AActor", "OnBeginCursorOver", ConservativeWidth({Accessor33<AActor>()})},
{"AActor", "OnClicked", ConservativeWidth({Accessor34<AActor>()})},
{"AActor", "OnDestroyed", ConservativeWidth({Accessor35<AActor>()})},
{"AActor", "OnEndCursorOver", ConservativeWidth({Accessor36<AActor>()})},
{"AActor", "OnEndPlay", ConservativeWidth({Accessor37<AActor>()})},
{"AActor", "OnInputTouchBegin", ConservativeWidth({Accessor38<AActor>()})},
{"AActor", "OnInputTouchEnd", ConservativeWidth({Accessor39<AActor>()})},
{"AActor", "OnInputTouchEnter", ConservativeWidth({Accessor40<AActor>()})},
{"AActor", "OnInputTouchLeave", ConservativeWidth({Accessor41<AActor>()})},
{"AActor", "OnReleased", ConservativeWidth({Accessor42<AActor>()})},
{"AActor", "OnTakeAnyDamage", ConservativeWidth({Accessor43<AActor>()})},
{"AActor", "OnTakePointDamage", ConservativeWidth({Accessor44<AActor>()})},
{"AActor", "OnTakeRadialDamage", ConservativeWidth({Accessor45<AActor>()})},
{"AActor", "Owner", ConservativeWidth({Accessor46<AActor>()})},
{"AActor", "ParentComponent", ConservativeWidth({Accessor47<AActor>()})},
{"AActor", "ParentComponentActor", ConservativeWidth({Accessor48<AActor>(),Accessor49<AActor>()})},
{"AActor", "ParentComponentActor_DEPRECATED", ConservativeWidth({Accessor50<AActor>()})},
{"AActor", "PhysicsReplicationMode", ConservativeWidth({Accessor51<AActor>()})},
{"AActor", "PrimaryActorTick", ConservativeWidth({Accessor52<AActor>()})},
{"AActor", "RayTracingGroupId", ConservativeWidth({Accessor53<AActor>()})},
{"AActor", "RemoteRole", ConservativeWidth({Accessor54<AActor>()})},
{"AActor", "ReplicatedComponentsInfo", ConservativeWidth({Accessor55<AActor>()})},
{"AActor", "ReplicatedMovement", ConservativeWidth({Accessor56<AActor>()})},
{"AActor", "ReplicatedSubObjects", ConservativeWidth({Accessor57<AActor>()})},
{"AActor", "Role", ConservativeWidth({Accessor58<AActor>()})},
{"AActor", "RootComponent", ConservativeWidth({Accessor59<AActor>()})},
{"AActor", "SpawnCollisionHandlingMethod", ConservativeWidth({Accessor60<AActor>()})},
{"AActor", "Tags", ConservativeWidth({Accessor61<AActor>()})},
{"AActor", "TimerHandle_LifeSpanExpired", ConservativeWidth({Accessor62<AActor>()})},
{"AActor", "UpdateOverlapsMethodDuringLevelStreaming", ConservativeWidth({Accessor63<AActor>()})},
{"AGameMode", "CurrentID", ConservativeWidth({Accessor64<AGameMode>()})},
{"AGameMode", "DefaultPlayerName", ConservativeWidth({Accessor65<AGameMode>(),Accessor66<AGameMode>()})},
{"AGameMode", "EngineMessageClass", ConservativeWidth({Accessor67<AGameMode>()})},
{"AGameMode", "GameModeClassAliases", ConservativeWidth({Accessor68<AGameMode>()})},
{"AGameMode", "GameSession", ConservativeWidth({Accessor69<AGameMode>()})},
{"AGameMode", "HUDClass", ConservativeWidth({Accessor70<AGameMode>()})},
{"AGameMode", "InactivePlayerArray", ConservativeWidth({Accessor71<AGameMode>()})},
{"AGameMode", "InactivePlayerStateLifeSpan", ConservativeWidth({Accessor72<AGameMode>()})},
{"AGameMode", "MatchState", ConservativeWidth({Accessor73<AGameMode>()})},
{"AGameMode", "MaxInactivePlayers", ConservativeWidth({Accessor74<AGameMode>()})},
{"AGameMode", "MinRespawnDelay", ConservativeWidth({Accessor75<AGameMode>()})},
{"AGameMode", "NumBots", ConservativeWidth({Accessor76<AGameMode>()})},
{"AGameMode", "NumPlayers", ConservativeWidth({Accessor77<AGameMode>()})},
{"AGameMode", "NumSpectators", ConservativeWidth({Accessor78<AGameMode>()})},
{"AGameMode", "NumTravellingPlayers", ConservativeWidth({Accessor79<AGameMode>()})},
{"AGameMode", "OptionsString", ConservativeWidth({Accessor80<AGameMode>()})},
{"AGameMode", "PlayerStarts", ConservativeWidth({Accessor81<AGameMode>()})},
{"AGameMode", "PlayerStateClass", ConservativeWidth({Accessor82<AGameMode>()})},
{"AGameMode", "SpectatorClass", ConservativeWidth({Accessor83<AGameMode>()})},
{"AGameMode", "TimerHandle_DefaultTimer", ConservativeWidth({Accessor84<AGameMode>()})},
{"AGameMode", "bHandleDedicatedServerReplays", ConservativeWidth({Accessor85<AGameMode>()})},
{"AGameModeBase", "DefaultPlayerName", ConservativeWidth({Accessor86<AGameModeBase>()})},
{"AGameModeBase", "GameNetDriverReplicationSystem", ConservativeWidth({Accessor87<AGameModeBase>()})},
{"AGameModeBase", "GameSession", ConservativeWidth({Accessor88<AGameModeBase>()})},
{"AGameModeBase", "GameSessionClass", ConservativeWidth({Accessor89<AGameModeBase>()})},
{"AGameModeBase", "HUDClass", ConservativeWidth({Accessor90<AGameModeBase>()})},
{"AGameModeBase", "OptionsString", ConservativeWidth({Accessor91<AGameModeBase>()})},
{"AGameModeBase", "PlayerStateClass", ConservativeWidth({Accessor92<AGameModeBase>()})},
{"AGameModeBase", "ServerStatReplicator", ConservativeWidth({Accessor93<AGameModeBase>()})},
{"AGameModeBase", "ServerStatReplicatorClass", ConservativeWidth({Accessor94<AGameModeBase>()})},
{"AGameModeBase", "SpectatorClass", ConservativeWidth({Accessor95<AGameModeBase>()})},
{"FArchive", "ArCustomPropertyList", ConservativeWidth({})},
{"FArchive", "ArEngineNetVer", ConservativeWidth({})},
{"FArchive", "ArEngineVer", ConservativeWidth({})},
{"FArchive", "ArGameNetVer", ConservativeWidth({})},
{"FArchive", "ArLicenseeUE4Ver", ConservativeWidth({})},
{"FArchive", "ArMaxSerializeSize", ConservativeWidth({})},
{"FArchive", "ArNetVer", ConservativeWidth({})},
{"FArchive", "ArPortFlags", ConservativeWidth({})},
{"FArchive", "ArSerializingDefaults", ConservativeWidth({})},
{"FArchive", "ArUE4Ver", ConservativeWidth({})},
{"FArchive", "CookingTargetPlatform", ConservativeWidth({})},
{"FArchive", "CustomVersionContainer", ConservativeWidth({})},
{"FArchive", "SerializedProperty", ConservativeWidth({})},
{"FArchive", "bCustomVersionsAreReset", ConservativeWidth({})},
{"FArchiveState", "ArCustomPropertyList", ConservativeWidth({Accessor96<FArchiveState>()})},
{"FArchiveState", "ArEngineNetVer", ConservativeWidth({Accessor97<FArchiveState>()})},
{"FArchiveState", "ArEngineVer", ConservativeWidth({Accessor98<FArchiveState>()})},
{"FArchiveState", "ArGameNetVer", ConservativeWidth({Accessor99<FArchiveState>()})},
{"FArchiveState", "ArLicenseeUE4Ver", ConservativeWidth({Accessor100<FArchiveState>()})},
{"FArchiveState", "ArLicenseeUEVer", ConservativeWidth({Accessor101<FArchiveState>()})},
{"FArchiveState", "ArMaxSerializeSize", ConservativeWidth({Accessor102<FArchiveState>()})},
{"FArchiveState", "ArPortFlags", ConservativeWidth({Accessor103<FArchiveState>()})},
{"FArchiveState", "ArSerializingDefaults", ConservativeWidth({Accessor104<FArchiveState>()})},
{"FArchiveState", "ArUE4Ver", ConservativeWidth({Accessor105<FArchiveState>()})},
{"FArchiveState", "ArUEVer", ConservativeWidth({Accessor106<FArchiveState>()})},
{"FArchiveState", "CookingTargetPlatform", ConservativeWidth({Accessor107<FArchiveState>()})},
{"FArchiveState", "CustomVersionContainer", ConservativeWidth({Accessor108<FArchiveState>()})},
{"FArchiveState", "NextProxy", ConservativeWidth({Accessor109<FArchiveState>()})},
{"FArchiveState", "SavePackageData", ConservativeWidth({Accessor110<FArchiveState>()})},
{"FArchiveState", "SerializedProperty", ConservativeWidth({Accessor111<FArchiveState>()})},
{"FArchiveState", "bCustomVersionsAreReset", ConservativeWidth({Accessor112<FArchiveState>()})},
{"FArrayProperty", "ArrayFlags", ConservativeWidth({Accessor113<FArrayProperty>()})},
{"FArrayProperty", "Inner", ConservativeWidth({Accessor114<FArrayProperty>()})},
{"FBoolProperty", "ByteMask", ConservativeWidth({Accessor115<FBoolProperty>()})},
{"FBoolProperty", "ByteOffset", ConservativeWidth({Accessor116<FBoolProperty>()})},
{"FBoolProperty", "FieldMask", ConservativeWidth({Accessor117<FBoolProperty>()})},
{"FBoolProperty", "FieldSize", ConservativeWidth({Accessor118<FBoolProperty>()})},
{"FByteProperty", "Enum", ConservativeWidth({Accessor119<FByteProperty>()})},
{"FClassProperty", "MetaClass", ConservativeWidth({Accessor120<FClassProperty>()})},
{"FDelegateProperty", "SignatureFunction", ConservativeWidth({Accessor121<FDelegateProperty>()})},
{"FEnumProperty", "Enum", ConservativeWidth({Accessor122<FEnumProperty>()})},
{"FEnumProperty", "UnderlyingProp", ConservativeWidth({Accessor123<FEnumProperty>()})},
{"FField", "ClassPrivate", ConservativeWidth({Accessor124<FField>()})},
{"FField", "FlagsPrivate", ConservativeWidth({Accessor125<FField>()})},
{"FField", "NamePrivate", ConservativeWidth({Accessor126<FField>()})},
{"FField", "Next", ConservativeWidth({Accessor127<FField>()})},
{"FField", "Owner", ConservativeWidth({Accessor128<FField>()})},
{"FFieldClass", "CastFlags", ConservativeWidth({Accessor129<FFieldClass>(),Accessor130<FFieldClass>()})},
{"FFieldClass", "ClassFlags", ConservativeWidth({Accessor131<FFieldClass>()})},
{"FFieldClass", "ConstructFn", ConservativeWidth({Accessor132<FFieldClass>(),Accessor133<FFieldClass>()})},
{"FFieldClass", "DefaultObject", ConservativeWidth({Accessor134<FFieldClass>()})},
{"FFieldClass", "FName", ConservativeWidth({Accessor135<FFieldClass>()})},
{"FFieldClass", "Id", ConservativeWidth({Accessor136<FFieldClass>()})},
{"FFieldClass", "SuperClass", ConservativeWidth({Accessor137<FFieldClass>()})},
{"FFieldClass", "UniqueNameIndexCounter", ConservativeWidth({Accessor138<FFieldClass>()})},
{"FFieldClass", "UnqiueNameIndexCounter", ConservativeWidth({Accessor139<FFieldClass>()})},
{"FFieldPathProperty", "PropertyClass", ConservativeWidth({Accessor140<FFieldPathProperty>()})},
{"FInterfaceProperty", "InterfaceClass", ConservativeWidth({Accessor141<FInterfaceProperty>()})},
{"FMapProperty", "KeyProp", ConservativeWidth({Accessor142<FMapProperty>()})},
{"FMapProperty", "MapFlags", ConservativeWidth({Accessor143<FMapProperty>()})},
{"FMapProperty", "MapLayout", ConservativeWidth({Accessor144<FMapProperty>()})},
{"FMapProperty", "ValueProp", ConservativeWidth({Accessor145<FMapProperty>()})},
{"FMulticastDelegateProperty", "SignatureFunction", ConservativeWidth({Accessor146<FMulticastDelegateProperty>()})},
{"FObjectPropertyBase", "PropertyClass", ConservativeWidth({Accessor147<FObjectPropertyBase>()})},
{"FOutputDevice", "bAutoEmitLineTerminator", ConservativeWidth({Accessor148<FOutputDevice>()})},
{"FOutputDevice", "bSuppressEventTag", ConservativeWidth({Accessor149<FOutputDevice>()})},
{"FProperty", "ArrayDim", ConservativeWidth({Accessor150<FProperty>()})},
{"FProperty", "DestructorLinkNext", ConservativeWidth({Accessor151<FProperty>()})},
{"FProperty", "ElementSize", ConservativeWidth({Accessor152<FProperty>()})},
{"FProperty", "NextRef", ConservativeWidth({Accessor153<FProperty>()})},
{"FProperty", "Offset_Internal", ConservativeWidth({Accessor154<FProperty>()})},
{"FProperty", "PostConstructLinkNext", ConservativeWidth({Accessor155<FProperty>()})},
{"FProperty", "PropertyFlags", ConservativeWidth({Accessor156<FProperty>(),Accessor157<FProperty>()})},
{"FProperty", "PropertyLinkNext", ConservativeWidth({Accessor158<FProperty>()})},
{"FProperty", "RepIndex", ConservativeWidth({Accessor159<FProperty>()})},
{"FProperty", "RepNotifyFunc", ConservativeWidth({Accessor160<FProperty>()})},
{"FSetProperty", "ElementProp", ConservativeWidth({Accessor161<FSetProperty>()})},
{"FSetProperty", "SetLayout", ConservativeWidth({Accessor162<FSetProperty>(),Accessor163<FSetProperty>()})},
{"FSoftClassProperty", "MetaClass", ConservativeWidth({Accessor164<FSoftClassProperty>()})},
{"FStructProperty", "Struct", ConservativeWidth({Accessor165<FStructProperty>()})},
{"FUObjectArray", "MasterSerialNumber", ConservativeWidth({Accessor166<FUObjectArray>()})},
{"FUObjectArray", "MaxObjectsNotConsideredByGC", ConservativeWidth({Accessor167<FUObjectArray>()})},
{"FUObjectArray", "ObjAvailable", ConservativeWidth({Accessor168<FUObjectArray>()})},
{"FUObjectArray", "ObjAvailableList", ConservativeWidth({Accessor169<FUObjectArray>(),Accessor170<FUObjectArray>()})},
{"FUObjectArray", "ObjAvailableListEstimateCount", ConservativeWidth({Accessor171<FUObjectArray>()})},
{"FUObjectArray", "ObjFirstGCIndex", ConservativeWidth({Accessor172<FUObjectArray>()})},
{"FUObjectArray", "ObjLastNonGCIndex", ConservativeWidth({Accessor173<FUObjectArray>()})},
{"FUObjectArray", "ObjObjects", ConservativeWidth({Accessor174<FUObjectArray>()})},
{"FUObjectArray", "OpenForDisregardForGC", ConservativeWidth({Accessor175<FUObjectArray>(),Accessor176<FUObjectArray>()})},
{"FUObjectArray", "PrimarySerialNumber", ConservativeWidth({Accessor177<FUObjectArray>()})},
{"FUObjectArray", "UObjectCreateListeners", ConservativeWidth({Accessor178<FUObjectArray>()})},
{"FUObjectArray", "UObjectDeleteListeners", ConservativeWidth({Accessor179<FUObjectArray>()})},
{"FUObjectArray", "bShouldRecycleObjectIndices", ConservativeWidth({Accessor180<FUObjectArray>()})},
{"FUObjectHashTables", "AllClassesVersion", ConservativeWidth({Accessor181<FUObjectHashTables>()})},
{"FUObjectHashTables", "ClassToChildListMap", ConservativeWidth({Accessor182<FUObjectHashTables>()})},
{"FUObjectHashTables", "ClassToChildListMapVersion", ConservativeWidth({Accessor183<FUObjectHashTables>()})},
{"FUObjectHashTables", "ClassToObjectListMap", ConservativeWidth({Accessor184<FUObjectHashTables>(),Accessor185<FUObjectHashTables>()})},
{"FUObjectHashTables", "Hash", ConservativeWidth({Accessor186<FUObjectHashTables>()})},
{"FUObjectHashTables", "HashOuter", ConservativeWidth({Accessor187<FUObjectHashTables>(),Accessor188<FUObjectHashTables>()})},
{"FUObjectHashTables", "NativeClassesVersion", ConservativeWidth({Accessor189<FUObjectHashTables>()})},
{"FUObjectHashTables", "ObjectOuterMap", ConservativeWidth({Accessor190<FUObjectHashTables>(),Accessor191<FUObjectHashTables>()})},
{"FUObjectHashTables", "ObjectToPackageMap", ConservativeWidth({Accessor192<FUObjectHashTables>()})},
{"FUObjectHashTables", "PackageToObjectListMap", ConservativeWidth({Accessor193<FUObjectHashTables>()})},
{"FUObjectItem", "ClusterAndFlags", ConservativeWidth({Accessor194<FUObjectItem>()})},
{"FUObjectItem", "ClusterIndex", ConservativeWidth({Accessor195<FUObjectItem>()})},
{"FUObjectItem", "ClusterRootIndex", ConservativeWidth({Accessor196<FUObjectItem>()})},
{"FUObjectItem", "Flags", ConservativeWidth({Accessor197<FUObjectItem>()})},
{"FUObjectItem", "FlagsAndRefCount", ConservativeWidth({Accessor198<FUObjectItem>()})},
{"FUObjectItem", "Object", ConservativeWidth({Accessor199<FUObjectItem>()})},
{"FUObjectItem", "ObjectPtrLow", ConservativeWidth({Accessor200<FUObjectItem>()})},
{"FUObjectItem", "RefCount", ConservativeWidth({Accessor201<FUObjectItem>()})},
{"FUObjectItem", "RemoteId", ConservativeWidth({Accessor202<FUObjectItem>()})},
{"FUObjectItem", "SerialNumber", ConservativeWidth({Accessor203<FUObjectItem>()})},
{"FWorldContext", "AudioDeviceHandle", ConservativeWidth({Accessor204<FWorldContext>()})},
{"FWorldContext", "AudioDeviceID", ConservativeWidth({Accessor205<FWorldContext>()})},
{"FWorldContext", "ContextHandle", ConservativeWidth({Accessor206<FWorldContext>()})},
{"FWorldContext", "CustomDescription", ConservativeWidth({Accessor207<FWorldContext>()})},
{"FWorldContext", "ExternalReferences", ConservativeWidth({Accessor208<FWorldContext>()})},
{"FWorldContext", "GameViewport", ConservativeWidth({Accessor209<FWorldContext>()})},
{"FWorldContext", "GarbageObjectsToVerify", ConservativeWidth({Accessor210<FWorldContext>()})},
{"FWorldContext", "LastRemoteURL", ConservativeWidth({Accessor211<FWorldContext>()})},
{"FWorldContext", "LastURL", ConservativeWidth({Accessor212<FWorldContext>()})},
{"FWorldContext", "LevelsToLoadForPendingMapChange", ConservativeWidth({Accessor213<FWorldContext>()})},
{"FWorldContext", "PIEAccumulatedTickSeconds", ConservativeWidth({Accessor214<FWorldContext>()})},
{"FWorldContext", "PIEFixedTickSeconds", ConservativeWidth({Accessor215<FWorldContext>()})},
{"FWorldContext", "PIEInstance", ConservativeWidth({Accessor216<FWorldContext>()})},
{"FWorldContext", "PIEPrefix", ConservativeWidth({Accessor217<FWorldContext>()})},
{"FWorldContext", "PIERemapPrefix", ConservativeWidth({Accessor218<FWorldContext>()})},
{"FWorldContext", "PendingMapChangeFailureDescription", ConservativeWidth({Accessor219<FWorldContext>()})},
{"FWorldContext", "RunAsDedicated", ConservativeWidth({Accessor220<FWorldContext>()})},
{"FWorldContext", "ThisCurrentWorld", ConservativeWidth({Accessor221<FWorldContext>()})},
{"FWorldContext", "TravelType", ConservativeWidth({Accessor222<FWorldContext>()})},
{"FWorldContext", "TravelURL", ConservativeWidth({Accessor223<FWorldContext>()})},
{"FWorldContext", "bIsPrimaryPIEInstance", ConservativeWidth({Accessor224<FWorldContext>()})},
{"FWorldContext", "bWaitingOnOnlineSubsystem", ConservativeWidth({Accessor225<FWorldContext>()})},
{"TUObjectArray", "Chunks", ConservativeWidth({Accessor226<TUObjectArray>()})},
{"TUObjectArray", "MaxChunks", ConservativeWidth({Accessor227<TUObjectArray>()})},
{"TUObjectArray", "MaxElements", ConservativeWidth({Accessor228<TUObjectArray>()})},
{"TUObjectArray", "NumChunks", ConservativeWidth({Accessor229<TUObjectArray>()})},
{"TUObjectArray", "NumElements", ConservativeWidth({Accessor230<TUObjectArray>()})},
{"TUObjectArray", "Objects", ConservativeWidth({Accessor231<TUObjectArray>(),Accessor232<TUObjectArray>()})},
{"TUObjectArray", "PreAllocatedObjects", ConservativeWidth({Accessor233<TUObjectArray>()})},
{"UClass", "AllFunctionsCache", ConservativeWidth({Accessor234<UClass>()})},
{"UClass", "AllFunctionsCacheLock", ConservativeWidth({Accessor235<UClass>()})},
{"UClass", "ClassAddReferencedObjects", ConservativeWidth({Accessor236<UClass>()})},
{"UClass", "ClassCastFlags", ConservativeWidth({Accessor237<UClass>(),Accessor238<UClass>()})},
{"UClass", "ClassConfigName", ConservativeWidth({Accessor239<UClass>()})},
{"UClass", "ClassConstructor", ConservativeWidth({Accessor240<UClass>()})},
{"UClass", "ClassDefaultObject", ConservativeWidth({Accessor241<UClass>()})},
{"UClass", "ClassFlags", ConservativeWidth({Accessor242<UClass>(),Accessor243<UClass>()})},
{"UClass", "ClassGeneratedBy", ConservativeWidth({Accessor244<UClass>()})},
{"UClass", "ClassUnique", ConservativeWidth({Accessor245<UClass>(),Accessor246<UClass>()})},
{"UClass", "ClassVTableHelperCtorCaller", ConservativeWidth({Accessor247<UClass>()})},
{"UClass", "ClassWithin", ConservativeWidth({Accessor248<UClass>()})},
{"UClass", "FirstOwnedClassRep", ConservativeWidth({Accessor249<UClass>()})},
{"UClass", "FuncMap", ConservativeWidth({Accessor250<UClass>()})},
{"UClass", "FuncMapLock", ConservativeWidth({Accessor251<UClass>()})},
{"UClass", "InterfaceFuncMap", ConservativeWidth({Accessor252<UClass>()})},
{"UClass", "Interfaces", ConservativeWidth({Accessor253<UClass>()})},
{"UClass", "NativeFunctionLookupTable", ConservativeWidth({Accessor254<UClass>()})},
{"UClass", "NetFields", ConservativeWidth({Accessor255<UClass>()})},
{"UClass", "ParentFuncMap", ConservativeWidth({Accessor256<UClass>()})},
{"UClass", "PropertiesStartOffset", ConservativeWidth({Accessor257<UClass>()})},
{"UClass", "ReferenceSchema", ConservativeWidth({Accessor258<UClass>()})},
{"UClass", "ReferenceTokenStream", ConservativeWidth({Accessor259<UClass>()})},
{"UClass", "SparseClassData", ConservativeWidth({Accessor260<UClass>()})},
{"UClass", "SparseClassDataStruct", ConservativeWidth({Accessor261<UClass>()})},
{"UClass", "SuperFuncMap", ConservativeWidth({Accessor262<UClass>()})},
{"UClass", "UberGraphFramePointerProperty", ConservativeWidth({Accessor263<UClass>()})},
{"UClass", "bCooked", ConservativeWidth({Accessor264<UClass>(),Accessor265<UClass>()})},
{"UClass", "bLayoutChanging", ConservativeWidth({Accessor266<UClass>()})},
{"UClass", "bNeedsDynamicSubobjectInstancing", ConservativeWidth({Accessor267<UClass>()})},
{"UClass", "bNeedsPostLoadSubobjectInstancing", ConservativeWidth({Accessor268<UClass>()})},
{"UDataTable", "ImportKeyField", ConservativeWidth({Accessor269<UDataTable>()})},
{"UDataTable", "ImportPath", ConservativeWidth({Accessor270<UDataTable>()})},
{"UDataTable", "RowMap", ConservativeWidth({Accessor271<UDataTable>()})},
{"UDataTable", "RowStruct", ConservativeWidth({Accessor272<UDataTable>()})},
{"UEnum", "CppForm", ConservativeWidth({Accessor273<UEnum>()})},
{"UEnum", "CppType", ConservativeWidth({Accessor274<UEnum>()})},
{"UEnum", "EnumDisplayNameFn", ConservativeWidth({Accessor275<UEnum>()})},
{"UEnum", "EnumFlags_Internal", ConservativeWidth({Accessor276<UEnum>()})},
{"UEnum", "EnumPackage", ConservativeWidth({Accessor277<UEnum>()})},
{"UEnum", "Names", ConservativeWidth({Accessor278<UEnum>(),Accessor279<UEnum>(),Accessor280<UEnum>(),Accessor281<UEnum>(),Accessor282<UEnum>()})},
{"UEnum", "UnderlyingType", ConservativeWidth({Accessor283<UEnum>()})},
{"UField", "Next", ConservativeWidth({Accessor284<UField>()})},
{"UFunction", "EventGraphCallOffset", ConservativeWidth({Accessor285<UFunction>()})},
{"UFunction", "EventGraphFunction", ConservativeWidth({Accessor286<UFunction>()})},
{"UFunction", "FirstPropertyToInit", ConservativeWidth({Accessor287<UFunction>()})},
{"UFunction", "Func", ConservativeWidth({Accessor288<UFunction>(),Accessor289<UFunction>()})},
{"UFunction", "FunctionFlags", ConservativeWidth({Accessor290<UFunction>(),Accessor291<UFunction>()})},
{"UFunction", "NumParms", ConservativeWidth({Accessor292<UFunction>()})},
{"UFunction", "ParmsSize", ConservativeWidth({Accessor293<UFunction>()})},
{"UFunction", "RPCId", ConservativeWidth({Accessor294<UFunction>()})},
{"UFunction", "RPCResponseId", ConservativeWidth({Accessor295<UFunction>()})},
{"UFunction", "RepOffset", ConservativeWidth({Accessor296<UFunction>()})},
{"UFunction", "ReturnValueOffset", ConservativeWidth({Accessor297<UFunction>()})},
{"UGameViewportClient", "ActiveSplitscreenType", ConservativeWidth({Accessor298<UGameViewportClient>()})},
{"UGameViewportClient", "AudioDeviceHandle", ConservativeWidth({Accessor299<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentBufferVisualizationMode", ConservativeWidth({Accessor300<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentGroomVisualizationMode", ConservativeWidth({Accessor301<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentLumenVisualizationMode", ConservativeWidth({Accessor302<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentMegaLightsVisualizationMode", ConservativeWidth({Accessor303<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentNaniteVisualizationMode", ConservativeWidth({Accessor304<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentStrataVisualizationMode", ConservativeWidth({Accessor305<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentSubstrateVisualizationMode", ConservativeWidth({Accessor306<UGameViewportClient>()})},
{"UGameViewportClient", "CurrentVirtualShadowMapVisualizationMode", ConservativeWidth({Accessor307<UGameViewportClient>()})},
{"UGameViewportClient", "CursorWidgets", ConservativeWidth({Accessor308<UGameViewportClient>(),Accessor309<UGameViewportClient>(),Accessor310<UGameViewportClient>()})},
{"UGameViewportClient", "DebugProperties", ConservativeWidth({Accessor311<UGameViewportClient>()})},
{"UGameViewportClient", "EngineShowFlags", ConservativeWidth({Accessor312<UGameViewportClient>()})},
{"UGameViewportClient", "GameLayerManager", ConservativeWidth({Accessor313<UGameViewportClient>()})},
{"UGameViewportClient", "GameLayerManagerPtr", ConservativeWidth({Accessor314<UGameViewportClient>(),Accessor315<UGameViewportClient>()})},
{"UGameViewportClient", "HardwareCursorCache", ConservativeWidth({Accessor316<UGameViewportClient>(),Accessor317<UGameViewportClient>()})},
{"UGameViewportClient", "HardwareCursors", ConservativeWidth({Accessor318<UGameViewportClient>(),Accessor319<UGameViewportClient>()})},
{"UGameViewportClient", "HighResScreenshotDialog", ConservativeWidth({Accessor320<UGameViewportClient>(),Accessor321<UGameViewportClient>()})},
{"UGameViewportClient", "MaxSplitscreenPlayers", ConservativeWidth({Accessor322<UGameViewportClient>()})},
{"UGameViewportClient", "MouseCaptureMode", ConservativeWidth({Accessor323<UGameViewportClient>(),Accessor324<UGameViewportClient>()})},
{"UGameViewportClient", "MouseLockMode", ConservativeWidth({Accessor325<UGameViewportClient>()})},
{"UGameViewportClient", "SplitscreenInfo", ConservativeWidth({Accessor326<UGameViewportClient>()})},
{"UGameViewportClient", "StatHitchesData", ConservativeWidth({Accessor327<UGameViewportClient>()})},
{"UGameViewportClient", "StatThermalsData", ConservativeWidth({Accessor328<UGameViewportClient>()})},
{"UGameViewportClient", "StatUnitData", ConservativeWidth({Accessor329<UGameViewportClient>()})},
{"UGameViewportClient", "TitleSafeZone", ConservativeWidth({Accessor330<UGameViewportClient>()})},
{"UGameViewportClient", "ViewModeIndex", ConservativeWidth({Accessor331<UGameViewportClient>()})},
{"UGameViewportClient", "Viewport", ConservativeWidth({Accessor332<UGameViewportClient>(),Accessor333<UGameViewportClient>()})},
{"UGameViewportClient", "ViewportConsole", ConservativeWidth({Accessor334<UGameViewportClient>()})},
{"UGameViewportClient", "ViewportFrame", ConservativeWidth({Accessor335<UGameViewportClient>(),Accessor336<UGameViewportClient>()})},
{"UGameViewportClient", "ViewportOverlayWidget", ConservativeWidth({Accessor337<UGameViewportClient>(),Accessor338<UGameViewportClient>(),Accessor339<UGameViewportClient>()})},
{"UGameViewportClient", "Window", ConservativeWidth({Accessor340<UGameViewportClient>(),Accessor341<UGameViewportClient>()})},
{"UGameViewportClient", "World", ConservativeWidth({Accessor342<UGameViewportClient>()})},
{"UGameViewportClient", "bDisableSplitScreenOverride", ConservativeWidth({Accessor343<UGameViewportClient>()})},
{"UGameViewportClient", "bHasAudioFocus", ConservativeWidth({Accessor344<UGameViewportClient>()})},
{"UGameViewportClient", "bHideCursorDuringCapture", ConservativeWidth({Accessor345<UGameViewportClient>()})},
{"UGameViewportClient", "bIgnoreInput", ConservativeWidth({Accessor346<UGameViewportClient>()})},
{"UGameViewportClient", "bIsMouseOverClient", ConservativeWidth({Accessor347<UGameViewportClient>()})},
{"UGameViewportClient", "bLockDuringCapture", ConservativeWidth({Accessor348<UGameViewportClient>()})},
{"UGameViewportClient", "bSuppressTransitionMessage", ConservativeWidth({Accessor349<UGameViewportClient>()})},
{"UGameViewportClient", "bUseSoftwareCursorWidgets", ConservativeWidth({Accessor350<UGameViewportClient>()})},
{"ULocalPlayer", "AspectRatioAxisConstraint", ConservativeWidth({Accessor351<ULocalPlayer>()})},
{"ULocalPlayer", "ConnectionIdentifier", ConservativeWidth({Accessor352<ULocalPlayer>()})},
{"ULocalPlayer", "ControllerId", ConservativeWidth({Accessor353<ULocalPlayer>()})},
{"ULocalPlayer", "LastViewLocation", ConservativeWidth({Accessor354<ULocalPlayer>()})},
{"ULocalPlayer", "OnlineSession", ConservativeWidth({Accessor355<ULocalPlayer>()})},
{"ULocalPlayer", "ViewportClient", ConservativeWidth({Accessor356<ULocalPlayer>()})},
{"ULocalPlayer", "bEmulateSplitscreen", ConservativeWidth({Accessor357<ULocalPlayer>()})},
{"UObjectBase", "ClassPrivate", ConservativeWidth({Accessor358<UObjectBase>()})},
{"UObjectBase", "InternalIndex_Private", ConservativeWidth({Accessor359<UObjectBase>()})},
{"UObjectBase", "NamePrivate", ConservativeWidth({Accessor360<UObjectBase>()})},
{"UObjectBase", "ObjectFlags", ConservativeWidth({Accessor361<UObjectBase>()})},
{"UObjectBase", "OuterPrivate", ConservativeWidth({Accessor362<UObjectBase>()})},
{"UPlayer", "ConfiguredInternetSpeed", ConservativeWidth({Accessor363<UPlayer>()})},
{"UPlayer", "ConfiguredLanSpeed", ConservativeWidth({Accessor364<UPlayer>()})},
{"UPlayer", "CurrentNetSpeed", ConservativeWidth({Accessor365<UPlayer>()})},
{"UScriptStruct", "CppStructOps", ConservativeWidth({Accessor366<UScriptStruct>()})},
{"UScriptStruct", "StructFlags", ConservativeWidth({Accessor367<UScriptStruct>()})},
{"UScriptStruct", "bCppStructOpsFromBaseClass", ConservativeWidth({Accessor368<UScriptStruct>()})},
{"UScriptStruct", "bPrepareCppStructOpsCompleted", ConservativeWidth({Accessor369<UScriptStruct>()})},
{"UScriptStruct::ICppStructOps", "Alignment", ConservativeWidth({Accessor370<UScriptStruct::ICppStructOps>()})},
{"UScriptStruct::ICppStructOps", "FakeVPtr", ConservativeWidth({Accessor371<UScriptStruct::ICppStructOps>()})},
{"UScriptStruct::ICppStructOps", "Size", ConservativeWidth({Accessor372<UScriptStruct::ICppStructOps>()})},
{"UStruct", "ChildProperties", ConservativeWidth({Accessor373<UStruct>()})},
{"UStruct", "Children", ConservativeWidth({Accessor374<UStruct>()})},
{"UStruct", "DestructorLink", ConservativeWidth({Accessor375<UStruct>()})},
{"UStruct", "MinAlignment", ConservativeWidth({Accessor376<UStruct>(),Accessor377<UStruct>()})},
{"UStruct", "PostConstructLink", ConservativeWidth({Accessor378<UStruct>()})},
{"UStruct", "PropertiesSize", ConservativeWidth({Accessor379<UStruct>()})},
{"UStruct", "PropertyLink", ConservativeWidth({Accessor380<UStruct>()})},
{"UStruct", "RefLink", ConservativeWidth({Accessor381<UStruct>()})},
{"UStruct", "Script", ConservativeWidth({Accessor382<UStruct>()})},
{"UStruct", "ScriptAndPropertyObjectReferences", ConservativeWidth({Accessor383<UStruct>()})},
{"UStruct", "ScriptObjectReferences", ConservativeWidth({Accessor384<UStruct>()})},
{"UStruct", "StructStateFlags", ConservativeWidth({Accessor385<UStruct>()})},
{"UStruct", "SuperStruct", ConservativeWidth({Accessor386<UStruct>(),Accessor387<UStruct>()})},
{"UStruct", "UnresolvedScriptProperties", ConservativeWidth({Accessor388<UStruct>()})},
{"UWorld", "ActiveLevelCollectionIndex", ConservativeWidth({Accessor389<UWorld>()})},
{"UWorld", "AsyncPreRegisterLevelStreamingTasks", ConservativeWidth({Accessor390<UWorld>()})},
{"UWorld", "AudioDeviceHandle", ConservativeWidth({Accessor391<UWorld>()})},
{"UWorld", "AudioTimeSeconds", ConservativeWidth({Accessor392<UWorld>(),Accessor393<UWorld>()})},
{"UWorld", "AuthorityGameMode", ConservativeWidth({Accessor394<UWorld>()})},
{"UWorld", "BlockTillLevelStreamingCompletedEpoch", ConservativeWidth({Accessor395<UWorld>()})},
{"UWorld", "BuildStreamingDataTimer", ConservativeWidth({Accessor396<UWorld>()})},
{"UWorld", "CleanupWorldTag", ConservativeWidth({Accessor397<UWorld>()})},
{"UWorld", "CommittedPersistentLevelName", ConservativeWidth({Accessor398<UWorld>()})},
{"UWorld", "DebugDrawTraceTag", ConservativeWidth({Accessor399<UWorld>()})},
{"UWorld", "DeltaRealTimeSeconds", ConservativeWidth({Accessor400<UWorld>()})},
{"UWorld", "DeltaTimeSeconds", ConservativeWidth({Accessor401<UWorld>()})},
{"UWorld", "EditorViews", ConservativeWidth({Accessor402<UWorld>()})},
{"UWorld", "ExtraReferencedObjects", ConservativeWidth({Accessor403<UWorld>()})},
{"UWorld", "FullPurgeTriggered", ConservativeWidth({Accessor404<UWorld>()})},
{"UWorld", "IsInBlockTillLevelStreamingCompleted", ConservativeWidth({Accessor405<UWorld>()})},
{"UWorld", "LWILastAssignedUID", ConservativeWidth({Accessor406<UWorld>()})},
{"UWorld", "LastRenderTime", ConservativeWidth({Accessor407<UWorld>()})},
{"UWorld", "LastTimeUnbuiltLightingWasEncountered", ConservativeWidth({Accessor408<UWorld>()})},
{"UWorld", "LevelSequenceActors", ConservativeWidth({Accessor409<UWorld>()})},
{"UWorld", "NetworkActors", ConservativeWidth({Accessor410<UWorld>(),Accessor411<UWorld>()})},
{"UWorld", "NextSwitchCountdown", ConservativeWidth({Accessor412<UWorld>()})},
{"UWorld", "NextURL", ConservativeWidth({Accessor413<UWorld>()})},
{"UWorld", "NumInvalidReflectionCaptureComponents", ConservativeWidth({Accessor414<UWorld>()})},
{"UWorld", "NumLightingUnbuiltObjects", ConservativeWidth({Accessor415<UWorld>()})},
{"UWorld", "NumStreamingLevelsBeingLoaded", ConservativeWidth({Accessor416<UWorld>()})},
{"UWorld", "NumTextureStreamingDirtyResources", ConservativeWidth({Accessor417<UWorld>()})},
{"UWorld", "NumTextureStreamingUnbuiltComponents", ConservativeWidth({Accessor418<UWorld>()})},
{"UWorld", "NumUnbuiltReflectionCaptures", ConservativeWidth({Accessor419<UWorld>()})},
{"UWorld", "OriginOffsetThisFrame", ConservativeWidth({Accessor420<UWorld>()})},
{"UWorld", "PauseDelay", ConservativeWidth({Accessor421<UWorld>(),Accessor422<UWorld>()})},
{"UWorld", "PendingVisibilityLock", ConservativeWidth({Accessor423<UWorld>()})},
{"UWorld", "PerModuleDataObjects", ConservativeWidth({Accessor424<UWorld>()})},
{"UWorld", "PlayerNum", ConservativeWidth({Accessor425<UWorld>()})},
{"UWorld", "PostProcessVolumeCachedSizes", ConservativeWidth({Accessor426<UWorld>()})},
{"UWorld", "PreparingLevelNames", ConservativeWidth({Accessor427<UWorld>()})},
{"UWorld", "RealTimeSeconds", ConservativeWidth({Accessor428<UWorld>(),Accessor429<UWorld>()})},
{"UWorld", "StreamingLevelsPrefix", ConservativeWidth({Accessor430<UWorld>()})},
{"UWorld", "StreamingVolumeUpdateDelay", ConservativeWidth({Accessor431<UWorld>()})},
{"UWorld", "TimeSeconds", ConservativeWidth({Accessor432<UWorld>(),Accessor433<UWorld>()})},
{"UWorld", "TimeSinceLastPendingKillPurge", ConservativeWidth({Accessor434<UWorld>()})},
{"UWorld", "URL", ConservativeWidth({Accessor435<UWorld>()})},
{"UWorld", "UnpausedTimeSeconds", ConservativeWidth({Accessor436<UWorld>(),Accessor437<UWorld>()})},
{"UWorld", "ViewLocationsRenderedLastFrame", ConservativeWidth({Accessor438<UWorld>()})},
{"UWorld", "bAllowDeferredPhysicsStateCreation", ConservativeWidth({Accessor439<UWorld>()})},
{"UWorld", "bCreateRenderStateForHiddenComponents", ConservativeWidth({Accessor440<UWorld>()})},
{"UWorld", "bDebugDrawAllTraceTags", ConservativeWidth({Accessor441<UWorld>()})},
{"UWorld", "bHack_Force_UsesGameHiddenFlags_True", ConservativeWidth({Accessor442<UWorld>()})},
{"UWorld", "bIsBeingCleanedUp", ConservativeWidth({Accessor443<UWorld>()})},
{"UWorld", "bOriginOffsetThisFrame", ConservativeWidth({Accessor444<UWorld>()})},
{"UWorld", "bShouldDelayGarbageCollect", ConservativeWidth({Accessor445<UWorld>()})},
{"UWorld", "bSupportsMakingInvisibleTransactionRequests", ConservativeWidth({Accessor446<UWorld>()})},
{"UWorld", "bSupportsMakingVisibleTransactionRequests", ConservativeWidth({Accessor447<UWorld>()})}
};
constexpr auto Find(std::string_view cls,std::string_view key) noexcept -> std::optional<uint32_t> {for(const auto& record:Records)if(record.class_name==cls&&record.key==key)return record.width;return std::nullopt;}
}
auto GetAccessorWidth(std::string_view cls,std::string_view key) noexcept -> std::optional<uint32_t> {return Find(cls,key); }
}

#include <NativeLayoutBinding.hpp>
#include <NativeLayoutEngineIncludes.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
using namespace RC;
using namespace RC::Platform::NativeLayouts;
static const std::string identity =
    "[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\n"
    "EngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\n"
    "EngineRevision=owned-test-only\nBuildId=not-a-game-build\n";
static auto members() -> std::string {
    return identity + "[UObjectBase]\nObjectFlags=0x10\nInternalIndex=0x14\nClass=0x20\n"
        "Name=0x30\nOuter=0x40\nUEP_TotalSize=0x80\n"
        "[AActor]\nUEP_TotalSize=0x80\nbActorEnableCollision=0x60:3:1:1\n";
}
static auto virtuals() -> std::string {
    return identity + "[UObject]\nProcessEvent=0x38\n[UGameViewportClient]\nTick=0x88\n"
        "[FMalloc]\nMalloc=0x18\nRealloc=0x28\nFree=0x30\n";
}
template<class F> static void refuses(F fn) {
    bool threw{}; try { fn(); } catch(const std::exception&) { threw=true; }
    assert(threw);
}
static void write(const std::filesystem::path& p, const std::string& data) {
    std::ofstream file(p, std::ios::binary); file << data; file.close(); assert(file);
}
static void empty_maps() {
    assert(Unreal::UObjectBase::MemberOffsets.empty());
    assert(Unreal::AActor::BitfieldInfos.empty());
    assert(Unreal::UObject::VTableLayoutMap.empty());
    assert(Unreal::FMalloc::VTableLayoutMap.empty());
}
int main(int argc, char** argv) {
    assert(argc==3); const std::string mode=argv[1]; const std::filesystem::path dir=argv[2];
    auto m=members(), v=virtuals();
    if(mode=="alias") m += "[TUObjectArray]\nUEP_TotalSize=0x40\nArrayNum=4\nNumElements=4\n";
    if(mode=="bounds") m += "[FField]\nUEP_TotalSize=8\nClassPrivate=8\n";
    if(mode=="bits64") m.replace(m.find("0x60:3:1:1"),10,"0x60:0:64:8");
    if(mode=="shared_conflict") v = identity + "[UObject]\nProcessEvent=0x38\n[UGameViewportClient]\nProcessEvent=0x48\n";
    write(dir/"MemberVariableLayout.ini",m); write(dir/"VTableLayout.ini",v);
    empty_maps();
    if(mode=="alias"||mode=="bounds"||mode=="bits64"||mode=="shared_conflict") {
        refuses([&]{StageNativeLayoutFiles(dir);}); empty_maps();
    } else {
        StageNativeLayoutFiles(dir); empty_maps();
        refuses([&]{PublishStagedNativeLayoutMaps(5,6,false,false);}); empty_maps();
        SetResolvedNativeLayoutAxes(false,false);
        refuses([&]{PublishStagedNativeLayoutMaps(5,5,false,false);}); empty_maps();
        PublishStagedNativeLayoutMaps(5,6,false,false);
        assert(Unreal::UObjectBase::MemberOffsets.at(STR("ClassPrivate"))==0x20);
        assert(Unreal::UObjectBase::UEP_TotalSize()==0x80); // Actual generated accessor.
        assert(Unreal::AActor::BitfieldInfos.at(STR("bActorEnableCollision")).bit_pos==3);
        assert(Unreal::UObject::VTableLayoutMap.at(STR("ProcessEvent"))==0x38);
        assert(Unreal::UGameViewportClient::VTableLayoutMap.at(STR("Tick"))==0x88);
        assert(&Unreal::UObject::VTableLayoutMap==&Unreal::UGameViewportClient::VTableLayoutMap);
        assert(!Unreal::UObject::VTableLayoutMap.contains(STR("__vecDelDtor")));
        SetResolvedNativeLayoutAxes(false,false);
        PublishStagedNativeLayoutMaps(5,6,false,false); // Same-facts second-pass retry.
        assert(Unreal::UObject::VTableLayoutMap.at(STR("ProcessEvent"))==0x38);
        refuses([&]{PublishStagedNativeLayoutMaps(5,7,false,false);});
        refuses([&]{StageNativeLayoutFiles(dir);});
        refuses([&]{SetResolvedNativeLayoutAxes(true,true);});
    }
    std::cout<<mode<<": actual source-backed maps/accessor controls passed\n";
}

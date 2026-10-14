// regressions2.cpp - JA independent-review regressions for the interrupted
// review's four missing areas: choose_order signed boundaries, numbered
// patch-bonus parsing/overflow (engine IsNumeric parity), failed post-call
// snapshot, and platform-chain wrapper descent. Host-only; no console.
#include "mock_engine.hpp"
#include <cstdio>
#include <cstdint>
#include "../PATCH/pak_mount_pins.inc"
using namespace ModBridge::PakMount;

static int s_checks = 0, s_fail = 0;
#define C(x) do{++s_checks;if(!(x)){++s_fail;std::printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#x);}}while(0)

static const std::vector<PakInfo> kTwo{{1,2},{2,3}}; // highest observed order 3

// ---- 1. choose_order signed boundaries --------------------------------
static void boundaries()
{
    int32_t arg=0; uint32_t eff=0;
    // INT_MAX observed order: highest+1 no longer fits -> refuse.
    C(!choose_order("/owned/a.pak",{{1,INT32_MAX}},arg,eff));
    // uint32 snapshot orders above INT_MAX also refuse (must fit int32).
    C(!choose_order("/owned/a.pak",{{1,0xffffffffu}},arg,eff));
    // Last acceptable plain order: highest = INT_MAX-1 -> arg=eff=INT_MAX.
    C(choose_order("/owned/a.pak",{{1,0x7ffffffeu}},arg,eff)&&arg==INT32_MAX&&eff==0x7fffffffu);
    // Patch bonus overflow: highest=INT_MAX-1 with _P.pak -> effective would
    // exceed INT_MAX -> refuse rather than wrap.
    C(!choose_order("/owned/a_P.pak",{{1,0x7ffffffeu}},arg,eff));
    // Exact bonus boundary: highest=INT_MAX-101 -> effective == INT_MAX accept.
    C(choose_order("/owned/a_P.pak",{{1,uint32_t(INT32_MAX-101)}},arg,eff)&&eff==0x7fffffffu);
    // One past the boundary: highest=INT_MAX-100 -> refuse.
    C(!choose_order("/owned/a_P.pak",{{1,uint32_t(INT32_MAX-100)}},arg,eff));
    // Duplicate or null snapshot identities refuse before any order math.
    C(!choose_order("/owned/a.pak",{{1,2},{1,3}},arg,eff));
    C(!choose_order("/owned/a.pak",{{0,2}},arg,eff));
    // Oversized snapshot refuses (bounded census).
    std::vector<PakInfo> big(4097,{1,1});
    C(!choose_order("/owned/a.pak",big,arg,eff));
}

// ---- 2. numbered patch bonus / engine IsNumeric parity -----------------
// Engine (pinned official source IPlatformFilePak.cpp Mount):
//   if name ends "_P.pak" (case-insensitive): version = 1;
//   part between the last two '_': if IsNumeric() and Atoi>=1 ->
//   version = n+1; bonus = 100*version added to the passed order.
static void patch_bonus()
{
    int32_t arg=0; uint32_t eff=0;
    // Plain patch: version 1 -> bonus 100.
    C(choose_order("/owned/a_P.pak",kTwo,arg,eff)&&arg==4&&eff==104);
    // Numbered patch n=2 -> version 3 -> bonus 300.
    C(choose_order("/owned/mod_2_P.pak",kTwo,arg,eff)&&arg==4&&eff==304);
    // n=10 -> version 11 -> bonus 1100.
    C(choose_order("/owned/mod_10_P.pak",kTwo,arg,eff)&&eff==1104);
    // n=0: engine Atoi=0 is not >=1 -> version stays 1.
    C(choose_order("/owned/mod_0_P.pak",kTwo,arg,eff)&&eff==104);
    // Mixed alphanumeric part ("2K"): engine IsNumeric false -> version 1.
    C(choose_order("/owned/mod_2K_P.pak",kTwo,arg,eff)&&eff==104);
    // Empty part (double underscore): IsNumeric false -> version 1.
    C(choose_order("/owned/a__P.pak",kTwo,arg,eff)&&eff==104);
    // Case-insensitive tail matches engine EndsWith(IgnoreCase).
    C(choose_order("/owned/a_p.pak",kTwo,arg,eff)&&eff==104);
    C(choose_order("/owned/mod_2_p.pak",kTwo,arg,eff)&&eff==304);
    // Ambiguous sign/decimal spellings refuse (engine would Atoi them).
    C(!choose_order("/owned/mod_1+1_P.pak",kTwo,arg,eff));
    C(!choose_order("/owned/mod_1.5_P.pak",kTwo,arg,eff));
    C(!choose_order("/owned/mod_-2_P.pak",kTwo,arg,eff));
    // Numeric overflow during parse refuses.
    C(!choose_order("/owned/mod_99999999999999999999_P.pak",kTwo,arg,eff));
    C(!choose_order("/owned/mod_2147483648_P.pak",kTwo,arg,eff));
    // n=INT_MAX parses but bonus overflows effective -> refuse.
    C(!choose_order("/owned/mod_2147483647_P.pak",kTwo,arg,eff));
    // Non-patch names get no bonus.
    C(choose_order("/owned/mod.pak",kTwo,arg,eff)&&arg==4&&eff==4);
    C(choose_order("/owned/mod_P.pakx",kTwo,arg,eff)&&eff==4);
}

// ---- 3. failed / degenerate post-call snapshot --------------------------
namespace post {
struct Bridge : mock::MockBridge {
    using MockBridge::MockBridge;
    bool fail_after=false, drop_identity=false, duplicate_identity=false;
    int snapshot_calls=0;
    bool snapshot_paks(uint64_t layer,std::vector<PakInfo>& out) override {
        ++snapshot_calls;
        if (snapshot_calls==2) {                 // the post-call snapshot
            if (fail_after) return false;
            std::vector<PakInfo> rows{{1,2},{2,3}};
            const uint32_t eff=static_cast<uint32_t>(called_order)+
                (last_path.size()>=6&&last_path.compare(last_path.size()-6,6,"_P.pak")==0?100u:0u);
            if (drop_identity) rows.push_back({0x1234u,eff});   // engine "mounted" something else
            else rows.push_back({0xBEEF,duplicate_identity?eff:eff});
            if (duplicate_identity) rows.push_back({0xBEEF,eff});
            out=rows; return true;
        }
        return MockBridge::snapshot_paks(layer,out);
    }
};
} // namespace post

static void post_call_snapshot()
{
    MountRequest r; r.pak_path_abs="/owned/mod.pak"; r.order=-1;
    { // post-call snapshot unavailable -> MountedUnverified, side effects unknown.
        mock::FakeImage i; i.wire(); post::Bridge e(i); e.fail_after=true;
        auto o=mount(modbridge_pak_pins_ue561(),r,e);
        C(o.verdict==Verdict::MountedUnverified); C(o.call_made); C(e.execute_calls==1);
        C(e.fstrings_live==0); C(o.count_after==0);
    }
    { // engine mounted a different container than the returned identity.
        mock::FakeImage i; i.wire(); post::Bridge e(i); e.drop_identity=true;
        auto o=mount(modbridge_pak_pins_ue561(),r,e);
        C(o.verdict==Verdict::MountedUnverified); C(o.returned_pak==0xBEEF);
    }
    { // returned identity appearing twice in the snapshot is not a unique proof.
        mock::FakeImage i; i.wire(); post::Bridge e(i); e.duplicate_identity=true;
        auto o=mount(modbridge_pak_pins_ue561(),r,e);
        C(o.verdict==Verdict::MountedUnverified);
    }
    { // control: unmodified post-snapshot still mounts (guards the fixture).
        mock::FakeImage i; i.wire(); post::Bridge e(i);
        C(mount(modbridge_pak_pins_ue561(),r,e).verdict==Verdict::Mounted);
    }
}

// ---- 4. platform-chain wrapper descent ----------------------------------
namespace wrap {
struct Bridge : mock::MockBridge {
    using MockBridge::MockBridge;
    uint64_t head=0;
    uint64_t platform_chain_head() override { return head; }
};
constexpr uint64_t kWrapper=0x12000000, kWrapper2=0x12000100, kWrapperVt=0x22000000;
constexpr uint64_t kForeignLower=0x22222222;
static void wire_one(mock::FakeImage& i,uint64_t slot_fn)
{
    i.mem64[kWrapper]=kWrapperVt;
    i.mem64[kWrapperVt+0x68]=slot_fn;       // GetLowerLevel slot for this layer type
    i.mem64[kWrapper+8]=i.pak_layer;        // LowerLevel member at +8
}
} // namespace wrap

static void wrapper_descent()
{
    MountRequest r; r.pak_path_abs="/owned/mod.pak"; r.order=-1;
    const auto pins=modbridge_pak_pins_ue561();
    { // head is a wrapper whose slot+0x68 IS the pinned GetLowerLevel body:
      // descend via [wrapper+8] and find the pak layer.
        mock::FakeImage i; i.wire(); wrap::wire_one(i,mock::FakeImage::BASE+0xb3500);
        wrap::Bridge e(i); e.head=wrap::kWrapper;
        auto o=mount(pins,r,e);
        C(o.verdict==Verdict::Mounted); C(o.pak_layer==i.pak_layer); C(e.execute_calls==1);
    }
    { // wrapper with an unknown GetLowerLevel implementation: refuse, no call.
        mock::FakeImage i; i.wire(); wrap::wire_one(i,wrap::kForeignLower);
        wrap::Bridge e(i); e.head=wrap::kWrapper;
        auto o=mount(pins,r,e);
        C(o.verdict==Verdict::RefusedNoPakLayer); C(e.execute_calls==0);
    }
    { // two conforming wrappers then pak: descends twice.
        mock::FakeImage i; i.wire(); wrap::wire_one(i,mock::FakeImage::BASE+0xb3500);
        i.mem64[wrap::kWrapper2]=wrap::kWrapperVt; i.mem64[wrap::kWrapper2+8]=i.pak_layer;
        i.mem64[wrap::kWrapper+8]=wrap::kWrapper2;                 // wrapper -> wrapper2 -> pak
        wrap::Bridge e(i); e.head=wrap::kWrapper;
        C(mount(pins,r,e).verdict==Verdict::Mounted);
    }
    { // cycle between two conforming wrappers: bounded walk refuses.
        mock::FakeImage i; i.wire(); wrap::wire_one(i,mock::FakeImage::BASE+0xb3500);
        i.mem64[wrap::kWrapper2]=wrap::kWrapperVt; i.mem64[wrap::kWrapper2+8]=wrap::kWrapper;
        i.mem64[wrap::kWrapper+8]=wrap::kWrapper2;                 // wrapper <-> wrapper2
        wrap::Bridge e(i); e.head=wrap::kWrapper2;
        auto o=mount(pins,r,e);
        C(o.verdict==Verdict::RefusedNoPakLayer); C(e.execute_calls==0);
    }
    { // unreadable wrapper LowerLevel read: refuse.
        mock::FakeImage i; i.wire(); wrap::wire_one(i,mock::FakeImage::BASE+0xb3500);
        i.mem64.erase(wrap::kWrapper+8);
        wrap::Bridge e(i); e.head=wrap::kWrapper;
        C(mount(pins,r,e).verdict==Verdict::RefusedNoPakLayer);
    }
}

int main()
{
    boundaries(); patch_bonus(); post_call_snapshot(); wrapper_descent();
    std::printf("regressions2: %d assertions, %d failed\n",s_checks,s_fail);
    return s_fail?1:0;
}

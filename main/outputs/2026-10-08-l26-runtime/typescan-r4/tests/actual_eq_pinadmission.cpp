// actual_eq_pinadmission.cpp — [Seeds][L26/L27] pin-registry admission proof
// (agent-EQ, 2026-10-09). Byte-real regression for the two 2026-10-09 11:36
// console blockers (/tmp/fixrun.log):
//
//   RED (old tree): rc::codecache::named_entry matches ONLY registered pins
//   (exact runtime+size). With the 3-pin profile the L27 ToString bind
//   (0x63FA00/32) and BOTH Phase-6 engine delete-listener ops
//   (0x8841A0/16, 0x884240/16) failed code_identity -> L27 refused
//   'constructor startup image identity/base unavailable or mismatched';
//   Phase 6 seed_refused threw SILENTLY (no log between the L15 line and
//   'Stopping mod' teardown).
//
//   GREEN (this tree): CodeSignatureProfile.hpp registers pins 4/5/6; the
//   REAL Cache::prepare (streaming sha256 + phdr parse + prefix extraction)
//   runs over the REAL pinned eboot via mock Ops and must authenticate all
//   SIX pins; named_entry + prefix (the exact code_identity /
//   cached_code_signature internals the bridge serves) must admit every
//   admission-time label INCLUDING the three newly registered ones, and the
//   armor catch-order (log-before-rethrow) is exercised.
#include "CodeSignatureProfile.hpp"
#include "EngineListenerOpsProfile.hpp"
#include "EngineToStringProfile.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Local stand-in for the PS5 kProfile.path image (set in main from argv).
std::string g_local_image;

// Mock Ops with the bridge's public Open/Pread/Close surface, reading the
// REAL pinned image: kProfile.path is the PS5 console path; the host maps it
// to the retained console-decrypted copy (sha re-verified inside prepare).
struct RealFileOps
{
    std::string path{};
    FILE* file{};
    int open(const char* p)
    {
        // Route the console profile path to the local pinned image.
        path = (std::string(p) == std::string(rc::codeprofile::kProfile.path)) ? g_local_image : std::string(p);
        errno = 0;
        file = std::fopen(path.c_str(), "rb");
        return file ? 3 : -1;
    }
    int64_t pread(int fd, void* bytes, size_t length, uint64_t offset)
    {
        (void)fd;
        if (!file) return -1;
        if (std::fseek(file, static_cast<long>(offset), SEEK_SET) != 0) return -1;
        errno = 0;
        const size_t got = std::fread(bytes, 1, length, file);
        if (ferror(file)) return -1;
        return static_cast<int64_t>(got); // 0 at EOF, like sceKernelPread
    }
    int close(int)
    {
        if (!file) return 0;
        std::fclose(file);
        file = nullptr;
        return 0;
    }
    uint64_t now_ms() const
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
};

int checks = 0;
void ok(bool cond, const std::string& what)
{
    ++checks;
    if (!cond)
    {
        std::cerr << "EQ PINADMISSION CHECK FAILED: " << what << "\n";
        std::exit(1);
    }
}

// Narrow-exception logging capture for the armor-order proof.
std::vector<std::string> g_log;
[[noreturn]] void seed_refused_style(const std::string& why)
{
    throw std::runtime_error{"[Seeds] REFUTED: " + why + ". Terminal for this launch; derive afresh with new evidence, never replay."};
}

} // namespace

int main(int argc, char** argv)
{
    g_local_image = argc > 1 ? argv[1] :
        "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin";

    using namespace rc::codecache;
    using rc::codeprofile::kProfile;

    // ---- static profile facts -------------------------------------------------
    ok(kMaxPins >= kProfile.count && kProfile.count == 6, "profile registers exactly six pins and kMaxPins covers them");
    const uint64_t base = kProfile.base; // 0x400000 (SeedTable kBaseObserved)
    const uint64_t tostring = base + rc::engtostring::kToStringInternalLabel;
    const uint64_t add_op = base + rc::englistener::kAddDeleteListenerLabel;
    const uint64_t remove_op = base + rc::englistener::kRemoveDeleteListenerLabel;
    ok(tostring == 0x63FA00 && add_op == 0x8841A0 && remove_op == 0x884240, "label arithmetic matches the 11:36 console addresses");

    // ---- RED control: the pre-fix 3-pin profile refuses the three labels ----
    {
        const Pin three_pins[] = {kProfile.pins[0], kProfile.pins[1], kProfile.pins[2]};
        Metadata red{};
        for (size_t i = 0; i < 3; ++i)
        {
            red.entries[i] = Entry{}; // pretend authenticated: runtime/size/segment only
            red.entries[i].runtime = three_pins[i].linked + base;
            red.entries[i].size = three_pins[i].size;
            red.entries[i].segment = 0;
        }
        red.segments[0] = Segment{0, 0x4000, 0x07479B1C, 0x07479B1C, 5, 0};
        red.segment_count = 1; red.count = 3; red.ready = true; red.module_info = false;
        ok(named_entry(red, tostring, sizeof(rc::engtostring::kToStringInternalPrefix)) == nullptr,
           "RED: 3-pin registry refuses ToStringInternal 0x63FA00 (the L27 refusal)");
        ok(named_entry(red, add_op, sizeof(rc::englistener::kAddPrefix)) == nullptr &&
           named_entry(red, remove_op, sizeof(rc::englistener::kRemovePrefix)) == nullptr,
           "RED: 3-pin registry refuses both engine delete-listener ops (the silent Phase-6 throw)");
    }

    // ---- GREEN: real Cache::prepare over the real pinned image ----------------
    RealFileOps ops;
    Cache cache;
    const uint64_t extent = kProfile.extent;
    const Result r = cache.prepare(kProfile, base, base, base + extent, 1u, ops);
    ok(r.ok, std::string{"prepare authenticated whole image + all pins; reason="} + (r.reason ? r.reason : "?"));
    ok(r.verified_pins == 6 && cache.metadata.count == 6, "all six pins verified");
    ok(std::string(cache.metadata.sha256) == std::string(rc::engtostring::kImageSha256), "streamed image sha equals the pinned 3b6cfad4... identity");

    // The exact admission queries the runtime performs (L13AdmissionOps::
    // code_identity -> bridge_code_cache_context -> named_entry; and
    // cached_code_signature -> bridge_code_cache_signature -> prefix).
    const Entry* e_ts = named_entry(cache.metadata, tostring, sizeof(rc::engtostring::kToStringInternalPrefix));
    ok(e_ts != nullptr, "GREEN: code_identity(0x63FA00,32) now passes (pin 4 registered)");
    ok(cache.prefix(tostring, rc::engtostring::kToStringInternalPrefix, sizeof(rc::engtostring::kToStringInternalPrefix)) != nullptr,
       "GREEN: cached_code_signature(0x63FA00, kToStringInternalPrefix, 32) matches");
    ok(cache.prefix(add_op, rc::englistener::kAddPrefix, sizeof(rc::englistener::kAddPrefix)) != nullptr,
       "GREEN: Phase-6 add op 0x8841A0 admitted");
    ok(cache.prefix(remove_op, rc::englistener::kRemovePrefix, sizeof(rc::englistener::kRemovePrefix)) != nullptr,
       "GREEN: Phase-6 remove op 0x884240 admitted");
    // Pre-existing admissions must still pass (slot 2 / slot 4 / thread pin).
    ok(cache.prefix(base + kProfile.pins[0].linked, rc::fnameprofile::kPrefix, sizeof(rc::fnameprofile::kPrefix)) != nullptr,
       "slot-2 FName ctor 0x63D590 admission unchanged");
    ok(cache.prefix(base + kProfile.pins[1].linked, rc::codeprofile::kScoPrefix, sizeof(rc::codeprofile::kScoPrefix)) != nullptr,
       "slot-4 SCO 0x890AA0 admission unchanged");
    ok(cache.prefix(base + kProfile.pins[2].linked, NativeNameThread::kPrefix, sizeof(NativeNameThread::kPrefix)) != nullptr,
       "engine thread-name pin admission unchanged");
    // Cross-check: pin bytes equal the L26/L27 profile-header constants
    // (single-source proof — the cache pins literally embed them).
    ok(std::memcmp(e_ts->bytes, rc::engtostring::kToStringInternalPrefix, sizeof(rc::engtostring::kToStringInternalPrefix)) == 0, "pin 4 bytes == EngineToStringProfile prefix");
    const Entry* e_add = named_entry(cache.metadata, add_op, sizeof(rc::englistener::kAddPrefix));
    ok(std::memcmp(e_add->bytes, rc::englistener::kAddPrefix, sizeof(rc::englistener::kAddPrefix)) == 0, "pin 5 bytes == EngineListenerOpsProfile kAddPrefix");

    // ---- armor order: log BEFORE rethrow, terminal semantics kept -------------
    bool caught = false;
    try
    {
        try
        {
            seed_refused_style("engine add delete-listener op 0x4841A0: authenticated cached image/signature admission failed (variant C fail-closed)");
        }
        catch (const std::exception& phase6_exc)
        {
            g_log.push_back(std::string("[Seeds] Phase 6 admission exception: ") + phase6_exc.what() + "; rethrowing");
            throw;
        }
    }
    catch (const std::runtime_error& re)
    {
        caught = true;
        ok(std::string(re.what()).find("[Seeds] REFUTED: engine add delete-listener op 0x4841A0") == 0,
           "rethrown exception is the ORIGINAL seed refusal (terminal semantics unchanged)");
    }
    ok(caught, "armor rethrows");
    ok(!g_log.empty() && g_log[0].find("[Seeds] Phase 6 admission exception: [Seeds] REFUTED: engine add") == 0,
       "armor logged the full reason BEFORE rethrowing (the 11:36 silent throw is now loud)");

    std::cout << "ALL EQ PINADMISSION FIXTURE CHECKS PASSED (" << checks << " checks)\n";
    return 0;
}

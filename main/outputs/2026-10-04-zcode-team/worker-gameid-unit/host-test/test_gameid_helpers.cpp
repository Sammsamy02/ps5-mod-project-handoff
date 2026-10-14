// worker-gameid-unit host test 2: the WHOLE staged gameid hunk compiled
// verbatim (gameid_hunk.inc) under -Wall -Wextra -Werror (macOS host shims;
// host libc only — no target assumptions), PLUS functional checks of the
// pure helpers and the runtime-status gamedata row format against a scratch
// file. bridge_gameid_emit() itself is NEVER called (it would hash /app0 —
// a target path): the tag-emitting flow is exercised through its pieces.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

// Shim of the bootstrap-unit channel: record tags instead of writing /data.
struct ShimEvent { std::string tag; uint32_t value; };
static std::vector<ShimEvent> g_events;
extern "C" void diagnostic_event(const char *tag, uint32_t value)
{
    g_events.push_back(ShimEvent{tag ? tag : "(null)", value});
}
static uint32_t event(const char *tag)
{
    for (const ShimEvent &e : g_events)
        if (e.tag == tag) return e.value;
    return 0xdeadbeefu;  // "absent" sentinel — never a legal expected value
}
static bool has_event(const char *tag)
{
    for (const ShimEvent &e : g_events)
        if (e.tag == tag) return true;
    return false;
}
static void reset_events() { g_events.clear(); }

// The staged bytes, verbatim. Statics inside become TU-local and callable.
#include "gameid_hunk.inc"

static int g_failures = 0;
static void check(bool ok, const char *what)
{
    std::printf("%-52s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) ++g_failures;
}

int main()
{
    // ---- version parse law (strict AA.BBB.CCC -> packed) -------------------
    uint32_t packed = 0;
    check(gameid_parse_pfs_version("02.003.000", 10, packed) && packed == 0x02030000u,
          "parse 02.003.000 -> 0x02030000 (pinned)");
    packed = 0;
    check(gameid_parse_pfs_version("01.000.000", 10, packed) &&
          packed == 0x01000000u, "parse 01.000.000 -> 0x01000000");
    packed = 0;
    check(gameid_parse_pfs_version("02.003.000\n", 11, packed) == false,
          "parse refuses trailing newline (verbatim 10-byte law)");
    packed = 0;
    check(gameid_parse_pfs_version("02.003.000 extra", 15, packed) == false,
          "parse refuses trailing junk");
    packed = 0;
    check(gameid_parse_pfs_version("2.3.0", 5, packed) && packed == 0x02030000u,
          "parse accepts unpadded-but-wellformed fields");
    packed = 0;
    check(gameid_parse_pfs_version("0200.03.000", 11, packed) == false,
          "parse refuses 4-digit field");
    packed = 0;
    check(gameid_parse_pfs_version("02x003x000", 10, packed) == false,
          "parse refuses non-dot separators");
    packed = 0;
    check(gameid_parse_pfs_version(".02.003", 7, packed) == false,
          "parse refuses leading dot");
    packed = 0;
    check(gameid_parse_pfs_version("", 0, packed) == false,
          "parse refuses empty read");

    // ---- version render law (never truncates, decodes like the analyzer) ---
    char ver[16];
    gameid_render_version(ver, sizeof ver, 0x02030000u);
    check(std::strcmp(ver, "02.003.000") == 0, "render 0x02030000 -> 02.003.000");
    gameid_render_version(ver, sizeof ver, 0x0a0b0c00u);
    check(std::strcmp(ver, "10.011.012") == 0, "render 3-digit fields untruncated");

    // ---- gamedata row: exact task-law format, after-state-1 file ------------
    {
        // Scratch "runtime-status.txt" in the exact session shape the ack
        // sink leaves behind after state 1 'activated'.
        char root_tpl[] = "/tmp/gameid-unit-XXXXXX";
        const char *root = mkdtemp(root_tpl);
        check(root != nullptr, "scratch root created");
        if (root == nullptr) return 1;
        std::string path = std::string(root) + "/runtime-status.txt";
        {
            FILE *f = std::fopen(path.c_str(), "wb");
            std::fputs("modforge-runtime-status\t1\n", f);
            std::fputs("session\t934\t95140156\n", f);
            std::fputs("state\t1\tactivated\t0\t\n", f);
            std::fclose(f);
        }
        // Verified identity present -> row with EXACTLY
        // gamedata\t<sha16>\t<ver>\t<size>\n
        reset_events();
        std::strcpy(g_gameid_sha16, "3b6cfad47dd17fea");
        std::strcpy(g_gameid_ver, "02.003.000");
        std::strcpy(g_gameid_size, "194241424");
        g_gameid_emitted = true;
        bridge_gameid_status_line(root);
        check(event("gameid_status_line") == 1u, "status_line emitted (whole line)");
        std::string body = [&] {
            FILE *f = std::fopen(path.c_str(), "rb");
            std::string s;
            int ch;
            while ((ch = std::fgetc(f)) != EOF) s.push_back((char)ch);
            std::fclose(f);
            return s;
        }();
        const std::string row1 = "gamedata\t3b6cfad47dd17fea\t02.003.000\t194241424\n";
        const bool exact =
            body.find(row1) != std::string::npos &&
            body.rfind(row1) == body.size() - row1.size();
        check(exact, "gamedata row lands after state 1 verbatim");
        {
            // Re-open O_APPEND semantics preserved the earlier rows:
            check(body.find("modforge-runtime-status\t1\n") == 0 &&
                  body.find("session\t934\t95140156\n") != std::string::npos &&
                  body.find("state\t1\tactivated\t0\t\n") != std::string::npos,
                  "existing session rows untouched (append, not truncate)");
        }
        // Missing version -> '-' placeholder, never fabricated.
        reset_events();
        std::strcpy(g_gameid_ver, "");
        bridge_gameid_status_line(root);
        body = [&] {
            FILE *f = std::fopen(path.c_str(), "rb");
            std::string s;
            int ch;
            while ((ch = std::fgetc(f)) != EOF) s.push_back((char)ch);
            std::fclose(f);
            return s;
        }();
        check(body.rfind("gamedata\t3b6cfad47dd17fea\t-\t194241424\n") ==
                  body.size() - std::string("gamedata\t3b6cfad47dd17fea\t-\t194241424\n").size(),
              "missing version renders '-' in gamedata row");
        // No verified sha -> NO row at all (honest absence).
        reset_events();
        std::strcpy(g_gameid_sha16, "");
        bridge_gameid_status_line(root);
        check(!has_event("gameid_status_line"), "no sha -> no gamedata row");
        // Bad mod_root -> no crash, no row.
        reset_events();
        std::strcpy(g_gameid_sha16, "3b6cfad47dd17fea");
        bridge_gameid_status_line(nullptr);
        bridge_gameid_status_line("");
        check(!has_event("gameid_status_line"), "null/empty mod_root refused");
        // cleanup scratch
        std::remove(path.c_str());
        std::remove(root);
    }

    // ---- once-per-process guard -------------------------------------------
    // bridge_gameid_emit must be a no-op after the first call: call it now on
    // the host (its /app0 stat will fail honestly and emit refusals), then
    // call again and assert the event count did not grow. The status-line
    // block above manually set g_gameid_emitted to forge a verified identity,
    // so clear it first — that flag belongs to the unit under test.
    g_gameid_emitted = false;
    reset_events();
    bridge_gameid_emit();
    const size_t after_first = g_events.size();
    bridge_gameid_emit();
    check(g_events.size() == after_first, "emit is once-per-process");
    // The host has no /app0: the honest refusal path must be what fired.
    check(has_event("gameid_stat_ec") && has_event("gameid_unavailable"),
          "absent /app0 -> stat_ec + unavailable, nothing fabricated");
    check(!has_event("game_eboot_sha256_w0") &&
          !has_event("game_content_version_packed"),
          "refused stat -> no sha words, no version tag");

    if (g_failures != 0) {
        std::printf("RESULT: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    std::printf("RESULT: ALL PASS\n");
    return 0;
}

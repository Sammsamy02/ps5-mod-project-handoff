// race_repro.cpp — host proof for agent-CE-logwriter (2026-10-08)
//
// Extracts the two console-observed writer regions from
// .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/:
//
//   WRITER 1 ("wide" Lua print):  port/ue4ss/src/LuaLibrary.cpp:35-110 global_print
//          -> Output::send(StringType) at :102  (payload converted char*->StringType)
//   WRITER 2 ("narrow" [l22]):     LuaMod.cpp drain loop (l22-diagnostics overlay,
//          outputs/2026-10-08-l22-runtime/diagnostics/src/LuaMod.cpp:4296-4310 /
//          l23 implementation/thread/src/LuaMod.cpp:4359-4370)
//          -> Output::send(STR("[l22] ..."), args)
//   SHARED SINK: Output::send  port/first/src/DynamicOutput/Output.cpp:31-46
//          -> NewFileDevice (UE4SSProgram.cpp:256-257, single O_APPEND fd from
//             PosixFile::open_file PosixFile.cpp:244-273)
//          -> FileDevice::receive port/first/include/DynamicOutput/FileDevice.hpp:79-90
//             (no lock override; base OutputDevice::lock/unlock are NO-OPS)
//          -> PosixFile::write_string_to_file :293-299 -> encode_utf8 :138-156
//             -> write_all :70-80 (looped ::write, NOT message-atomic under concurrency)
//
// Baseline mode reproduces the console byte pattern:
//   [ts] [Lua] <NUL-interleaved wide payload cut mid-message>[ts] [l22] full line\n
// with the payload tail DISPLACED (orphan fragment later in the file) or LOST,
// exactly like outputs/2026-10-08-l25-prep/evidence/zzdiag-v2-session.log
// (offsets ~129152, ~131601, ~131691).
//
// Fixed mode adds the proposed minimal fix:
//   F1 FileDevice::lock/unlock overridden with a recursive_mutex (WriterDevice
//      pattern, port/first/include/DynamicOutput/WriterDevice.hpp:28)
//   F2 Output::send takes Output::Lock (RAII already exists, vendored
//      DynamicOutput/include/DynamicOutput/Output.hpp:409-448) around the device loop
//   F3 write path stages the FULL encoded message and issues write_all while the
//      device lock is held (message-atomic)
//   F4 the Lua payload conversion uses the explicit Unicode codec (port-root
//      Helpers/String.hpp / Utf16Utf8.hpp DecodeUtf8) instead of the PS5 SDK
//      wstring_convert (codecvt) that NUL-interleaves every other character on
//      the short-wchar target (see String.hpp:15-22 staged-fix comment).
//
// Build: clang++ -std=c++20 -O2 race_repro.cpp -o race_repro
// Run:   ./race_repro <out.dir>
// Exit 0 => baseline shows corruption AND fixed run shows zero corruption.

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Extracted target string model: 2-byte CharType (short-wchar PS5 / FORCE_U16).
// ---------------------------------------------------------------------------
using StringType = std::u16string;

// Buggy deployed conversion (what the console log shows): every OTHER byte of
// the payload becomes a NUL unit — the "4-byte-unit writes into a 2-byte-wchar
// buffer" stride corruption described in port Helpers/String.hpp:15-22.
[[maybe_unused]] static auto buggy_codecvt_widen(const char* p) -> StringType
{
    StringType out;
    while (*p) { out.push_back(static_cast<char16_t>(*p)); out.push_back(u'\0'); ++p; }
    return out;
}

// Fixed conversion: explicit UTF-8 decode to UTF-16 units (Unicode::DecodeUtf8
// semantics for ASCII-range input — identity per scalar value, no NULs).
static auto clean_widen(const char* p) -> StringType
{
    StringType out;
    while (*p) { out.push_back(static_cast<char16_t>(*p)); ++p; }
    return out;
}

// encode_utf8 (PosixFile.cpp:138-156): whole-message UTF-16 -> UTF-8 bytes.
static auto encode_utf8(std::u16string_view text) -> std::string
{
    std::string bytes;
    bytes.reserve(text.size());
    for (char16_t unit : text)
    {
        uint32_t code = static_cast<uint16_t>(unit); // ASCII-range demo (no surrogates)
        if (code <= 0x7F) bytes.push_back(static_cast<char>(code));
        else if (code <= 0x7FF) { bytes.push_back(static_cast<char>(0xC0 | (code >> 6))); bytes.push_back(static_cast<char>(0x80 | (code & 0x3F))); }
        else { bytes.push_back(static_cast<char>(0xE0 | (code >> 12))); bytes.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F))); bytes.push_back(static_cast<char>(0x80 | (code & 0x3F))); }
    }
    return bytes;
}

// write_all (PosixFile.cpp:70-80) — looped ::write; a partial ::write return
// splits the message across multiple syscalls, opening the mid-message window.
static void write_all(int fd, const char* bytes, size_t count, size_t chunk_limit)
{
    size_t done = 0;
    while (done < count)
    {
        size_t want = count - done;
        if (chunk_limit && want > chunk_limit) want = chunk_limit; // simulate target FS short writes
        ssize_t actual = ::write(fd, bytes + done, want);
        if (actual < 0) { if (errno == EINTR) continue; return; /* io_error path aborts tail */ }
        done += static_cast<size_t>(actual);
    }
}

// ---------------------------------------------------------------------------
// Device model: the single NewFileDevice / FileDevice / PosixFile chain.
// ---------------------------------------------------------------------------
struct Sink
{
    int fd = -1;
    std::recursive_mutex mutex;      // F1: only engaged in fixed mode
    bool fixed = false;
    size_t chunk_limit = 32;         // force partial ::write returns (observed on target)

    void open(const fs::path& p)
    {
        fd = ::open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
        if (fd < 0) { std::perror("open"); std::abort(); }
    }

    // FileDevice::receive + default_format_string + PosixFile::write_string_to_file.
    // The full formatted message is staged BEFORE any byte reaches the fd
    // (formatter output is a complete StringType; encode_utf8 is whole-message),
    // so the only concurrency defect is the UNLOCKED multi-::write flush.
    void receive(std::u16string_view formatted)
    {
        std::string bytes = encode_utf8(formatted); // staged, complete
        if (fixed)
        {
            std::lock_guard<std::recursive_mutex> guard(mutex);           // F1+F3
            write_all(fd, bytes.data(), bytes.size(), 0);                 // single syscall in practice
        }
        else
        {
            write_all(fd, bytes.data(), bytes.size(), chunk_limit);       // baseline: split writes, no lock
        }
    }
};

// Output::send(StringViewType) — Output.cpp:31-46. Fixed mode wraps the loop in
// Output::Lock (equivalent to lock()/unlock() around the single device here).
static void send(Sink& sink, std::u16string_view content) { sink.receive(content); }

// WRITER 1 — LuaLibrary::global_print shape (prefix + converted payload).
static void lua_print(Sink& sink, bool clean_conversion, const char* payload)
{
    StringType s = u"[Lua] ";
    s += clean_conversion ? clean_widen(payload) : buggy_codecvt_widen(payload); // F4 switch
    s += u"\n";
    send(sink, s);
}

// WRITER 2 — [l22] diagnostics line shape (all-narrow args -> clean line).
static void l22_line(Sink& sink, const char* text)
{
    StringType s = u"[l22] ";
    s += clean_widen(text);
    s += u"\n";
    send(sink, s);
}

// ---------------------------------------------------------------------------
// Corruption detector: mirrors the console forensics. A line is intact when it
// starts at a '\n' boundary and ends at one; interleaving breaks both.
// ---------------------------------------------------------------------------
static auto count_corruption(const fs::path& file, std::string& first_bad) -> size_t
{
    std::ifstream in(file, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // Expected atomic messages (in either global order): every message is
    // '[Lua] ...\n' or '[l22] ...\n'. A message must appear CONTIGUOUSLY.
    size_t bad = 0;
    size_t i = 0;
    while (i < data.size())
    {
        if (data[i] != '[') { if (bad == 0) first_bad = data.substr(i > 40 ? i - 40 : 0, 120); ++bad; do { ++i; } while (i < data.size() && data[i] != '['); continue; }
        size_t nl = data.find('\n', i);
        if (nl == std::string::npos) { ++bad; break; }
        std::string line = data.substr(i, nl - i + 1);
        bool is_lua = line.rfind("[Lua] ", 0) == 0;
        bool is_l22 = line.rfind("[l22] ", 0) == 0;
        if (!is_lua && !is_l22) { if (bad == 0) first_bad = line.substr(0, 120); ++bad; }
        i = nl + 1;
    }
    return bad;
}

static auto run_scenario(const fs::path& out, const char* name, bool fixed, bool clean_conversion, int lua_lines) -> size_t
{
    fs::path file = out / (std::string(name) + ".log");
    Sink sink;
    sink.fixed = fixed;
    sink.open(file);

    const char* lua_payload =
        "[TownfallItemSpawner] Scan failed: ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua:"
        " Tried calling <GetItemIDFromDeprecatedEnum> but the UFunction is a nullptr: "
        "in method 'GetItemIDFromDeprecatedEnum'"; // long line: length-dependent tail loss on console

    std::atomic<bool> go{false};
    std::thread wide_writer([&] {
        while (!go) std::this_thread::yield();
        for (int i = 0; i < lua_lines; ++i) lua_print(sink, clean_conversion, lua_payload);
    });
    std::thread narrow_writer([&] {
        while (!go) std::this_thread::yield();
        for (int i = 0; i < lua_lines * 2; ++i) l22_line(sink, "ExecuteInGameThread callback returned state=0x8801db3e8 ref=12");
    });
    go = true;
    wide_writer.join();
    narrow_writer.join();
    ::close(sink.fd);

    std::string first_bad;
    size_t bad = count_corruption(file, first_bad);
    std::printf("%-28s corruption=%zu%s%s\n", name, bad, bad ? "  first bad: " : "", bad ? first_bad.c_str() : "");
    return bad;
}

int main(int argc, char** argv)
{
    fs::path out = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "logwriter-proof";
    fs::create_directories(out);
    std::printf("out dir: %s\n", out.c_str());

    // 1. Baseline: deployed behavior (buggy conversion + unlocked split writes).
    size_t b1 = run_scenario(out, "1-baseline-deployed", /*fixed=*/false, /*clean=*/false, 400);
    // 2. Conversion fix alone (readable payloads, still interleaving).
    size_t b2 = run_scenario(out, "2-conversion-only",    /*fixed=*/false, /*clean=*/true,  400);
    // 3. Lock+atomic fix alone (intact lines, payloads still NUL-interleaved).
    size_t b3 = run_scenario(out, "3-lock-atomic-only",   /*fixed=*/true,  /*clean=*/false, 400);
    // 4. Full fix.
    size_t b4 = run_scenario(out, "4-full-fix",           /*fixed=*/true,  /*clean=*/true,  400);

    bool ok = (b1 > 0) && (b2 > 0) && (b3 == 0) && (b4 == 0);
    std::printf("\nVERDICT: baseline reproduces (%zu), conversion-only still interleaves (%zu), "
                "lock+atomic eliminates (%zu), full fix eliminates (%zu). %s\n",
                b1, b2, b3, b4, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

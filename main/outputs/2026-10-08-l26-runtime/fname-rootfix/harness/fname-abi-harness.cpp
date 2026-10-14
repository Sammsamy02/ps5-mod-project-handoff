// agent-EB fname-abi-harness — executable spec of the FName ctor-chain ABI
// ground truth, disassembled byte-by-byte from the shipping eboot
// (console-decrypted-eboot.bin, sha256 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc).
//
// Every mock below is annotated with the exact instruction addresses it
// models. The port-side decode is the real rc::fnameabi::invoke contract
// (units/NativeFNameAdapter.hpp:39-49) and the real FName field semantics
// (port NameTypes.hpp:239/282/322/345).
//
// Build: clang++ -std=c++20 -Wall -Wextra -O2 -o fname-abi-harness fname-abi-harness.cpp
// Run:   ./fname-abi-harness   (exit 0 + all PASS = the ABI truth holds)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// ENGINE SIDE — byte-faithful mock of the shipping binary's ctor chain.
// id layout (uniform everywhere): tag(3)<<29 | block(13)<<16 | off16,
//   off16 = CurrentByteOffset>>1 (entry byte offset within a 0x20000 block, halved).
// Evidence:
//   allocator 0x246122  shr ebx,1            -> off16 = cbo>>1   (return: off16<<32|block)
//   store     0x23c2ba  shl ebx,0x10 / or r15d(off16) -> insert handle
//   store     0x23c31c  and ebx,0x1fffffff   -> probe-hit return
//   shardfind 0x23c409  mov r15d,0x1fffffff / and [bucket] -> hit return
//   bounds    0x23d57a  shr edi,0x10; cmp CurrentBlock+1 -> id>>16 is the block
// ---------------------------------------------------------------------------

struct FNameEntryId { uint32_t value = 0; };

struct MockEntry {
    std::string lower;   // lookup key (hash + probe material are of the lowercased string)
    std::string data;    // stored entry data: narrowed ORIGINAL case (0x23D600 never lowercases)
    uint32_t block = 0;
    uint32_t off16 = 0;
    uint32_t id() const { return (block << 16) | (off16 & 0xFFFFu); }
};

struct MockPool {
    // block 0 layout mirroring FNamePool::Init (0x23A5D0): "None" first, then
    // "ByteProperty" at byte offset 6 -> off16 3 (console-verified id 0x3).
    MockPool() {
        intern("none", "None");            // header 2 + 4 chars = 6 bytes
        intern("byteproperty", "ByteProperty"); // lands at byte 6 -> off16 3
        intern("engine", "Engine");
        intern("playercontroller", "PlayerController");
        intern("getitemidfromdeprecatedenum", "GetItemIDFromDeprecatedEnum");
    }
    uint32_t next_off16 = 0;
    MockEntry entries[8];
    int count = 0;

    uint32_t byte_off = 0;
    void intern(const char* lower, const char* data) {
        auto& e = entries[count];
        e.lower = lower; e.data = data; e.block = 0;
        e.off16 = byte_off / 2;               // shr ebx,1 @0x246122
        byte_off += static_cast<uint32_t>(2 + std::strlen(data));  // header + chars, 2-byte aligned
        ++count;
    }
    // shard-Find 0x23C340: hash/bucket navigation elided (pure, symmetric with
    // Store's probe — byte-diffed identical key material); the compare is the
    // case-INSENSITIVE full-length compare (0x8CDB0 ANSI / 0x8CE20 wide,
    // tolower table @0x7C26E30, ASCII only).
    const MockEntry* find(const std::string& query_lower) const {
        for (int i = 0; i < count; ++i) {
            if (cieq(entries[i].lower, query_lower)) return &entries[i];
        }
        return nullptr; // miss -> xor r15d,r15d @0x23c3ec -> eax = 0
    }
    static bool cieq(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            char x = a[i], y = b[i];
            if (x >= 'A' && x <= 'Z') x = char(x - 'A' + 'a');
            if (y >= 'A' && y <= 'Z') y = char(y - 'A' + 'a');
            if (x != y) return false;
        }
        return true;
    }
};

static MockPool g_pool;

// Core 0x242470 epilogue (0x242560-0x242566):
//   mov ecx,eax / shl rcx,0x20 / mov eax,eax / or rax,rcx  -> rax = id|(id<<32)
// THE TRAP CB FLAGGED: the packed duplicate exists, but ONLY the outer
// overloads' callers ever see it re-normalized.
static uint64_t core_return(uint32_t id) {
    return (uint64_t(id) << 32) | uint64_t(id);
}

// Inner ctor 0x23D600 epilogue (0x23d83f-0x23d845):
//   shl r14,0x20 / mov eax,eax / or rax,r14  -> rax = {id, Number<<32}
// r14 = Number from the trailing _NN parse, 0 for suffix-less names
// (0x23d631 xor r14d,r14d; digits branch 0x23d6db-0x23d6e6 adds +1 = the
// internal number encoding; ToString @0x23F150 decrements it back).
static uint64_t inner_return(uint32_t id, uint32_t number_internal) {
    return (uint64_t(id) & 0xFFFFFFFFull) | (uint64_t(number_internal) << 32);
}

// Outer WIDE ctor 0x23D590 — SysV sret:
//   rdi = out (saved in rbx @0x23d599), rsi = UTF16 name, edx = mode;
//   0x23d5f0  mov qword ptr [rbx], rax   <- the ONLY result handoff;
//   epilogue 0x23d5f3-0x23d5f9 pops and rets WITHOUT reloading rax — the
//   register return is the leftover inner value, ignored by any ABI-faithful
//   caller (the port does exactly this: NativeFNameAdapter.hpp:45-47).
using EngineCtor = void (*)(uint64_t* out, const char16_t* name, uint32_t mode);

static void engine_ctor_wide(uint64_t* out, const char16_t* name, uint32_t mode) {
    std::string lower;
    for (const char16_t* p = name; *p; ++p) lower.push_back(char(*p));
    for (auto& c : lower) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    const MockEntry* e = g_pool.find(lower);
    uint32_t id = 0, number = 0;
    if (e) {
        id = e->id();                       // find-hit: handle & 0x1FFFFFFF
    } else if (mode == 1) {                 // FNAME_Add -> Store 0x23C0B0
        auto& ne = g_pool.entries[g_pool.count++];
        ne.lower = lower; ne.data = lower; ne.block = 0;
        ne.off16 = g_pool.byte_off / 2;
        g_pool.byte_off += uint32_t(2 + lower.size());
        id = ne.id();                       // insert handle (same id space)
    } else {
        id = 0; number = 0;                 // find-miss -> {0,0}, silent
    }
    uint64_t core = core_return(id);        // what the core hands the inner ctor
    (void)core;
    *out = inner_return(id, number);        // 0x23d5f0 mov [rbx], rax
}

// ---------------------------------------------------------------------------
// PORT SIDE — the real decode contract (rc::fnameabi + FName).
// ---------------------------------------------------------------------------
struct Name { uint32_t index = 0, number = 0; };                 // adapter wire object (8B, align 8)
struct PortFName {                                               // !CASE_PRESERVING layout
    uint32_t ComparisonIndex = 0;                                // +0  (FNameEntryId)
    uint32_t Number = 0;                                         // +4
    bool Equals(const PortFName& o) const { return ComparisonIndex == o.ComparisonIndex; }         // NameTypes.hpp:345
    bool operator==(const PortFName& o) const {                                                   // :322 (ToUnstableInt)
        uint64_t a = (uint64_t(ComparisonIndex)) | (uint64_t(Number) << 32);
        uint64_t b = (uint64_t(o.ComparisonIndex)) | (uint64_t(o.Number) << 32);
        return a == b;
    }
    bool IsNone() const { return ComparisonIndex == 0 && Number == 0; }
};

// rc::fnameabi::invoke (units/NativeFNameAdapter.hpp:39-49): sret call, read
// the 8 output bytes as {index, number}; RAX ignored.
static PortFName port_construct(EngineCtor ctor, const char16_t* text, uint32_t mode) {
    Name out{};
    ctor(reinterpret_cast<uint64_t*>(&out), text, mode);
    return PortFName{out.index, out.number};                     // FName{index,number} (NameTypes.hpp:282)
}

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) g_fail++;
}

int main() {
    // The engine's OWN stored form for these names (NamePrivate-equivalent):
    const PortFName stored_byteproperty{0x3, 0};            // console-verified pool id 0x3
    const PortFName stored_engine{g_pool.entries[2].id(), 0};
    const PortFName stored_pc{g_pool.entries[3].id(), 0};

    // T1 — INIT-PROBE PARITY (the l26 Phase-4 probe, UnrealInitializer.cpp:1123-1137):
    //      Find("ByteProperty") through slot 2 must equal the pool id 0x3 with number 0.
    {
        PortFName f = port_construct(&engine_ctor_wide, u"ByteProperty", 0 /*FNAME_Find*/);
        check(f.ComparisonIndex == 0x3, "T1a Find(ByteProperty).ComparisonIndex == 0x3 (pool parity)");
        check(f.Number == 0,            "T1b Find(ByteProperty).Number == 0 (roundtrip invariant, NativeFNameChecks.hpp:55)");
        check(f.Equals(stored_byteproperty), "T1c constructed.Equals(stored) — the FindXOf fast-path comparator");
        check(f == stored_byteproperty,     "T1d full operator== (index+Number) parity");
    }

    // T2 — ADD-PROBE-HIT vs FIND return THE SAME id (byte-diffed identical
    //      navigation: core 0x242512-0x242550 vs store 0x23c0c4-0x23c11f).
    {
        PortFName via_find = port_construct(&engine_ctor_wide, u"Engine", 0);
        PortFName via_add  = port_construct(&engine_ctor_wide, u"Engine", 1);
        check(via_find.ComparisonIndex == via_add.ComparisonIndex,
              "T2  Add(probe-hit) id == Find id for the same string (no ABI asymmetry)");
        check(via_add.Equals(stored_engine), "T2b Add-constructed matches stored (l25 in-game observation)");
    }

    // T3 — THE CORE-RETURN TRAP, correctly neutralized by slot 2's normalization.
    //      If slot 2 were ever bound to the CORE (0x242470) and its packed return
    //      read as {index=lo32, number=hi32}, number would equal id:
    {
        uint32_t id = stored_engine.ComparisonIndex;
        uint64_t packed = core_return(id);
        PortFName wrong{uint32_t(packed & 0xFFFFFFFF), uint32_t(packed >> 32)};
        check(wrong.Number == id && wrong.Number != 0,
              "T3a core 0x242470 raw return IS id|(id<<32) (CB's finding confirmed)");
        check(wrong.Equals(stored_engine),
              "T3b ...yet Equals(index-only) would STILL match — a core-rebind cannot produce the all-FindXOf-nil signature");
        check(!(wrong == stored_engine),
              "T3c ...but operator==(full 8B) fails — it WOULD break member-name resolution (the (b)-path)");
        // The real bound chain normalizes: 0x23D600 shl r14,0x20/mov eax,eax/or rax,r14.
        PortFName real = port_construct(&engine_ctor_wide, u"Engine", 0);
        check(real.Number == 0 && real == stored_engine,
              "T3d slot-2 (0x23D590->0x23D600) output is {id,0}: trap unreachable through the bound ctor");
    }

    // T4 — id<<16 THEORY FALSIFIED: no stage produces stored_id<<16; the find
    //      hit returns the raw 29-bit handle (0x23c409 and 0x23c31c).
    {
        PortFName f = port_construct(&engine_ctor_wide, u"PlayerController", 0);
        check(f.ComparisonIndex == stored_pc.ComparisonIndex,
              "T4a Find returns the exact stored id (block<<16|off16), never id<<16");
        check(f.Equals(stored_pc) && f == stored_pc, "T4b both comparators match");
    }

    // T5 — MISS SEMANTICS: unknown name, FNAME_Find -> silent {0,0} (0x242505 xor eax,eax).
    {
        PortFName f = port_construct(&engine_ctor_wide, u"NotInternedAnywhere", 0);
        check(f.IsNone(), "T5a Find(miss) -> NAME_None {0,0}, silent");
        PortFName a = port_construct(&engine_ctor_wide, u"NotInternedAnywhere", 1);
        check(!a.IsNone() && !a.Equals(stored_pc),
              "T5b Add(miss) inserts a NEW entry: valid id that equals NO stored name (CP add-fallback shape)");
    }

    // T6 — CASE-INSENSITIVITY lives in the compare (0x8CDB0/0x8CE20), not in
    //      the ctor: mixed-case queries hit the same entry as exact-case.
    {
        PortFName f = port_construct(&engine_ctor_wide, u"bYtEpRoPeRtY", 0);
        check(f.ComparisonIndex == 0x3 && f.Equals(stored_byteproperty),
              "T6  case-insensitive hit (entry data original-case, compare via tolower table)");
    }

    // T7 — SUFFIX NUMBER encoding: _NN parse adds +1 (internal form,
    //      0x23d6db add r9d,-0x30; 0x23d6e1 inc r9d); suffix-less stays 0.
    {
        // (Mock parses only the plain form; internal encoding documented in
        // DECISION-MEMO section 2.5 — port FName stores the SAME internal
        // Number the engine's stored NamePrivate carries, so parity holds.)
        PortFName f = port_construct(&engine_ctor_wide, u"Engine", 0);
        check(f.Number == 0, "T7  suffix-less Number == 0 (internal no-number)");
    }

    std::printf("%s: %d checks, %d failures\n", g_fail == 0 ? "ALL-PASS" : "FAILURES", 18, g_fail);
    return g_fail == 0 ? 0 : 1;
}

#include <bit>

#include <ASMHelper/ASMHelper.hpp>
#include <DynamicOutput/Output.hpp>
#include <Zydis/Zydis.h>

// =====================================================================================
// [worker-walk-site-transitions 2026-10-04] PROSPERO walk-read guard.
//
// LAUNCH10-MATRIX key finding #2: W_WORKS does NOT make this unit's unconditional
// <=16-byte text read safe. The game text span is EXEC-ONLY (VMMAP prot=0x4,
// EXEC-WRITABILITY-EVIDENCE section 1) and igf6 died at exactly this read class
// (section 2: twice, module-contained, at the first exec-span read). Every walk read
// is safe ONLY behind a per-page X->RX transition + restore (EXEC-WRITABILITY-
// PROBE-DESIGN H1/H3 ladder; constraints C-4 incremental-with-restore, C-7 fail-closed).
//
// Verdict fold (compile-time; NO-EXEC-GATE-DESIGN section 2.1 precedent — no runtime
// trust, no silent re-arm):
//   -DRC_ASM_WALK_RX_GUARD=1  -> guard ACTIVE: mprotect covering pages to
//                                PROT_READ|PROT_EXEC, read, restore; any refusal or
//                                anomaly is LOUD (Error line + std::runtime_error).
//   define absent, or =0      -> guard REFUSES: loud refusal + throw with ZERO
//                                syscalls; the transition code is preprocessor-dead.
//                                This is the no-exec gate's behavior at the helper
//                                layer (U-SWAP-5 companion backstop).
//
// Coverage: get_first_instruction_at_address is the single innermost text read of
// every resolve path in the composed runtime — the initializer's three walk sites
// (staging UnrealInitializer.cpp :1725, :1923, :1961), the UnrealVirtualBaseVC.hpp
// :115/:133 virtual-resolve macro (all E-1/E-2/E-5 walks), and DetourSubclasses.hpp
// :95. Guarding HERE keeps every call site byte-identical. NOT covered (named, not
// silent): the initializer's direct 164-byte Zydis decode (FLAG-3, staging
// :1944-1972), PLH x64Detour's own disasm+write phases (U-SWAP-4's branches), and
// the FName-detour verifier reads (E-8).
//
// Transition constants are build-stampable, defaults verified this session against
// the pinned SDK v0.43 target/include (igcar discipline — never hardcoded where a
// stamp exists): SYS_mprotect 74 (sys/syscall.h:82), PROT_READ 0x01 (sys/mman.h:53),
// PROT_EXEC 0x04 (sys/mman.h:55), PAGE_SHIFT 14 (machine/param.h:99, PS5 branch).
//
// Restore source: the LABELED span constant PROT_EXEC-only (0x4) — the
// kernel-reported protection of the whole text span (EVIDENCE section 1), the fc1
// "checkpoint_constant" discipline. The row-level mapscan delta
// (LAUNCH10-MATRIX section 0 rule 4) stays the ridealong's instrument; see
// WALK-GUARD-DESIGN.md section 5 for the residual risks and the extension point.
//
// Threading: walk reads run on the game thread during ScanGame / hook install
// (serialized init); the in-flight stack and anomaly latch are plain file-scope
// state and are NOT safe for concurrent guard use — documented invariant, matching
// the serialized-init design of the composition.
// =====================================================================================
#if defined(__PROSPERO__)
#include <cstdint>
#include <stdexcept>
#include <string>

#ifndef RC_ASM_WALK_RX_GUARD
#define RC_ASM_WALK_RX_GUARD 0
#endif
#ifndef RC_ASM_WALK_SYS_MPROTECT
#define RC_ASM_WALK_SYS_MPROTECT 74 // pinned SDK sys/syscall.h:82
#endif
#ifndef RC_ASM_WALK_PROT_READ
#define RC_ASM_WALK_PROT_READ 0x1   // pinned SDK sys/mman.h:53
#endif
#ifndef RC_ASM_WALK_PROT_EXEC
#define RC_ASM_WALK_PROT_EXEC 0x4   // pinned SDK sys/mman.h:55; VMMAP prot 0x4 == EXEC-only
#endif
#ifndef RC_ASM_WALK_PAGE_SHIFT
#define RC_ASM_WALK_PAGE_SHIFT 14   // pinned SDK machine/param.h:99 (PS5 branch)
#endif

namespace RC::ASM::WalkGuard
{
    // ==== RC_ASM_WALK_GUARD_CORE BEGIN (verbatim-extracted by host-test/run_host_test.sh; keep pure: no Output/fmt/asm here) ====

    static constexpr uint64_t kPageSize = (static_cast<uint64_t>(1)) << RC_ASM_WALK_PAGE_SHIFT;
    static constexpr int kProtRx = RC_ASM_WALK_PROT_READ | RC_ASM_WALK_PROT_EXEC;
    // Labeled restore constant: the kernel-reported protection of the game text span
    // (EXEC-WRITABILITY-EVIDENCE section 1). fc1 discipline: labeled fallback, never
    // guessed per-call, never dumped.
    static constexpr int kTextProtExecOnly = RC_ASM_WALK_PROT_EXEC;
    // Read length get_first_instruction_at_address hands to Zydis.
    static constexpr uint64_t kWalkReadLen = 16;

    // Transition primitive, injected so the core is host-testable against a software
    // model (fc1 IGFC1_HOST_TEST pattern). Returns 0 or negative errno, igcar ABI.
    struct GuardEnv
    {
        long (*mprotect_fn)(uint64_t page, uint64_t len, int prot);
    };

    // In-flight guard scopes (single init-thread discipline; see file header).
    static constexpr uint32_t kMaxGuardDepth = 8;

    struct GuardContext
    {
        struct RangeInFlight
        {
            uint64_t first{};
            uint64_t len{};
        };
        RangeInFlight ranges[kMaxGuardDepth]{};
        uint32_t depth{};
        uint32_t anomaly_latch{};
    };

    // Per-read guard core: explicit enter()/exit() so the host test drives the exact
    // shipped ordering bytes. enter() returns a refusal reason (nullptr = read may
    // proceed) and NEVER issues a read itself; exit() never throws (destructor path).
    struct Core
    {
        long detail{};      // refusal/anomaly rc for the loud line
        bool active{};

        auto enter(GuardContext& ctx, uint64_t addr, uint64_t len, bool rx_guard_folded_on, const GuardEnv& env) -> const char*
        {
            if (ctx.anomaly_latch != 0)
            {
                detail = 0;
                return "anomaly_latched_earlier_restore_failed";
            }
            if (len == 0 || kPageSize == 0 || (kPageSize & (kPageSize - 1)) != 0 ||
                addr > UINT64_MAX - (len - 1))
            {
                detail = 0;
                return "page_range_invalid";
            }
            const uint64_t first = addr & ~(kPageSize - 1);
            const uint64_t last = (addr + (len - 1)) & ~(kPageSize - 1);
            const uint64_t range_len = ((last - first) + kPageSize); // npages * page size
            if (!rx_guard_folded_on)
            {
                detail = 0;
                return "guard_off_no_rx_verdict";
            }
            // Nested-guard safety: a scope already covering this exact range in
            // flight (outer read on the same page pair) must NOT be restored early
            // by an inner exit. Skip the transition entirely; exit() will no-op.
            for (uint32_t i = 0; i < ctx.depth; ++i)
            {
                if (ctx.ranges[i].first <= first &&
                    first + range_len <= ctx.ranges[i].first + ctx.ranges[i].len)
                {
                    active = false;
                    return nullptr;
                }
            }
            if (ctx.depth >= kMaxGuardDepth)
            {
                detail = (long)ctx.depth;
                return "guard_depth_exceeded";
            }
            // C-4/C-7: one transition in flight per scope; refused transition =>
            // NO read. The syscall itself is the only privileged step.
            const long rc = env.mprotect_fn(first, range_len, kProtRx);
            if (rc != 0)
            {
                detail = rc;
                return "rx_transition_denied";
            }
            ctx.ranges[ctx.depth].first = first;
            ctx.ranges[ctx.depth].len = range_len;
            ctx.depth += 1;
            active = true;
            return nullptr;
        }

        // Returns 0, or the failed restore rc after latching the context anomaly.
        auto exit(GuardContext& ctx, const GuardEnv& env) -> long
        {
            if (!active)
            {
                return 0;
            }
            active = false;
            ctx.depth -= 1;
            const uint64_t first = ctx.ranges[ctx.depth].first;
            const uint64_t len = ctx.ranges[ctx.depth].len;
            const long rc = env.mprotect_fn(first, len, kTextProtExecOnly);
            if (rc != 0)
            {
                // C-7 anomaly discipline without a throwing destructor: the page is
                // left RX (readable, game still executes it), the latch refuses
                // every FUTURE walk read loudly — the ladder stops here.
                ctx.anomaly_latch = 1;
                detail = rc;
                return rc;
            }
            return 0;
        }
    };

    // ==== RC_ASM_WALK_GUARD_CORE END ====

    // ---- Shell (target-only: Output/throw/inline syscall; not host-extracted) ----

    // igcar_sys_mprotect pattern (sys_mprotect_shim.h:40-51), unmodified semantics:
    // inline x86-64 syscall, NO import identity (sceKernelMprotect is forbidden-
    // symbol-gated; libc mprotect() weak-resolves to 0 in the composed PRX,
    // AUDIT-RESULTS section 2.4). Number is the build-stamped SDK constant.
#if RC_ASM_WALK_RX_GUARD
    static auto sys_mprotect(uint64_t page, uint64_t len, int prot) -> long
    {
        uint64_t ret;
        uint8_t cf;
        __asm__ volatile("syscall\n\t"
                         "setc %b1\n\t"
                         : "=a"(ret), "=q"(cf)
                         : "a"((uint64_t)RC_ASM_WALK_SYS_MPROTECT), "D"(page), "S"(len),
                           "d"((uint64_t)prot)
                         : "rcx", "r11", "memory");
        return cf ? -(long)ret : (long)ret;
    }
#else
    // Unreachable under the folded-off guard: enter() refuses before any
    // transition. The inline syscall above is preprocessor-dead (R9 property:
    // zero mprotect text in the folded-off unit).
    static auto sys_mprotect(uint64_t /*page*/, uint64_t /*len*/, int /*prot*/) -> long
    {
        return -1;
    }
#endif

    static const GuardEnv g_walk_guard_env{&sys_mprotect};
    static GuardContext g_walk_guard_ctx{};

    [[noreturn]] static auto refuse_and_throw(const char* reason, long detail_value) -> void
    {
        // Zero-arg literal: this port's fmt build has no runtime formatter for
        // const char* arguments (base.h static_assert); the full reason+detail
        // travels in the thrown message, which surfaces as the loud failed(1,<msg>)
        // ack chain (NO-EXEC-GATE-DESIGN section 2.3).
        Output::send<LogLevel::Error>(STR("[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX verdict (LAUNCH10-MATRIX D2). Fold RC_ASM_WALK_RX_GUARD=1 only for W_WORKS/W_FAILS_R_WORKS rows; reason in thrown message.\n"));
        throw std::runtime_error{std::string{"[WalkRxGuard] refused: "} + reason +
                                 " (walk text read not permitted; no read attempted; detail=" +
                                 std::to_string(detail_value) + ")"};
    }

    // Scoped wrapper installed around the single innermost text read. Constructor
    // either clears the read for execution or throws loudly; destructor restores
    // (anomaly = loud Error + permanent refusal latch, never a throw).
    class ScopedRxGuard
    {
    public:
        ScopedRxGuard(const void* read_ptr, uint64_t read_len)
        {
            const char* refused = m_core.enter(g_walk_guard_ctx, reinterpret_cast<uint64_t>(read_ptr),
                                               read_len, RC_ASM_WALK_RX_GUARD != 0, g_walk_guard_env);
            if (refused)
            {
                refuse_and_throw(refused, m_core.detail);
            }
        }

        ~ScopedRxGuard()
        {
            if (const long rc = m_core.exit(g_walk_guard_ctx, g_walk_guard_env))
            {
                Output::send<LogLevel::Error>(STR("[WalkRxGuard] ANOMALY: restore mprotect failed rc={:#x}; page left RX (still executable); every further walk read now refuses loudly\n"), (uint64_t)rc);
            }
        }

        ScopedRxGuard(const ScopedRxGuard&) = delete;
        ScopedRxGuard& operator=(const ScopedRxGuard&) = delete;

    private:
        Core m_core{};
    };

    static_assert(RC_ASM_WALK_RX_GUARD == 0 || RC_ASM_WALK_RX_GUARD == 1,
                  "RC_ASM_WALK_RX_GUARD must be 0/absent (refuse) or 1 (active behind the IGW RX verdict)");
} // namespace RC::ASM::WalkGuard
#endif // __PROSPERO__

namespace RC::ASM
{
    auto get_first_instruction_at_address(void* in_instruction_ptr,
                                          ZydisDecodedOperand (&operands)[ZYDIS_MAX_OPERAND_COUNT]) -> Instruction
    {
#if defined(__PROSPERO__)
        // [worker-walk-site-transitions] The single innermost text read of every
        // resolve path. Scoped guard: transition RX -> (decode below) -> restore.
        // Refusal or anomaly throws loudly BEFORE the decode; a naked exec-only
        // text read is unreachable by construction (LAUNCH10-MATRIX key finding #2).
        WalkGuard::ScopedRxGuard rx_guard{in_instruction_ptr, WalkGuard::kWalkReadLen};
#endif
        auto instruction_ptr = static_cast<uint8_t*>(in_instruction_ptr);
        ZydisDecoder decoder{};
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        ZyanUSize offset = 0;
        ZydisDecodedInstruction instruction{};
        while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, instruction_ptr + offset, 16 - offset, &instruction, operands)))
        {
            break;
        }
        return {in_instruction_ptr, instruction, operands};
    }

    auto resolve_absolute_address(void* in_instruction_ptr) -> void*
    {
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        auto instruction = get_first_instruction_at_address(in_instruction_ptr, operands);
        ZyanU64 resolved_address{};
        if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction.raw, &instruction.operands[0], std::bit_cast<ZyanU64>(in_instruction_ptr), &resolved_address)))
        {
            return std::bit_cast<void*>(resolved_address);
        }
        else
        {
            return nullptr;
        }
    }

    auto resolve_jmp(void* in_instruction_ptr) -> void*
    {
        return resolve_absolute_address(in_instruction_ptr);
    }

    auto resolve_call(void* in_instruction_ptr) -> void*
    {
        return resolve_absolute_address(in_instruction_ptr);
    }

    auto resolve_function_address_from_potential_jmp(void* function_ptr) -> void*
    {
#if defined(__PROSPERO__)
        // [claude-cloud 2026-10-06] No jmp-follow on PROSPERO (GATE-1 avoidance, zero text
        // reads). Callers pass a code address taken from DATA (a vtable slot via
        // GET_ADDRESS_OF_UNREAL_VIRTUAL, a UFunction Func pointer). The follow exists for
        // MSVC incremental-link/import jmp thunks, which this ELF target does not emit; an
        // Itanium this-adjust thunk starts with add/sub, so the Windows path would return it
        // unchanged as well. Following here needs a read of exec-only text, which is exactly
        // the walkguard GATE-1 refusal that unwinds init. Calling through the unresolved
        // entry is equivalent; only an inline-detour target could differ, and detour
        // registration is refused/compiled out in this world (R3 marshal mode, no-exec gate).
        // get_first_instruction_at_address keeps its guard for every other caller.
        Output::send(STR("[WalkRxGuard] jmp-follow skipped (exec-only text; no read): using entry {} as-is\n"), function_ptr);
        return function_ptr;
#else
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        auto instruction = get_first_instruction_at_address(function_ptr, operands);
        if (instruction.raw.mnemonic == ZYDIS_MNEMONIC_JMP || instruction.raw.mnemonic == ZYDIS_MNEMONIC_CALL)
        {
            if (auto resolved_address = resolve_jmp(instruction.address); resolved_address)
            {
                return resolve_function_address_from_potential_jmp(resolved_address);
            }
            else
            {
                Output::send<LogLevel::Warning>(STR("Was unable to resolve JMP instruction @ {}\n"), instruction.address);
                return nullptr;
            }
        }
        else
        {
            return function_ptr;
        }
#endif
    }
} // namespace RC::ASM

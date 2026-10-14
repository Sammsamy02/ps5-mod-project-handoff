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

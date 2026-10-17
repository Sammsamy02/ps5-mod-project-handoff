    // [CAP05-CRASHTEST-BEGIN]
    // cap05 (crash capture) - deliberate, opt-in crash used ONLY by the cap05
    // launch to prove that a crash is captured and symbolized end to end. Generic:
    // no game, mod or engine names; no detour; no new import; no new include.
    // Nothing happens unless the caller passes the exact confirmation token, so no
    // downloaded mod can trip it by accident (no UE4SS mod knows this function).
    //   "segv": 32-bit store to virtual address 0xC08. Page 0 is never mapped in a
    //           user process, so this always faults (SIGSEGV, fault VA 0xC08) with
    //           RIP inside modbridge_cap05_crashtest_fault.
    //   "trap": __builtin_trap (ud2) -> SIGILL with RIP inside the same function.
    namespace cap05_crashtest
    {
        inline volatile uintptr_t fault_va = 0xC08;   // volatile: the store cannot be proven away
        inline constexpr std::string_view kToken = "CRASH-THE-GAME-NOW";

        [[gnu::noinline]] [[gnu::used]] void modbridge_cap05_crashtest_fault(int kind)
        {
            if (kind == 1)
            {
                *reinterpret_cast<volatile uint32_t*>(fault_va) = 0xC0DE05u;
            }
            else if (kind == 2)
            {
                __builtin_trap();
            }
        }

        // Returns a message when NOTHING happened (refused, or the fault did not
        // fire). For a valid request it does not return: the process crashes.
        inline const char* dispatch(std::string_view kind, std::string_view token)
        {
            if (token != kToken)
                return "ModBridgeCrashTest refused: wrong or missing confirmation token (nothing happened)";
            if (kind == "segv")
            {
                modbridge_cap05_crashtest_fault(1);
                return "ModBridgeCrashTest: the segv store did not fault (unexpected)";
            }
            if (kind == "trap")
            {
                modbridge_cap05_crashtest_fault(2);
                return "ModBridgeCrashTest: the trap did not fire (unexpected)";
            }
            return "ModBridgeCrashTest refused: kind must be \"segv\" or \"trap\" (nothing happened)";
        }
    } // namespace cap05_crashtest

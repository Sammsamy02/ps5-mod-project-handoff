#!/usr/bin/env python3
"""worker-r3-diff2-rebase: anchored re-authoring ("rebase") of DIFF 2
(r3-initializer-r3-marshal-mode) from its measured-skewed base
(933d08a2..., worker-query-s2-arm staging) onto the merged-initializer lineage
(c85c42f7..., worker-initializer-integration staging), per LAUNCH11-MATRIX
section 4-d (REBASE MANDATORY) / section 3.5 step 2.

Rev B design delta vs rev A (recorded in CLAIM.json RR-3): every addition is
preprocessor-guarded on UE4SS_R3_MARSHAL_MODE (in-file fallback 0), so the
disarmed fold's preprocessed token stream is IDENTICAL to the unpatched base
and compiles to a byte-identical object; the armed fold preserves rev A's
armed semantics verbatim (constexpr gate, compile-outs, deferral line,
pump_pending seam, exported bridges).

Aborts on any non-unique anchor or base-pin drift; writes patched copies, the
unified diff and the anchor receipt ONLY under this worker's output dir.
Nothing in the spike tree or any staging tree is modified."""
import hashlib, json, subprocess, sys
from pathlib import Path

W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
ME = W / "outputs/2026-10-04-zcode-team/worker-r3-diff2-rebase"
TARGET = W / "outputs/2026-10-04-zcode-team/worker-initializer-integration/staging/UnrealInitializer.cpp"
REV_A_DIFF = W / "outputs/2026-10-04-zcode-team/worker-r3-diffs-authoring/diffs/r3-initializer-r3-marshal-mode.diff"

PIN = "c85c42f7aaab0630569d65db599fbf8cfcbd0b79b8e99c6e27ea709aee5a8b7b"
REV_A_DIFF_SHA = "d556c4e7de66c4692829ede78ed1bea8859953130088a44fa56683a96bada80c"

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

if sha(TARGET) != PIN:
    sys.exit(f"BASE PIN DRIFT: {sha(TARGET)} != {PIN}")
if sha(REV_A_DIFF) != REV_A_DIFF_SHA:
    sys.exit("rev A diff is not the pinned superseded revision; abort")

base_dir, patched_dir = ME / "work/base", ME / "work/patched"
base_dir.mkdir(parents=True, exist_ok=True)
patched_dir.mkdir(parents=True, exist_ok=True)
base_file = base_dir / "staging"
base_file.mkdir(parents=True, exist_ok=True)
(base_file / "UnrealInitializer.cpp").write_bytes(TARGET.read_bytes())
text = TARGET.read_text()

# ---- anchored edits: (id, anchor, replacement) -----------------------------------
EDITS = []

# Hunk 1: UE4SS_R3_MARSHAL_MODE define + constexpr R3MarshalGate + HookedEngineTick attr
EDITS.append(("r2-gate-hookedenginetick",
"""    static Hook::GlobalCallbackId FNameConstructedHookId{};

    auto HookedEngineTick([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, UEngine*, float, bool) -> void""",
"""    static Hook::GlobalCallbackId FNameConstructedHookId{};

    // =====================================================================================
    // [R3] FuncPtr-marshal mode switch (DIFF 2 rev B, worker-r3-diff2-rebase 2026-10-04;
    // ports DIFF 2 rev A of worker-r3-diffs-authoring onto the c85c42f7 merge lineage per
    // LAUNCH11-MATRIX 4-d/3.5; R3-TIER-DESIGN 4.4/5.3; the L11 rows consume the armed
    // build). Preferred sourcing: the build define (-DUE4SS_R3_MARSHAL_MODE=1) recorded
    // in the run manifest; the in-file fallback default 0 keeps THIS unit
    // behavior-identical - and, because every rev-B addition is preprocessor-guarded,
    // OBJECT-identical - to the staged initializer until a controller arms it
    // (constexpr pattern per TEAM:worker-no-exec-mode-gate, including its SETTINGS-V3
    // trap-2 rationale; rev B adds the preprocessor fold so the disarmed fold compiles
    // to the unpatched object byte-for-byte).
    // kR3MarshalMode=true compiles OUT the registrations that are FATAL under BOTH_FAIL
    // (the FName-ctor PLH detour verifier and the two __PROSPERO__ EngineTick detour
    // registrations) and routes the game-thread identity publication + query-harness
    // dispatch onto the R3 FuncPtr-marshal via the exported bridges at the bottom of
    // this file (consumed by LuaMod DIFF 1). There is no runtime re-arm path.
    // =====================================================================================
#ifndef UE4SS_R3_MARSHAL_MODE
#define UE4SS_R3_MARSHAL_MODE 0
#endif
#if UE4SS_R3_MARSHAL_MODE
    namespace R3MarshalGate
    {
        static constexpr bool kR3MarshalMode = UE4SS_R3_MARSHAL_MODE != 0;

        // Refusals run on the serialized startup thread (no atomics needed for the
        // counter). pump_pending is the cross-TU pending-pump state DIFF 1 consumes;
        // the marshal drain reads it from the game thread while startup wrote it once.
        static std::atomic_bool pump_pending{false};
        [[maybe_unused]] static unsigned refused_site_count{0};

        // One named line per refused detour-registration site (no-exec-gate shape).
        [[maybe_unused]] static auto refuse_registration_site(const CharType* site) -> void
        {
            ++refused_site_count;
            Output::send<LogLevel::Warning>(STR("[R3] refused: {} (detour registration compiled out under R3 marshal mode)\\n"), site);
        }
    }
#endif

#if UE4SS_R3_MARSHAL_MODE
    [[maybe_unused]]
#endif
    auto HookedEngineTick([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, UEngine*, float, bool) -> void"""))

# Hunk 2: QueryHarnessEngineTickDispatch [[maybe_unused]]
EDITS.append(("r2-querydispatch-attr",
"""    static auto QueryHarnessEngineTickDispatch([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] UEngine*, [[maybe_unused]] float, [[maybe_unused]] bool) -> void""",
"""#if UE4SS_R3_MARSHAL_MODE
    [[maybe_unused]]
#endif
    static auto QueryHarnessEngineTickDispatch([[maybe_unused]] Hook::TCallbackIterationData<void>& CallbackIterationData, [[maybe_unused]] UEngine*, [[maybe_unused]] float, [[maybe_unused]] bool) -> void"""))

# Hunk 3: FName-ctor detour compile-out (inside VerifyFNameConstructor)
EDITS.append(("r2-fname-compileout",
"""            FName::ConstructorInternal.assign_temp_address(AddressOverride);
        }

        // To ensure we don't use the FName constructor too early, hook it until something uses it.""",
"""            FName::ConstructorInternal.assign_temp_address(AddressOverride);
        }

#if defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE
        // [R3] BOTH_FAIL (DIFF 2 rev B; R3D 4.4c/5.3): RegisterFNameConstructorPostCallback
        // arms a PLH detour (exec write) whose ERROR_ID path throws at startup before
        // any Lua loads. Compiled out under R3 mode. Honest labels: the constructor
        // is NOT verified by this unit; verification is delegated to the FC-1
        // read-only validator (VSD 5 C1) when that lands. The data-side address
        // override binding above is kept, and the verification latch is settled so
        // the detour path's blocking wait can never be reached, let alone deadlock.
        // Merge-lineage coexistence: the seed path skips this verifier entirely when
        // SeedArming::bSeedBoundFNameCtor is set (and settles this same latch), so
        // armed-seed, Shape-A-refused and R3-mode launches all compose here.
        StaticStorage::FNameVerificationStatus.store(true, std::memory_order_release);
        StaticStorage::FNameVerificationStatus.notify_all();
        R3MarshalGate::refuse_registration_site(STR("FName constructor verification detour (verification delegated to the FC-1 read-only validator, VSD 5 C1)"));
        return;
#endif
        // To ensure we don't use the FName constructor too early, hook it until something uses it."""))

# Hunks 4+5: EngineTick registration compile-outs -> deferral + pump_pending.
# The #else arm carries the VERBATIM base registrations so the disarmed fold's
# token stream is identical to the unpatched base.
EDITS.append(("r2-enginetick-deferral",
"""#ifdef __PROSPERO__
            // Native publication/verification retirement may fail transiently.
            // HookedEngineTick invalidates itself after those steps succeed.
            const auto initializer_callback = Hook::RegisterEngineTickPreCallback(HookedEngineTick, {false, false, STR("UE4SS"), STR("GameThreadInitializer")});
            if (initializer_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Native game-thread initializer callback registration failed");
            }
            // Persistent harmless-query dispatch route (worker-query-s2-arm):
            // the self-retiring GameThreadInitializer callback publishes the
            // game-thread identity exactly once, but the query harness must
            // observe EVERY genuine engine tick (two consecutive accepted
            // reads). This callback never removes itself; refusal outcomes
            // are counted inside the harness and never thrown from a Tick.
            const auto query_dispatch_callback = Hook::RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch, {false, false, STR("UE4SS"), STR("QueryHarnessDispatch")});
            if (query_dispatch_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Query harness tick dispatch callback registration failed");
            }
#else""",
"""#ifdef __PROSPERO__
#if UE4SS_R3_MARSHAL_MODE
            // [R3] BOTH_FAIL (DIFF 2 rev B; R3D 4.4a/4.4b/5.3): RegisterEngineTickPreCallback
            // arms a forwarding detour whose only installable backends (PLH / D-1
            // mprotect) are dead on this world; arming would throw here before any Lua
            // loads. The game-thread pump - identity publication + query-harness
            // dispatch - is DEFERRED to the R3 FuncPtr-marshal drain (LuaMod DIFF 1).
            // The deferral stays loud and counted; the runtime status never silently
            // proceeds as if the pump were armed. Site ownership per LAUNCH11-MATRIX
            // 4-e: THIS block (both registrations) belongs to DIFF 2 in the rebased
            // unit; the no-exec gate owns every other E-site. Seeds coexistence: with
            // armed seeds the tick address is bound by the vtable fallback above; the
            // deferral replaces only the registration step.
            R3MarshalGate::refuse_registration_site(STR("EngineTick pre-callback registrations (GameThreadInitializer + QueryHarnessDispatch)"));
            Output::send(STR("[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal\\n"));
            R3MarshalGate::pump_pending.store(true, std::memory_order_release);
            R3MarshalGate::refused_site_count += 1; // the second refused site of this block
#else
            // Native publication/verification retirement may fail transiently.
            // HookedEngineTick invalidates itself after those steps succeed.
            const auto initializer_callback = Hook::RegisterEngineTickPreCallback(HookedEngineTick, {false, false, STR("UE4SS"), STR("GameThreadInitializer")});
            if (initializer_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Native game-thread initializer callback registration failed");
            }
            // Persistent harmless-query dispatch route (worker-query-s2-arm):
            // the self-retiring GameThreadInitializer callback publishes the
            // game-thread identity exactly once, but the query harness must
            // observe EVERY genuine engine tick (two consecutive accepted
            // reads). This callback never removes itself; refusal outcomes
            // are counted inside the harness and never thrown from a Tick.
            const auto query_dispatch_callback = Hook::RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch, {false, false, STR("UE4SS"), STR("QueryHarnessDispatch")});
            if (query_dispatch_callback == Hook::ERROR_ID)
            {
                throw std::runtime_error("Query harness tick dispatch callback registration failed");
            }
#endif
#else"""))

# Hunk 6: exported bridges at the RC::Unreal tail (defined only when ARMED)
EDITS.append(("r2-bridges",
"""    auto IsInGameThreadRaw() noexcept -> bool
    {
#ifdef __PROSPERO__
        return UnrealInitializer::NativeGameThreadIdentity.in_current_raw();
#else
        return IsGameThreadInitialized() && std::this_thread::get_id() == GetGameThreadId();
#endif
    }
}""",
"""    auto IsInGameThreadRaw() noexcept -> bool
    {
#ifdef __PROSPERO__
        return UnrealInitializer::NativeGameThreadIdentity.in_current_raw();
#else
        return IsGameThreadInitialized() && std::this_thread::get_id() == GetGameThreadId();
#endif
    }

#if defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE
    // =====================================================================================
    // [R3] FuncPtr-marshal bridges (DIFF 2 rev B; R3D 4.4a/4.4b; ports rev A). Called from
    // the LuaMod marshal drain (DIFF 1) once the calibration-selected carrier is armed.
    // Declared by the consumer locally (namespace Unreal in LuaMod.cpp) - no shared header
    // change, since a header edit would force a tree-wide recompile; the signatures are
    // the DIFF 1/DIFF 2 contract. REV B FOLD CONTRACT: defined only when BOTH __PROSPERO__
    // and UE4SS_R3_MARSHAL_MODE hold, so a consumer carrying DIFF 1's __PROSPERO__
    // references must be built in the matching ARMED fold (ARM-DISARM 2.3 pairing law,
    // now linker-enforced: mismatched folds fail the link loudly, never silently).
    // =====================================================================================
    auto R3MarshalPublishGameThreadIdentity() -> bool
    {
        if constexpr (UnrealInitializer::R3MarshalGate::kR3MarshalMode)
        {
            // HookedEngineTick publication logic (merge-lineage SINI:161-190), reused,
            // not re-invented: cancellation check first, then publish_current; the
            // first published ID is immutable and publication never manufactures a
            // worker identity.
            const auto& StartupWait = UnrealInitializer::StaticStorage::GlobalConfig.StartupWait;
            if (StartupWait && StartupWait->IsCancelled && StartupWait->IsCancelled())
            {
                // Startup refused/cancelled: never publish; the marshal drain gates on
                // this false return and drains nothing (refusal, not degradation).
                return false;
            }
            const auto publication = UnrealInitializer::NativeGameThreadIdentity.publish_current();
            if (publication == NativeThreadDetail::Publication::Unavailable)
            {
                static std::atomic<bool> logged_unavailable{false};
                if (!logged_unavailable.exchange(true))
                {
                    Output::send<LogLevel::Error>(STR("[R3] Unable to retrieve or synchronize ID of game thread\\n"));
                }
                return false;
            }
            // Published and AlreadyCurrent both mean this caller IS the game thread;
            // DifferentThread refuses. (NativeThreadDetail::Publication order:
            // Published, AlreadyCurrent, DifferentThread, Unavailable.)
            return publication != NativeThreadDetail::Publication::DifferentThread;
        }
        else
        {
            static std::atomic<bool> logged_disarmed{false};
            if (!logged_disarmed.exchange(true))
            {
                Output::send<LogLevel::Warning>(STR("[R3] publish bridge called while R3 marshal mode is disarmed; refusing\\n"));
            }
            return false;
        }
    }

    auto R3MarshalQueryHarnessDispatch() -> void
    {
        if constexpr (UnrealInitializer::R3MarshalGate::kR3MarshalMode)
        {
            // QueryHarnessEngineTickDispatch body (merge-lineage SINI:223-228) unchanged:
            // per-observation entry for the query harness; refusals are counted inside
            // the harness and never thrown. Semantic change declared per R3D 4.4b: the
            // harness's "two consecutive accepted reads" now count CARRIER invocations,
            // not engine ticks; rung instruments must be relabeled.
            RC::Platform::QueryHarness::Global().OnEngineTickDispatch();
        }
    }

    auto R3MarshalPumpPending() -> bool
    {
        return UnrealInitializer::R3MarshalGate::pump_pending.load(std::memory_order_acquire);
    }
#endif
}"""))

# ---- splice with uniqueness enforcement ------------------------------------------
anchor_receipt = {"base": {"path": str(TARGET), "sha256": sha(TARGET)},
                  "revision": "DIFF-2 rev B (worker-r3-diff2-rebase)",
                  "anchors": [], "aborted": False}
patched_text = text
for anchor_id, old, new in EDITS:
    n = patched_text.count(old)
    entry = {"id": anchor_id, "occurrences": n, "aborted": n != 1}
    anchor_receipt["anchors"].append(entry)
    print(f"{anchor_id:28s} occurrences={n}")
    if n != 1:
        anchor_receipt["aborted"] = True
        (ME / "receipts" / "anchors.json").write_text(json.dumps(anchor_receipt, indent=1) + "\n")
        sys.exit(f"ANCHOR DRIFT/AMBIGUITY for {anchor_id}: count={n}")
    patched_text = patched_text.replace(old, new, 1)

patched_file = patched_dir / "staging"
(patched_file.parent.mkdir(parents=True, exist_ok=True))
patched_file.mkdir(exist_ok=True)
(patched_file / "UnrealInitializer.cpp").write_text(patched_text)

# ---- emit the deliverable unified diff (a/staging, b/staging labels) -------------
out = subprocess.run(["diff", "-u",
                      "--label", "a/staging/UnrealInitializer.cpp",
                      "--label", "b/staging/UnrealInitializer.cpp",
                      str(base_file / "UnrealInitializer.cpp"),
                      str(patched_file / "UnrealInitializer.cpp")],
                     capture_output=True, text=True)
if out.returncode != 1:
    sys.exit(f"diff -u exited {out.returncode}: {out.stderr}")
diff_path = ME / "diffs" / "r3-initializer-r3-marshal-mode.revB.diff"
diff_path.parent.mkdir(exist_ok=True)
diff_path.write_text(out.stdout)
hunks = out.stdout.count("\n@@ ")
anchor_receipt["deliverable"] = {"path": str(diff_path), "sha256": sha(diff_path),
                                 "bytes": diff_path.stat().st_size, "hunks": hunks}
anchor_receipt["fold_guard"] = "every addition inside #if UE4SS_R3_MARSHAL_MODE (or #if defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE); in-file fallback #define ... 0 kept unconditional"
(ME / "receipts" / "anchors.json").write_text(json.dumps(anchor_receipt, indent=1) + "\n")
print(f"rebased diff: {diff_path} sha256={sha(diff_path)[:16]}... hunks={hunks}")

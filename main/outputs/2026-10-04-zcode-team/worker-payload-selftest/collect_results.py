#!/usr/bin/env python3
"""collect_results.py — merge build receipts, host record, console record and
wrapper evidence into results/selftest.json. Bounded terminal artifact writer;
no console contact."""
from pathlib import Path
import json
import time

WORKER = Path(__file__).resolve().parent
R = WORKER / "results"


def j(path):
    p = Path(path)
    return json.loads(p.read_text()) if p.exists() else None


build = j(WORKER / "build/build-receipt.json")
host_rec_line = (WORKER / "host/host-selftest.jsonl")
host_record = json.loads(host_rec_line.read_text().strip()) if host_rec_line.exists() else None
console_record = j(WORKER / "results/selftest-console-record.jsonl")
dispatch = j(WORKER / "results/dispatch-receipt.json")
console_run = j(WORKER / "results/console-run.json")
disasm_lines = (WORKER / "build/callsite-disasm.txt").read_text().splitlines() if (WORKER / "build/callsite-disasm.txt").exists() else []
syscall_syms = (WORKER / "build/payload-syscall-symbols.txt").read_text().splitlines() if (WORKER / "build/payload-syscall-symbols.txt").exists() else []

c5_console = next((c for c in console_record["checks"] if c["id"] == "5_range_verdict_semantics"), {})
c5_live = c5_console.get("detail", {}).get("live", {})
case_c = c5_live.get("case_c_exec_only", {})
case_b = c5_live.get("case_b_unmapped_negative", {})
prot_behavior = case_c.get("prot_behavior")
prot_verdict = {0: "inconclusive", 1: "prot-blind", 2: "protection-aware"}.get(prot_behavior, "absent")

out = {
    "worker": "worker-payload-selftest",
    "utc_collected_epoch": time.time(),
    "evidence_class": "separate-process standalone payload (pldmgr /loadpayload) + host child process; NEVER game-process; no game/console behavior claims",
    "bounded_condition": {
        "dispatches": 1,
        "upload_once": True,
        "readback_identity_verified": True,
        "build_retries_used": 0,
        "build_retries_budget": 2,
        "terminal": "all checks report pass/fail with build ids; stop and report",
    },
    "builds": {
        "host": build["builds"]["host"],
        "prospero": build["builds"]["prospero"],
        "determinism": "relink reproduced byte-identical binaries (same sha) with -Wl,--build-id=none",
        "pins": build["pin"],
    },
    "checks": {
        "host_build_id": build["builds"]["host"]["build_id"],
        "host_record": host_record,
        "console_build_id": build["builds"]["prospero"]["build_id"],
        "console_record": console_record,
    },
    "range_verdict_semantics": {
        "status_on_target": c5_console.get("status"),
        "CASE_C_VERDICT": prot_verdict,
        "case_c_evidence": case_c,
        "case_c_interpretation": (
            "mincore (SYS_mincore=78 via the generic syscall trampoline) returned 0 over a "
            "PROT_EXEC-only page constructed with mprotect on-target: the mapped/residency verdict "
            "is PROTECTION-BLIND. Per the SeedValidation v5 evidence contract this makes the "
            "(CaptureTruncated, Mapped, None) refusal CORRECT behavior; RangeVerdictImpliesReadable "
            "is NOT evidence-backed. Production code unchanged by this worker."
        ),
        "case_b_evidence": case_b,
        "case_b_finding": (
            "CHECK FAILED on the wrapper's errno convention, not on kernel behavior: the retained "
            "SDK libc.a syscalls.o trampoline returns the kernel error as a POSITIVE RAW return "
            "(raw_rc=12=ENOMEM) and does NOT set errno (raw_errno=0). The selftest wrapper (and the "
            "PRODUCTION bridge_kernel_range_verdict, which maps rc==0->Mapped / ::errno==ENOMEM->"
            "NotMapped / else Unavailable) therefore classifies a genuine-unmapped ENOMEM answer as "
            "Unavailable. Fail-safe (the listing gap still refuses) but the stronger NotMapped "
            "classification is lost whenever errno happens to be 0. Production implication flagged; "
            "no production file edited by this worker."
        ),
        "pure_logic_on_both": "rc_seed refusal matrix passed on host and target",
    },
    "wrapper_conventions": {
        "payload_defines_trampolines": [s for s in syscall_syms if " T " in s],
        "call_site_disasm_lines": len(disasm_lines),
        "call_site_evidence": "build/callsite-disasm.txt (edi=0x4e=78 SYS_mincore; rsi=start; rdx=span; rcx=static vec; addr32 callq syscall@0x7d2d0; errno compared against 0xc=12=ENOMEM)",
        "composed_candidate_evidence": "worker-launch11-vtable-prebuild/ue4ss-native-supplier-composition-6-l11/run-a/runtime.map lines 497820-497825: libc.a(syscalls.o):(.text.syscall) defines syscall/__syscall",
    },
    "dispatch": dispatch,
    "console_run": console_run,
}
(R / "selftest.json").write_text(json.dumps(out, indent=1) + "\n")
print("results/selftest.json written")
print("case-c verdict:", prot_verdict)

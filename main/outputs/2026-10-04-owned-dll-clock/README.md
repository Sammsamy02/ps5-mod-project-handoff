# Owned DLL clock integration

The opt-in `windows-candidate/build.py --entry --clock` profile now executes
real QPC/QPF imports from freshly compiled consumer/helper DLLs during DllMain
attach, exported work, and detach. Each phase requires positive, nondecreasing
counter samples, frequency 1,000,000,000, and preserved nonzero Windows last-error.
The frequency is a count scale, not a timer-resolution claim. The native provider
is `clock_gettime(CLOCK_MONOTONIC)`; no timestamps are fabricated.

The DLLs return attach BOOL 7, export values 86/93, and detach result 0 only when
their corresponding checks pass. Consumer/helper imports are 18/10. Each clock
session begins before module loading and stays pinned through attach rejection
and failed unmap retries; it retires after module EMPTY and before thread-domain
retirement. Result records expose session begin/end counts, not raw samples.
`--clock` without `--entry` fails before compilation.

| Evidence | Receipt | Passed commands |
| --- | --- | ---: |
| Real local normal/release execution and SDK ELF | `SDK-AND-EXECUTION-RECEIPT.json` (`622202c1`) | 52 |
| Rejected attach and deferred cleanup | `REJECT-RECEIPT.json` (`8a43fb9b`) | 14 |
| Entry-only regression | `ENTRY-REGRESSION-RECEIPT.json` (`4ffee5fe`) | 13 |
| Default/no-entry regression | `DEFAULT-REGRESSION-RECEIPT.json` (`ac966626`) | 27 |

All 106 recorded commands passed. The retained initial red run `ee460299` failed
before attach because the original candidate could not load the added imports.
Default and entry-only DLL hashes match their earlier receipts exactly; their
JSON records omit clock fields. Independent review checked actual PE IAT calls,
all three clock-check call sites per image, two Win64 BOOL(int64*) thunks, layout,
lifetimes and final hashes. No material finding remained. The only build warnings
are the existing PE linker's `/align specified without /driver` warnings.

The original SDK ELF is 202,680 bytes, SHA-256
`aab7d6a8ad2eb577f2161a10088a21184f2bdfc08cea21a2044245a172f6c2e1`.
Its frozen report path is `/tmp/modbridge-owned-dll-candidate.jsonl`; the parent
reports that `/tmp` is unavailable on the target. That artifact supplies local
compile evidence, not evidence of target result delivery.

A separate future profile uses `/data/modbridge-owned-dll-candidate.jsonl`.
`DATA-PROFILE-RECEIPT.json` records four successful commands: driver compilation,
relink, import inspection and ELF inspection. It reuses 18 reviewed objects
unchanged and changes only the driver report path/build identity. The prepared
ELF remains 202,680 bytes, SHA-256
`588d6e6ddf476fe764153df9a3235a3558f0e98e2a3b4b6070a6fb645c47bff4`.
Independent profile review confirmed identical driver machine instructions, all
18 reused objects, the exact import set and four successful commands; only the
profile strings and their local symbol metadata differ.
Neither artifact was launched by this work, and the user's separate frozen
no-entry diagnostic remains untouched.

Actual DLL execution here is local AMD64/macOS Rosetta only. The PS5 evidence is
SDK compilation/linking, native import declarations and ABI/layout inspection.
It does not establish target executable-memory permission, target clock calls,
game success, arbitrary Windows DLL support, or a new playable UE4SS build.
No console contact or game/system writes occurred. Static TLS, unwind/SEH and
broader Windows services remain incomplete; UE still needs actual game
provider/layout/Tick evidence.

See `CHECKPOINT.json` for full receipt provenance and artifact paths, and
`../ps5-mod-bridge/windows-services/CLOCK.md` for the previously reviewed provider,
Microsoft signatures, checked conversion and explicit last-error policy.

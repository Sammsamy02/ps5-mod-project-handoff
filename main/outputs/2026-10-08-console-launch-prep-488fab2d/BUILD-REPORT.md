The combined menu candidate is built locally, with byte-identical full runtime links and byte-identical PRX packages. It has not been uploaded or run on the PS5. The console remains on l18.

Verified changes selected in this candidate:
- Authenticated Townfall build/layout profile and the R4 dependent consumer rebuilds.
- Native callback forwarding through 256 static thunks, immutable registration identity and data-pointer publication rather than executable detours.
- Native thread identification through the pinned engine thread-name API plus exactly one declared system getter import; the owned relocation row is checked at runtime. This is not yet console-proven.
- Controller borrow retry/lifetime repair and stop guards; no pad settings or mod files changed.
- Retirement before stop and again after the initializer joins, plus sequentially consistent constructor/entry lifetime guards.

Build evidence:
- Actual coherent runtime: 374 selected units / 391 link inputs. Two full links are byte-identical (119,191,728 bytes; abcf511b262cd495367859df28c1ffb27ca2405023ec3bb06ffa656be71ada02). Pass2 freshly compiled the final initializer and retirement provider; all 374 selected source/object hashes agree.
- Two bootstrap compilations and two packaging passes used the same frozen source paths. Combined object, bare module and wrapped PRX are byte-identical.
- Wrapped PRX: 31,148,615 bytes; sha256 6085a1c43418c8a2e1008e1ba915f7f33207d0764ff6f94a9bf35a29136e33c7; SELF magic 4f153d1d; SDK stamp 0x11000000.
- Each package passes the 20 preserved static package checks and exact comparison of all 207 NID/module/library/version import identities. The sole new system import is scePthreadGetthreadid. The additional bridge check is locally defined, not a system import.
- Full constructor census preserves 238 init entries and 2 fini entries, including legacy .ctors/.dtors and repeated section names. This proves packaging preservation, not native constructor execution.

Preserved failures and corrections:
1. The first staging attempt followed a symlink anchor to the older bootstrap directory and missed runtime_start_bridge.h. It is preserved under ../2026-10-08-combined-menu-package/. The v2 edition copies the explicit, pinned R4 overlay directory.
2. Both v2 controller receipts retain the final checker IndexError: the combined ELF uses extended numbering (206,055 sections, e_shnum=0, e_shstrndx=65535). No failing receipt was rewritten. A separate corrected qualification reproduces that exact error, validates every original static check, and measures all modern/legacy array sections. Seven focused parser controls pass. CORRECTED-QUALIFICATION.json records each result.

Limits and holds:
- Mac-only. No console contact, uploads, configuration edits, payload loads, launches, original mod edits or game-file writes occurred in this work segment.
- The original PC release remains byte-identical locally (TownfallItemSpawner V1.0 main.lua f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658).
- The independent cleanup review did not complete: automatic review flagged the request for possible cybersecurity risk. No acceptance is claimed and that rejected review was not retried or rerouted. These local build receipts do not replace it.
- The known per-mod Lua callback borrower/uninstall lifetime gap is separate from the whole-runtime residence guard.
- Native controller borrowing/input, recurring carrier execution, authentic game-thread delivery, native UMG/reflected parameter schemas and visible spawning remain unproved. Host graph models are not native widget evidence.
- The latest console PS5Pad.ini has not been captured in this local segment; no mapping is assumed from an older copy.
- Upload qualification remains HOLD. No claim that the menu will appear, or that arbitrary UE4SS/DLL mods or games are supported.

Next steps:
1. Finish the separate source-backed menu-marshalling report and the unpromoted bounds guard experiment; do not guess native widget ownership or alter the PC mod.
2. Resolve the outstanding review/promotion hold without treating host tests as console success.
3. When a candidate is eligible and the user returns: confirm console state, follow the standing PRX-only procedure, then one user launch. Collect provider binding, admission/FName, original mod still running, controller path/F7, carrier/thread refusal or drain, original widget messages and crash evidence together before another launch. An observed menu and original script messages are required; a Ploader OK alone is insufficient.

Evidence files: TWIN-PACKAGE-RECEIPT.json; pass-1 and pass-2/CORRECTED-QUALIFICATION.json; BUILD-INPUTS.json; ../2026-10-08-r5-thread-final-integration-v2/TWIN-RECEIPT.json; ../2026-10-08-menu-marshal-gap/REPORT.md. All results are local and unpromoted.

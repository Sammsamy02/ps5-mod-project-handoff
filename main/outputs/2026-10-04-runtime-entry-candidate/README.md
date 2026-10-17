# Owned DllMain SDK candidate

The coherent candidate now has an opt-in real DLL entry profile. Both freshly
compiled owned DLLs run process attach and process detach through the existing
production PE loader/module owner and `mb_win_call_entry` bridge. This is local
host execution evidence plus a native SDK ELF build; PS5 execution is untested.

The missing integration was threefold: the previous candidate linked fixtures
with `/noentry`, passed NULL catalog entry callbacks, and left native thread
admission before final loader cleanup. The new `--entry` profile supplies stable
callbacks and retains the original admitted pthread through detach and any
failed release retries. Default no-entry behavior remains available.

Each DllMain allocates a heap token and dynamic TLS slot and uses last-error
services. Exports require that initialized state, then produce 86 and 93.
Detach checks and frees it while imports and thread services remain alive.
Attach returns BOOL 7; detach returns FALSE, which the owner correctly ignores.
There are no loader calls from DllMain, CRT startup, PE static TLS, unwind,
thread notifications, asynchronous work or escaping callbacks.

| Evidence | Result | Receipt |
| --- | --- | --- |
| Entry host execution and native SDK build | 16/8 bound imports; each DLL attaches and detaches once; 86/93; retry never repeats detach | [ENTRY-RECEIPT.json](ENTRY-RECEIPT.json) |
| FALSE attach, including failed release rollback | Consumer attaches/detaches once; zero export calls; original ENTRY_REJECTED cause 13 retained | [REJECT-RECEIPT.json](REJECT-RECEIPT.json) |
| No-entry compatibility | Both entry RVAs zero; zero entry calls; original 10/6 imports and 86/93 behavior | [NOENTRY-RECEIPT.json](NOENTRY-RECEIPT.json) |

Final current-source checks matched 187 entry-profile hash entries, 60 rejected
profile entries and 60 no-entry entries. All 46, 13 and 27 respective saved
commands exited zero. The native entry bridge retains one exact Win64 call:
32-bit BOOL return and pointer/DWORD/pointer arguments. The 45 public ABI layout
metrics and 27 SDK dynamic import mappings also pass. Both PE fixture links emit
the existing `/align specified without /driver; image may not run` warning.

The entry SDK ELF is 202,152 bytes, SHA256
`5931b705bec85e30035e21532d6f436d716d7d925d58af014496a4e62137a049`, at
`.superpowers/native-dependency-spike/owned-dll-sdk-candidate-1/8554db78fdc54a5a8ebbc4494c2fa1b6/owned-dll-candidate.elf`.
Full artifact paths, source/dependency identities, JSONL evidence and saved
commands are retained in the receipts and [SUMMARY.json](SUMMARY.json).

Reproduce from the repository root:

```sh
python3 outputs/ps5-mod-bridge/windows-candidate/build.py --entry
python3 outputs/ps5-mod-bridge/windows-candidate/build.py --entry --reject-attach --host-only
python3 outputs/ps5-mod-bridge/windows-candidate/build.py --host-only
```

Only candidate-owned sources/fixtures/tests and this report slice were changed.
Shared loader, module and service sources were reused without edits. The prior
`20cd914761004ad5855ebf73ea4e1db3` ELF and receipt remain byte-identical.

No target dispatch, contact, placement or writes occurred. Native executable
permission, report-path availability and actual PS5 DLL execution remain
untested. This is an SDK payload ELF, with no ordinary PRX acceptance claim.

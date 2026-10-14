# VERIFICATION.md — worker-engineversion-ini, 2026-10-04

Host-only verification receipts. All runs on this Mac (arm64, clang); no
console contact, no engine writes. Oracle lineage and exact commands are
recorded so any later worker can replay.

## V1 — Existing retained-parser oracle on the NEW settings bytes: PASS

Binary: `worker-ue4ss-settings/host-test/settings_parse_test` (compiled by
worker-ue4ss-settings from the vendored IniParser with PS5 `FORCE_U16` string
semantics; replays `SettingsManager::deserialize` reads + the
`validate_native_services` gate order from `UE4SSProgram.cpp:721-727`).

```
$ settings_parse_test <this worker>/package-v2/ue4ss/UE4SS-settings.ini \
      worker-ue4ss-settings/host-test/probe-inline-comment.ini \
      worker-ue4ss-settings/host-test/probe-emptysection.ini
package-v2/ue4ss/UE4SS-settings.ini -> PASS (all gates + expected values OK)
probe-inline-comment.ini -> FAIL (THROW[...x3]; EXPECT-FAIL[...11 keys])
probe-emptysection.ini   -> FAIL (PARSE-THROW: Syntax error(3 : 1): ...)
exit=0   (only argv[1] must pass; probes prove the oracle still discriminates)
```

Interpretation: appending `[EngineVersionOverride]` did not disturb a single
existing constraint — console/GUI/crashdump zeros, RenderMode=ExternalThread,
EngineTick, Scan, bUseUObjectArrayCache=false, no `[Overrides]`, hot-reload
keys absent, full-line-comment parser-trap compliance.

## V2 — Derived oracle asserting the override values + admission rule: PASS

Source: `host-test/settings_parse_test_ev.cpp`
(sha256 `3466fde07de247f2ab445f5b1c5050b3b2eb5adac99251d83771b496aa11b86e`)
— worker-ue4ss-settings' oracle PLUS three assertions: `MajorVersion==5`,
`MinorVersion==6`, and a replay of `UE4SSProgram.cpp:869-884` admission
(both != -1 AND uint32-range, else the recorded startup-throw branch).

Binary: `host-test/settings_parse_test_ev`
(sha256 `360255eb736f3968775465d64531c4f99dc863978cb84f06b519f910c8817d13`).
Built from the SAME vendored parser sources as V1
(`ue4ss-native-settings-port-1/patched/IniParser` + `ParserBase`, fmt
header-only) plus a never-executed link stub for
`RC::File::LinuxFile::read_all` (`host-test/linuxfile_stub.cpp` — the
file-handle parse overload is unused by the oracle). 0 warnings emitted from
our translation units.

```
$ settings_parse_test_ev package-v2/ue4ss/UE4SS-settings.ini \
      probe-inline-comment.ini probe-emptysection.ini
package-v2/ue4ss/UE4SS-settings.ini -> PASS (all gates + expected values OK)
probe-inline-comment.ini -> FAIL (... EXPECT-FAIL[EngineVersionOverride.MajorVersion==5];
                                  EXPECT-FAIL[EngineVersionOverride.MinorVersion==6];
                                  EXPECT-FAIL[UP.cpp:869-884 admission: both set, uint32-range])
probe-emptysection.ini   -> FAIL (PARSE-THROW ...)
exit=0
```

## V3 — Admission edge probes (discrimination of V2's new checks)

- `host-test/probe-ev-outofrange.ini` (`MajorVersion=4294967296`):
  `FAIL (PARSE-THROW: stoi: out of range)`. FINDING: the retained parser's
  `get_int64` is backed by a 32-bit `stoi`; values above 2147483647 throw
  inside the parser, `REGISTER_INT64_SETTING` swallows it, and the override
  silently stays unset. The UP.cpp uint32-throw is therefore shadowed by the
  parser for out-of-int32 input; the effective window is int32 ∩ uint32.
  `MajorVersion=5`/`MinorVersion=6` are inside every bound.
- `host-test/probe-ev-unset.ini` (section removed → byte-identical to the old
  779-byte file): `FAIL (EXPECT-FAIL[MajorVersion==5]; EXPECT-FAIL[MinorVersion==6];
  EXPECT-FAIL[admission])` — proves the checks detect absence, and incidentally
  re-proves the old file is exactly the new file minus the appended 55 bytes.
- exit=1 for both, as required.

## V4 — Base-pin integrity re-verification

```
sha256 worker-loadlist-final/final-package/ue4ss/UE4SS-settings.ini
= 9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f  (779 B)
```
Matches the config-review §1 pin and the worker-final-staging-rehearsal C3
check. Composition asserted the old bytes are a strict prefix of the new file
and that the new file contains no NUL, no CR, only ASCII+LF.

## V5 — mfa activation simulation against the NEW loadlist row: 46 PASS / 0 FAIL

Derived from `worker-loadlist-final/host-test/test_activation_final.c` with
ONLY the settings-size and payload-total expectations updated
(`host-test/test_activation_ev.c`,
sha256 `0bf036184230ec961137d32a12837dad0abbcda784d377e60880ba0ca59dfeb1`;
binary `host-test/test_activation_ev`,
sha256 `8bb40afc87a5c5f6b5603d1b91ac6e3e02f8bf22bd3eb251aef337a2e46fd822`),
linked against the vendored `modforge_activation.c` +
`modforge_loadlist.h` (`worker-f-r2-fix/composition/vendored` — the same
mfa the runtime chain performs). Full log: `host-test/ev-activation-output.txt`.

```
$ test_activation_ev <this worker>/package-v2 \
      worker-final-staging-rehearsal/root-badtitle \
      worker-final-staging-rehearsal/root-badsize \
      worker-final-staging-rehearsal/root-noll
PASS T1 ... (header/1 title/1 engine/1 framework/1 mod, 7 file rows exact
             role/path/size incl. settings row size==834, 0 order, 0 unknown)
PASS T2 mfa_activate(package-v2) OK; engine unreal; framework ue4ss-ps5;
       mod [TownfallItemSpawner 1.0.0]; 7 files preloaded;
       preloaded bytes == 35952 (35945 payload + 7 NUL terminators)
PASS T3 ue4ss/UE4SS-settings.ini present, size==834, data NUL-terminated at [size]
PASS T3/T4/T5 ... (layout pair 588/186; startup list [TownfallItemSpawner])
PASS T6 wrong title -> MFA_E_TITLE (rc=-4)
PASS T6 truncated file -> MFA_E_SIZE (rc=-8)
PASS T6 missing loadlist -> MFA_E_NO_LOADLIST (rc=-2)
test_activation_final: 0 failure(s)
46 PASS / 0 FAIL
```

This is the mfa parser (`mf_next`) validating the NEW row
(`file\tconfig\tue4ss/UE4SS-settings.ini\t834`) — path rules, u64 size parse,
role/path/size table, exact buffering, and the negative-control failure codes
all hold.

## V6 — Lockstep negative: OLD settings bytes vs NEW row → refused

`host-test/root-ev-size-mismatch/` = package-v2 with the settings file swapped
back to the 779-byte original (sha re-verified `9f3fa12772183314…`), loadlist
still demanding 834.

```
$ mismatch_probe host-test/root-ev-size-mismatch
rc=-8 err.code=-8 err.message=file is shorter than the load list says err.path=ue4ss/UE4SS-settings.ini
exit=0 (probe exits 0 iff MFA_E_SIZE/-8)
```
(`host-test/mismatch_probe.c`, sha256 of binary `6ec1979232968e3f6d06709f04a71abd931c6897fbf7c1a6a15cf50bc6d5ec21`.)

Symmetric case (new bytes vs old 779 row) is the same check inside mfa's
`read_exact` ("file is longer than the load list says") — the pair must move
together, as NEW-SETTINGS.md §3 requires.

## Deliverable hashes

- `package-v2/ue4ss/UE4SS-settings.ini` 834 B
  `9570ead39ce8b9a2a2d96eddbcf04d43dd0225be0bbfd5b2cd417d618b8c6e0c`
- `package-v2/loadlist.txt` 446 B
  `3afd10cebca1a89c653d70fff453bdec121f2bc6c230c4768f0833b3d0032ff5`
- all other package-v2 files byte-copied (shas as listed in
  worker-config-final-review CONFIG-VERDICT.md §1; only the two files above
  differ from final-package, re-verified by the T1/T3 size table and the
  composition-time copy asserts).

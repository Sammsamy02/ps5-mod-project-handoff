# Licensing

Effective 2026-10-14, Sammsamy02 releases the original project work they own in
this repository under the GNU General Public License, version 3 only
(`GPL-3.0-only`). The full, unmodified license text is in [LICENSE](LICENSE).
Copyright (c) 2026 Sammsamy02 for that original work; existing contributor and
upstream copyright notices remain intact.

This grant covers original source, scripts, tests, project documentation and owned
UI screenshots unless a file or component already carries a separate license.
It includes original work retained in the main, UI, installer, team, legacy and
session snapshots. Copies and patches of upstream code retain their upstream
notices and applicable terms; this grant does not claim ownership of third-party
material or replace its licenses. Repository layout alone does not change those
terms. GPLv3 is the default for original work without another explicit license.

You may use, modify and redistribute covered work, including commercially, under
GPLv3. When distributing modified covered work, preserve notices, identify changes
and provide the corresponding source under GPLv3 as the license requires. The
software is provided without warranty. The license text controls over this summary.

## Retained third-party terms

This is a guide to retained notices, not a claim that every dependency has the
same license. File headers and component notices remain authoritative.

| Component | Retained terms / notice location |
| --- | --- |
| SDK and SharpProspero | GPLv3; `third-party-licenses/SDK-GPL-3.0.txt` and `SharpProspero-GPL-3.0.txt` |
| UE4SS, PolyHook, Zydis, Zycore and fmt | MIT; respective notices in `third-party-licenses/` |
| AsmJit and AsmTK | zlib; respective notices in `third-party-licenses/` |
| PatternSleuth | MIT OR Apache-2.0 declaration in `third-party-licenses/PatternSleuth-license-declaration.toml`; obtain complete notices with pinned upstream sources |
| nlohmann JSON and PicoSHA2 | MIT; app/UI `third_party/` component licenses |
| libarchive | Component-specific BSD-style notices; app/UI `third_party/libarchive/COPYING` and source headers |
| ImGui, ImGui text editor and GLFW | Component licenses in `legacy-port-source/ue4ss-real-lua-link-route-1/source-dependencies/` |
| prosperodev module metadata | MIT; `main/outputs/ps5-mod-bridge/native-module-metadata/LICENSE.prosperodev` |
| Mono and related sources | Mixed upstream terms; `teams/caps-resume-20261013/caps-team-work/mono-host-research/LICENSE` and other component notices |
| LLVM libraries, BLAKE3, Mesa and other vendored sources | Applicable component license files and source headers retained alongside each copy |

Preserve upstream copyright, license and patent notices when copying or rebuilding
components. Obtain missing dependency sources and notices from the pinned upstreams
listed in [handoff/DEPENDENCIES.md](handoff/DEPENDENCIES.md). A new dependency or
combined binary requires checking its own terms; this source license does not
certify every possible future combination or release artifact.

Game content, downloaded mods and Epic engine source are excluded from this export
and are not licensed by this grant. No rights to third-party trademarks, services
or accounts are granted. Technical restrictions and historical test results are
not additional restrictions on the rights granted by GPLv3.

## Change record

2026-10-14: At the owner's request, replaced the private-handoff notice with GPLv3
for original project work, preserved third-party notices, and updated current
handoff documentation. Historical notes describe their original dates; this file
states the current licensing policy. Runtime/app source and repository visibility
were not changed by the licensing update.

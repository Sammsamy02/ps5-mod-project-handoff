# STAGING-COMMANDS.md — placing the first-run tree on the console

worker-ue4ss-settings, 2026-10-04. Controller-executed; this worker ran none of it (no network).
Conventions follow worker-e2e-console-staging/STEP-BY-STEP.md + PREREQS.md:
`HTTP=http://ps5-host-not-configured.invalid:8084`, `FTP=ftp://ps5-host-not-configured.invalid:2121` (anonymous). Ownership: only
`/data/modbridge/**` is ever written; nothing else. Honest channel status: FTP RETR readback is
proven; **FTP STOR write is NOT evidenced anywhere in workspace records** (PREREQS G3) — gate A
below must pass before any STOR in route A/B.

## 0. Local staged bytes (already prepared + hashed under this worker dir)

| console path (under root /data/modbridge/roots/PPSA33286/) | local file | bytes | sha256 |
|---|---|---|---|
| ue4ss/UE4SS-settings.ini | staged/ue4ss/UE4SS-settings.ini | 779 | 9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f |
| ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua | staged/.../main.lua | 21415 | f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658 |
| ue4ss/Mods/TownfallItemSpawner/enabled.txt | staged/.../enabled.txt | 0 | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| ue4ss/Mods/shared/UEHelpers/UEHelpers.lua | staged/.../UEHelpers.lua | 9962 | 5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7 |
| ue4ss/MemberVariableLayout.ini | **BLOCKED-ON-LAYOUT-DATA (worker-2)** — no bytes exist; staging it placeholder-free is impossible and placeholders are forbidden (LAYOUT-GATE) | — | — |
| ue4ss/VTableLayout.ini | **BLOCKED-ON-LAYOUT-DATA (worker-2)** | — | — |

main.lua bytes = retained intake member-00000000.bin (f586043a…658), re-hashed unchanged above.
UEHelpers = retained distro asset (5a8f6609…15a), re-hashed unchanged above.
Until the layout pair exists, the composed runtime stops inside setup_unreal()
("Native activation requires both member and vtable layout files", UE4SSProgram.cpp:896-906):
staging routes below place the four real files and honestly leave the pair absent.

## Route A (sanctioned): installer-managed root — preferred

The managed root and its internal directory structure are created transactionally by the
modbridge installer (host-proven verbs, worker-e2e-console-staging REPORT §3); we never mkdir
inside `roots/` by hand.

```sh
# A0. Bounded STOR write-probe gate (PREREQS G3/P1) — once, into our own intake dir:
curl -s -T /etc/hostname "/data/modbridge/intake/.writeprobe" &&   curl -s "/data/modbridge/intake/.writeprobe" | head -c 16   # STOR+RETR roundtrip
# No roundtrip -> STOP: no evidenced write route; report; do not improvise.

# A1. Stage the mod archive we own (bytes = BUNDLE-SPEC §2 bundle, WITH the
#     regenerated settings ini from this worker — NOT the f6d06cad… one):
curl -s -T outputs/2026-10-04-zcode-team/worker-itemspawner-bundle/bundle.zip      "/data/modbridge/intake/townfall-itemspawner.zip"
curl -s "/data/modbridge/intake/townfall-itemspawner.zip" | shasum -a 256   # must equal staged zip hash

# A2. Register + prepare + install (exact verb grammar per E2E-RUNBOOK-V2):
SVC="--state /data/modbridge/state --roots /data/modbridge/roots"
$SVC_CMD register-mod   --id townfall-itemspawner --title PPSA33286 --bundle /data/modbridge/intake/townfall-itemspawner.zip
$SVC_CMD prepare-archive --title PPSA33286 --archive /data/modbridge/intake/townfall-itemspawner.zip --quarantine /data/modbridge/intake/townfall.q --mod townfall-itemspawner
$SVC_CMD install --title PPSA33286 --mods townfall-itemspawner
# install prints: staged files=N root=…PPSA33286.next -> installed … loadlist=<sha256>

# A3. Readback identity of every staged file (RETR is proven):
for f in ue4ss/UE4SS-settings.ini \
         ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua \
         ue4ss/Mods/TownfallItemSpawner/enabled.txt \
         ue4ss/Mods/shared/UEHelpers/UEHelpers.lua; do
  curl -s "/data/modbridge/roots/PPSA33286/$f" | shasum -a 256
done
# Expected: the four sha256 values from the table in §0, in order.
curl -s "/data/modbridge/roots/PPSA33286/loadlist.txt"    # header + title/engine/framework/mod/file rows
```

## Route B (preview-only, NOT the managed path): raw FTP tree under our intake dir

Only for byte-level preview/diff of the logical tree. NEVER write `roots/` directly — that
bypasses the transactional installer and recovery journal. curl creates intermediate dirs on
STOR via --ftp-create-dirs:

```sh
D=/data/modbridge/intake/townfall-ue4ss-preview
R=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-ue4ss-settings/staged
curl -s --ftp-create-dirs -T "$R/ue4ss/UE4SS-settings.ini"                          "$D/ue4ss/UE4SS-settings.ini"
curl -s --ftp-create-dirs -T "$R/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"   "$D/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
curl -s --ftp-create-dirs -T "$R/ue4ss/Mods/TownfallItemSpawner/enabled.txt"        "$D/ue4ss/Mods/TownfallItemSpawner/enabled.txt"
curl -s --ftp-create-dirs -T "$R/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"         "$D/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"
# readback hash verify (must equal §0):
for f in ue4ss/UE4SS-settings.ini ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua \
         ue4ss/Mods/TownfallItemSpawner/enabled.txt ue4ss/Mods/shared/UEHelpers/UEHelpers.lua; do
  curl -s "$D/$f" | shasum -a 256
done
# Do NOT create ue4ss/MemberVariableLayout.ini / VTableLayout.ini here: blocked on worker-2.
```

## Postconditions + rollback

- Route A owns: intake zip, quarantine dir on refusal, records rows, generation under
  `roots/PPSA33286/`. Rollback before stage-3 commit = fresh-tree reset (`/data/modbridge/state`
  delete); after commit = the installer's own remove verb. Nothing outside `/data/modbridge/**`.
- Route B owns only `intake/townfall-ue4ss-preview/**`; delete the whole subtree to roll back.
- The settings ini in this tree is the [VERIFIED] canonical content (SETTINGS-CONTENT.md). Any
  bundle still carrying the prior 221-B ini (sha256 f6d06cad…) must be regenerated first — it
  hard-fails validate_native_services at UE4SSProgram construction.

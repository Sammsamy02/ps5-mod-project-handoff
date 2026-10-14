#!/bin/bash
# run_stage_next.sh — ONE-COMMAND staging plan for the NEXT candidate cycle
# (worker-staging-next, 2026-10-05). Reuses, verbatim in law and shape:
#   - worker-launch10-staging-packet/run_stage_launch10.sh (plan/stage/post wrapper,
#     BUILD-ID placeholder refusal, ini freeze, .bak-before-upload, GO/NO-GO)
#   - worker-console-staging-bundle/MASTER-STAGING-PACKAGE.md §0/§B (Gate A0 write
#     probe, pre-image RETR before any STOR, readback sha of every upload,
#     loadlist.txt LAST, rollback §F shape)
#   - worker-console-batch-runner/console_batch.py flow `stage-launch10` for the
#     UE4SS candidate composition PRX (phase C) — REUSED, not reinvented.
#
# Console facts (all prior packets): FTP=ftp://ps5-host-not-configured.invalid:2121 (anonymous),
# HTTP=http://ps5-host-not-configured.invalid:8084 (pldmgr, arming only). Game must be CLOSED.
#
# Phases (exact order, the plan's whole law):
#   A  bundle upload set (preflight-CLEAN worker-itemspawner-bundle/bundle):
#      4 tree files FIRST, loadlist.txt LAST (install-commit truth). Every file
#      readback-verified against the pinned sha below.
#      MUST NOT touch: any game file, ploader.ini, MemberVariableLayout.ini,
#      VTableLayout.ini, PS5Pad.ini, anything else under $R — no DELE anywhere
#      in this flow; only the 5 files below are written, exactly once each.
#   B  NEW ploader payload (built THIS session host-side, pinned):
#      pre-image RETR -> console .bak -> ONE STOR -> readback sha MUST equal the
#      pinned sha. Mismatch -> ONE bounded byte-exact restore of the pre-image,
#      then refuse (never retry).
#   -- ploader.ini FREEZE: RETR sha at start AND end; byte-equal or loud refusal
#      + restore of whatever this flow changed (it changes NOTHING — external
#      drift is reconciled by the controller, never blind-fixed here).
#   C  UE4SS candidate composition PRX AFTER composition lands: pass
#      --ue4ss-prx PATH --ue4ss-sha SHA (or use `stage-c`). Delegated to the
#      launch10 flow (BUILD-ID law, .bak, SHT-strip readback law, Gate A1 magic
#      7f454c46, ini freeze — all enforced there).
#
# The tool NEVER launches. Arming (`curl -s "$HTTP/loadpayload:/data/pldmgr/
# payloads/ploader/ploader.elf"`) and the game launch are USER/controller steps,
# documented in STAGING-PLAN-NEXT.md — run them only after phase C is staged.
#
# Usage:
#   ./run_stage_next.sh plan                 # host-only dry-run, ZERO contact
#   ./run_stage_next.sh stage                # phases A+B (+C if --ue4ss-* given)
#   ./run_stage_next.sh stage --ue4ss-prx P --ue4ss-sha S
#   ./run_stage_next.sh stage-c --ue4ss-prx P --ue4ss-sha S   # phase C only
#   ./run_stage_next.sh post                 # AFTER the user-driven launch

set -euo pipefail

W=/WORKSPACE/.codex/worktrees/translation-core/what
T=$W/outputs/2026-10-04-zcode-team
SELF_DIR=$T/worker-staging-next
TOOL_DIR=$T/worker-console-batch-runner
BUNDLE=$T/worker-itemspawner-bundle/bundle
PLOADER_ELF=$SELF_DIR/ploader-build/ploader.elf
RUN_ROOT=$SELF_DIR/runs

FTP=ftp://ps5-host-not-configured.invalid:2121
HTTP=http://ps5-host-not-configured.invalid:8084
CURL="curl -sS --connect-timeout 10 --max-time 120"
R=/data/modbridge/roots/PPSA33286
PLOADER_REMOTE=/data/pldmgr/payloads/ploader/ploader.elf
INI_REMOTE=/data/plugins/ploader.ini

# ---- PINNED ARTIFACTS (verified on disk this session; do not edit by hand) ----
# new ploader payload (task: "new ploader.prx" — the payload is named ploader.elf)
PLOADER_SHA=4c33923521ee8d8b810f8daaa719b935e9e92f6e8892deae43143fb9ec81d292
PLOADER_BYTES=144248
PLOADER_STRIP_LEN=98332          # max(p_offset+p_filesz) — SHT-strip fallback law
# bundle set, local path relative to $BUNDLE, remote path under $R, sha256
BUNDLE_FILES=(
  "ue4ss/UE4SS-settings.ini|f6d06cada43d43d0674c2986152b72a4dddecf29331acb9cd85e47172889a37f"
  "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua|f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658"
  "ue4ss/Mods/TownfallItemSpawner/enabled.txt|e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  "ue4ss/Mods/shared/UEHelpers/UEHelpers.lua|5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7"
)
LOADLIST_LOCAL=$BUNDLE/loadlist.txt
LOADLIST_SHA=cbe301368f4a8108ccca16f9739a1c74a8e3242ebc875a9e40b8d21d3846746e
# ---------------------------------------------------------------------------

RUN_DIR=""

die() { echo "REFUSED: $*" >&2; exit 2; }

# rescue law (launch10 shape): the verdict ALWAYS lands in go-nogo.json —
# if the pass halts before the GO summary was written, record an honest NO-GO.
rescue_nogo() {
  if [[ -n "$RUN_DIR" && ! -f "$RUN_DIR/go-nogo.json" ]]; then
    echo '{ "verdict": "NO-GO", "halted_before_completion": true }' > "$RUN_DIR/go-nogo.json"
    echo "NO-GO recorded: $RUN_DIR/go-nogo.json" >&2
  fi
}

sha() { shasum -a 256 "$1" | awk '{print $1}'; }

host_preflight() {   # plan + stage both run this FIRST (zero contact in plan)
  [[ -f "$PLOADER_ELF" ]] || die "ploader payload missing: $PLOADER_ELF"
  local got; got=$(sha "$PLOADER_ELF")
  [[ "$got" == "$PLOADER_SHA" ]] || die "ploader payload sha drift: $got != $PLOADER_SHA"
  [[ "$(stat -f%z "$PLOADER_ELF")" == "$PLOADER_BYTES" ]] || die "ploader payload size drift"
  [[ "$(sha "$LOADLIST_LOCAL")" == "$LOADLIST_SHA" ]] || die "bundle loadlist sha drift"
  local row rel want
  for row in "${BUNDLE_FILES[@]}"; do
    rel=${row%%|*}; want=${row##*|}
    [[ -f "$BUNDLE/$rel" ]] || die "bundle file missing: $rel"
    [[ "$(sha "$BUNDLE/$rel")" == "$want" ]] || die "bundle file sha drift: $rel"
  done
  echo "host preflight: all pinned artifacts present and hash-clean"
}

dump_plan() {
  cat <<'PLAN'
Ordered steps (each has its verification; ANY failure halts the pass):
  0. G0 reachability: pldmgr 8084 /list_payloads (existing pattern; no alternates)
  1. Gate A0 write probe, exactly once: STOR+RETR /data/modbridge/intake/.writeprobe
  2. PRE-IMAGES (RETR only): $PLOADER_REMOTE, $INI_REMOTE, and each bundle target;
     record sha256 + size (absence recorded honestly). No STOR has happened yet.
  3. console .bak of the installed ploader payload: $PLOADER_REMOTE.$TAG.bak
  4. phase A STOR wave 1 — the 4 bundle tree files to $R/ue4ss/... (exactly once each)
  5. phase A STOR wave 2 — $R/loadlist.txt LAST (install-commit truth)
  6. phase B STOR — new ploader payload to $PLOADER_REMOTE (exactly once)
  7. READBACK verify EVERY uploaded file (RETR | shasum) against the pinned shas;
     ploader payload: full byte identity expected (plain ELF, magic 7f454c46;
     SHT-strip tail class accepted: len==98332 && body 64..end identical).
     ANY mismatch -> ONE bounded restore from the saved pre-image, then refuse.
  8. ploader.ini STILL byte-equal to its step-2 pre-image sha (external drift ->
     refuse + reconcile by controller; this flow never writes the ini)
  9. GO/NO-GO summary JSON in the run dir (rescue: written even after a halt)
 10. phase C (only with --ue4ss-prx/--ue4ss-sha): console_batch.py stage-launch10
     (BUILD-ID law, .bak, SHT-strip readback law, Gate A1 magic 7f454c46)
 11. NOT in this tool (user/controller, game CLOSED): arm ploader via pldmgr
     loadpayload, then USER launches. NEVER launch with phase C still pending.
Untouchable by design: game files; $INI_REMOTE; $R/ue4ss/MemberVariableLayout.ini;
  $R/ue4ss/VTableLayout.ini; $R/ue4ss/PS5Pad.ini; /data/plugins/* (except nothing —
  phase C owns its one PRX name inside its own flow). No DELE anywhere in A/B.
PLAN
}

phase_a_b() {
  local TAG=$1
  mkdir -p "$RUN_DIR" /tmp/ppsa33286-preimages-next
  trap rescue_nogo EXIT
  local PRE=/tmp/ppsa33286-preimages-next

  echo "== G0: pldmgr reachability =="
  $CURL "$HTTP/list_payloads" -o "$RUN_DIR/g0-list-payloads.json" || die "pldmgr 8084 unreachable (G0)"

  echo "== Gate A0: write probe, exactly once =="
  # macOS host fix (controller 2026-10-05): /etc/hostname does not exist on
  # Darwin — probe with the bundle loadlist bytes instead (any small local
  # file works; the probe only proves the FTP write route).
  $CURL -T "$T/worker-itemspawner-bundle/bundle/loadlist.txt" "$FTP/data/modbridge/intake/.writeprobe" || die "Gate A0: no FTP write route — STOP per MASTER §0"
  $CURL "$FTP/data/modbridge/intake/.writeprobe" | head -c 16 > "$RUN_DIR/gate-a0-readback.txt"

  echo "== pre-images (RETR only, recorded before ANY STOR) =="
  $CURL "$FTP$PLOADER_REMOTE" -o "$PRE/ploader.elf.pre" || die "installed ploader payload not readable — cannot stage without a rollback pre-image"
  sha "$PRE/ploader.elf.pre" > "$RUN_DIR/ploader.elf.pre.sha256"; stat -f%z "$PRE/ploader.elf.pre" >> "$RUN_DIR/ploader.elf.pre.sha256"
  $CURL "$FTP$INI_REMOTE" -o "$PRE/ploader.ini.pre" || die "ploader.ini not readable — ini freeze impossible, refusing"
  local INI_SHA; INI_SHA=$(sha "$PRE/ploader.ini.pre"); echo "$INI_SHA" > "$RUN_DIR/ploader.ini.frozen.sha256"
  echo "ini freeze sha: $INI_SHA"
  local row rel dest
  for row in "${BUNDLE_FILES[@]}"; do
    rel=${row%%|*}
    $CURL "$FTP$R/$rel" -o "$PRE/$(basename "$rel").pre" || echo "no prior $R/$rel (fresh install path)"
  done
  $CURL "$FTP$R/loadlist.txt" -o "$PRE/loadlist.pre" || echo "no prior $R/loadlist.txt"

  echo "== console .bak of installed ploader payload =="
  $CURL -T "$PRE/ploader.elf.pre" "$FTP$PLOADER_REMOTE.$TAG.bak" || die "could not write console .bak — aborting before any overwrite"

  echo "== phase A wave 1: bundle tree (exactly one STOR each) =="
  for row in "${BUNDLE_FILES[@]}"; do
    rel=${row%%|*}
    $CURL --ftp-create-dirs -T "$BUNDLE/$rel" "$FTP$R/$rel" || die "STOR failed: $rel"
  done

  echo "== phase A wave 2: loadlist LAST =="
  $CURL -T "$LOADLIST_LOCAL" "$FTP$R/loadlist.txt" || die "STOR failed: loadlist.txt"

  echo "== phase B: new ploader payload (exactly one STOR) =="
  $CURL -T "$PLOADER_ELF" "$FTP$PLOADER_REMOTE" || die "STOR failed: $PLOADER_REMOTE"

  echo "== readback verification of EVERY upload =="
  local got bad=0
  for row in "${BUNDLE_FILES[@]}"; do
    rel=${row%%|*}; want=${row##*|}
    got=$($CURL "$FTP$R/$rel" | shasum -a 256 | awk '{print $1}')
    [[ "$got" == "$want" ]] || { echo "READBACK MISMATCH: $rel ($got != $want)"; bad=1; }
  done
  got=$($CURL "$FTP$R/loadlist.txt" | shasum -a 256 | awk '{print $1}')
  [[ "$got" == "$LOADLIST_SHA" ]] || { echo "READBACK MISMATCH: loadlist.txt ($got != $LOADLIST_SHA)"; bad=1; }
  got=$($CURL "$FTP$PLOADER_REMOTE" | shasum -a 256 | awk '{print $1}')
  if [[ "$got" != "$PLOADER_SHA" ]]; then
    $CURL "$FTP$PLOADER_REMOTE" -o "$RUN_DIR/ploader.elf.readback.bin" || true
    local rb_bytes; rb_bytes=$(stat -f%z "$RUN_DIR/ploader.elf.readback.bin" 2>/dev/null || echo 0)
    # SHT-strip tail-artifact class (launch10 law, simplified strictly):
    # readback len == max(p_offset+p_filesz) of the local file AND bytes
    # 64..readback-end byte-identical to the local file AND ELF magic intact.
    # Anything else = mismatch -> restore + refuse (never widen the law live).
    if [[ "$rb_bytes" == "$PLOADER_STRIP_LEN" ]] \
       && [[ "$(xxd -l 4 -p "$RUN_DIR/ploader.elf.readback.bin")" == "7f454c46" ]] \
       && cmp -s <(tail -c +65 "$RUN_DIR/ploader.elf.readback.bin") <(tail -c +65 "$PLOADER_ELF" | head -c $((PLOADER_STRIP_LEN - 64))); then
      echo "ploader readback: known SHT-strip tail-artifact class (len $rb_bytes, body 64.. identical, magic intact) — ACCEPTED"
    else
      echo "READBACK MISMATCH: ploader payload ($got != $PLOADER_SHA)"; bad=1
    fi
  fi
  if [[ "$bad" == "1" ]]; then
    echo "== ONE bounded restore attempt from saved pre-images =="
    $CURL -T "$PRE/ploader.elf.pre" "$FTP$PLOADER_REMOTE" || true
    $CURL "$FTP$PLOADER_REMOTE" | shasum -a 256 | awk '{print $1}' > "$RUN_DIR/ploader.elf.restored.sha256" || true
    die "readback verification failed; one restore attempted; read $RUN_DIR/ — never retry blindly"
  fi

  echo "== ini STILL byte-equal? =="
  got=$($CURL "$FTP$INI_REMOTE" | shasum -a 256 | awk '{print $1}')
  [[ "$got" == "$INI_SHA" ]] || { echo "$got" > "$RUN_DIR/ploader.ini.DRIFT.sha256"; die "ploader.ini drifted during the pass (this flow never wrote it) — reconcile, never blind-fix"; }

  {
    echo "{ \"verdict\": \"GO\", \"phases\": [\"A\",\"B\"],"
    echo "  \"ploader.elf\": \"$PLOADER_SHA\","
    echo "  \"loadlist.txt\": \"$LOADLIST_SHA\","
    echo "  \"ploader.ini.frozen\": \"$INI_SHA\","
    echo "  \"phase_c\": $([[ -n "${UE4SS_PRX:-}" ]] && echo '"same-pass"' || echo '"PENDING — run stage-c after composition lands; do NOT arm/launch before it"') }"
  } > "$RUN_DIR/go-nogo.json"
  echo "GO (phases A+B): $RUN_DIR/go-nogo.json"
}

phase_c() {
  local UE4SS_PRX=$1 UE4SS_SHA=$2 RUN_DIR=$3
  echo "== phase C: UE4SS candidate PRX via the REUSED launch10 flow =="
  cd "$TOOL_DIR"
  # controller fix 2026-10-05: --run-root is a GLOBAL option (before the
  # subcommand) per console_batch.py's argparse; the trailing form was
  # rejected with 'unrecognized arguments'.
  python3 console_batch.py --run-root "$RUN_DIR" stage-launch10 \
    --fself "$UE4SS_PRX" --expect-sha "$UE4SS_SHA"
}

case "${1:-}" in
  plan)
    host_preflight
    dump_plan
    echo "(plan mode: zero console contact — nothing above was sent)"
    ;;
  stage)
    shift
    UE4SS_PRX=""; UE4SS_SHA=""
    while [[ $# -gt 0 ]]; do
      case "$1" in
        --ue4ss-prx) UE4SS_PRX=$2; shift 2;;
        --ue4ss-sha) UE4SS_SHA=$2; shift 2;;
        *) die "unknown arg $1";;
      esac
    done
    host_preflight
    TAG=$(date +%Y%m%d-%H%M%S)
    RUN_DIR=$RUN_ROOT/$TAG-stage-next
    phase_a_b "$TAG"
    if [[ -n "$UE4SS_PRX" ]]; then
      [[ -f "$UE4SS_PRX" ]] || die "ue4ss prx not found: $UE4SS_PRX"
      [[ "$(sha "$UE4SS_PRX")" == "$UE4SS_SHA" ]] || die "ue4ss prx sha mismatch vs --ue4ss-sha (BUILD-ID law)"
      phase_c "$UE4SS_PRX" "$UE4SS_SHA" "$RUN_DIR"
    else
      echo "phase C PENDING: rerun with stage-c --ue4ss-prx P --ue4ss-sha S once composition lands"
      echo "DO NOT arm ploader or launch before phase C is staged (ONE launch per staged build)."
    fi
    ;;
  stage-c)
    shift
    UE4SS_PRX=""; UE4SS_SHA=""
    while [[ $# -gt 0 ]]; do
      case "$1" in
        --ue4ss-prx) UE4SS_PRX=$2; shift 2;;
        --ue4ss-sha) UE4SS_SHA=$2; shift 2;;
        *) die "unknown arg $1";;
      esac
    done
    [[ -n "$UE4SS_PRX" && -n "$UE4SS_SHA" ]] || die "stage-c requires --ue4ss-prx and --ue4ss-sha"
    [[ -f "$UE4SS_PRX" ]] || die "ue4ss prx not found: $UE4SS_PRX"
    [[ "$(sha "$UE4SS_PRX")" == "$UE4SS_SHA" ]] || die "ue4ss prx sha mismatch vs --ue4ss-sha (BUILD-ID law)"
    TAG=$(date +%Y%m%d-%H%M%S)
    RUN_DIR=$RUN_ROOT/$TAG-stage-c
    mkdir -p "$RUN_DIR"
    phase_c "$UE4SS_PRX" "$UE4SS_SHA" "$RUN_DIR"
    echo "phase C done — then (game CLOSED): arm ploader, USER launches. See STAGING-PLAN-NEXT.md."
    ;;
  post)
    POST_DIR=$RUN_ROOT/post-$(date +%Y%m%d-%H%M%S)
    mkdir -p "$POST_DIR"
    $CURL "$FTP/data/plugins/UE4SS.log" -o "$POST_DIR/UE4SS.log" || echo "UE4SS.log absent"
    $CURL "$FTP$R/runtime-status.txt" -o "$POST_DIR/runtime-status.txt" || echo "runtime-status.txt absent"
    $CURL "$FTP/data/townfall_supplier_composition.log" -o "$POST_DIR/module-log.txt" || echo "module log absent"
    $CURL "$FTP$INI_REMOTE" -o "$POST_DIR/ploader.ini"
    shasum -a 256 "$POST_DIR"/* | tee "$POST_DIR/SHASUMS.txt"
    frozen=$(cat "$RUN_ROOT"/*-stage-next/ploader.ini.frozen.sha256 2>/dev/null | head -1)
    if [[ -n "$frozen" ]] && [[ "$(sha "$POST_DIR/ploader.ini")" == "$frozen" ]]; then
      echo "ploader.ini STILL byte-equal post-launch (frozen $frozen)"
    else
      echo "WARNING: ploader.ini post-launch sha != staging freeze (or no freeze found) — reconcile, never blind-fix"
    fi
    ;;
  *)
    echo "usage: $0 {plan|stage|stage-c|post} [--ue4ss-prx P --ue4ss-sha S]" >&2
    exit 2;;
esac

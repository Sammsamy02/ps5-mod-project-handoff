#!/bin/bash
# run_stage_launch10.sh — controller wrapper for the LAUNCH-10 STAGING packet
# (worker-launch10-staging-packet, 2026-10-04). Thin wrapper around the JUST-FIXED
# batch tool: python3 console_batch.py stage-launch10 --fself <path> --expect-sha <sha>
#
# What the flow does (game CLOSED, LOCAL console only — ps5-host-not-configured.invalid):
#   l10.0  preflight: the rebuilt FSELF must be pinned (--fself + --expect-sha);
#          placeholders are a loud refusal — no unidentified build is ever staged
#   l10.0b G0 reachability: pldmgr 8084 /list_payloads (never probe alternates)
#   l10.1  INI FREEZE (read-only): ploader.ini must byte-equal the pinned 2-plugin
#          config sha 1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0
#   l10.2  PRE-IMAGE + .bak FIRST: current composition PRX read back (sha+size saved)
#          + console .bak written  <- ride.9c-style rollback artifacts
#   l10.3  UPLOAD EXACTLY ONCE: FSELF stored to its FINAL name
#          /data/plugins/modbridge_supplier_composition.prx
#   l10.4  READBACK CONTENT LAW vs the LOCAL FSELF (full identity, or ONLY the
#          ftpsrv SHT-strip tail-artifact class); mismatch -> OLD PRX restored
#          byte-exact from the pre-image, refusal
#   l10.5  ploader.ini STILL byte-equal 1216aab6… — ANY drift refuses + restores
#          the OLD PRX; this flow NEVER writes the ini
#   l10.6  fresh pre-launch baselines re-fetched into the run dir
#   l10.7  GO/NO-GO summary (l10-go-nogo.json)
#
# The tool NEVER launches the game: the next launch is USER-driven.
#
# Usage:
#   ./run_stage_launch10.sh plan    # dry-run: exact steps/commands, zero contact
#   ./run_stage_launch10.sh stage   # the staging run (FILL the two placeholders first)
#   ./run_stage_launch10.sh post    # AFTER the user-driven launch: evidence fetches

set -euo pipefail

TOOL_DIR="/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-console-batch-runner"
RUN_ROOT="$TOOL_DIR/runs/launch10"

# ---------------------------------------------------------------------------
# FILL AFTER THE LAUNCH-10 BUILD LANDS (the flow refuses placeholders):
#   <L10_FSELF_LOCAL_PATH> — local path of the rebuilt launch-10 composition FSELF
#   <L10_FSELF_SHA256>     — its pinned sha256 (BUILD-ID law)
# ---------------------------------------------------------------------------
L10_FSELF_LOCAL_PATH="<L10_FSELF_LOCAL_PATH>"
L10_FSELF_SHA256="<L10_FSELF_SHA256>"

# Pinned 2-plugin config the ini must STILL byte-equal (flow-enforced, twice):
# composition + path_test enabled — sha
# 1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0

FTP="ftp://ps5-host-not-configured.invalid:2121"
CURL="curl -sS --connect-timeout 10 --max-time 60"

case "${1:-}" in
  plan)
    cd "$TOOL_DIR"
    exec python3 console_batch.py plan --only stage-launch10
    ;;
  stage)
    if [[ "$L10_FSELF_LOCAL_PATH" == "<"* || "$L10_FSELF_SHA256" == "<"* ]]; then
      echo "REFUSED: fill L10_FSELF_LOCAL_PATH / L10_FSELF_SHA256 after the build" >&2
      echo "(the flow also refuses placeholders itself — no unidentified build is staged)" >&2
      exit 2
    fi
    cd "$TOOL_DIR"
    exec python3 console_batch.py stage-launch10 \
      --fself  "$L10_FSELF_LOCAL_PATH" \
      --expect-sha "$L10_FSELF_SHA256" \
      --run-root "$RUN_ROOT"
    # On GO: the newest run dir under $RUN_ROOT holds l10-go-nogo.json (verdict GO),
    # l10-readback-verdict.json, l10-ploader.ini.pre/.post.sha256, l10-composition
    # .prx.pre(+.sha256), l10-baselines.json + baseline-* files.
    # On NO-GO: read l10-go-nogo.json + l10-restore-verdict.json; the console .bak
    # (modbridge_supplier_composition.prx.$TAG.bak) is the operator rollback artifact.
    ;;
  post)
    # RUN ONLY AFTER THE USER-DRIVEN GAME LAUNCH (the tool never launches).
    POST_DIR="$RUN_ROOT/post-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$POST_DIR"
    cd "$TOOL_DIR"
    LATEST_RUN="$(ls -1dt "$RUN_ROOT"/20*-stage-launch10 2>/dev/null | head -1 || true)"
    echo "post-launch evidence dir: $POST_DIR"
    echo "compared against staging baselines from: ${LATEST_RUN:-<run dir>}"
    $CURL "$FTP/data/plugins/UE4SS.log"                                 -o "$POST_DIR/UE4SS.log"
    $CURL "$FTP/data/modbridge/roots/PPSA33286/runtime-status.txt"      -o "$POST_DIR/runtime-status.txt"
    $CURL "$FTP/data/townfall_runtime_aligned_entry.log"                -o "$POST_DIR/entry-log.txt"
    $CURL "$FTP/data/modbridge-supplier-diag.jsonl"                     -o "$POST_DIR/supplier-diag.jsonl" || \
      echo "supplier-diag.jsonl absent (valid if the diag sink never opened)"
    # the ini must STILL be the untouched 2-plugin config after the launch too
    $CURL "$FTP/data/plugins/ploader.ini"                               -o "$POST_DIR/ploader.ini"
    shasum -a 256 "$POST_DIR"/* | tee "$POST_DIR/SHASUMS.txt"
    echo "1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0  $POST_DIR/ploader.ini" | shasum -a 256 -c - \
      && echo "ploader.ini STILL byte-equal after launch (no ini republish happened)" \
      || echo "WARNING: ploader.ini drifted post-launch — reconcile, never blind-fix"
    [[ -n "$LATEST_RUN" ]] && \
      echo "compare: baseline-UE4SS.log / baseline-runtime-status.txt / baseline-entry-log.txt" \
           "in $LATEST_RUN vs the files above (fresh growth = this launch's evidence)"
    ;;
  *)
    echo "usage: $0 {plan|stage|post}" >&2
    exit 2
    ;;
esac

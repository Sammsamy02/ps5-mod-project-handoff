PATCHES — worker-injection-latency, 2026-10-05

ALL FILES IN THIS DIRECTORY ARE PROPOSAL-ONLY. Nothing here is built, staged, or
applied to the console. The controller owns all console changes and ploader builds.

Base tree: pinned ploader source
  .superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c
  (yourphonelinging0x0/ps5-plugin-loader@1409a94, sha256 2b521cd7…, byte-verified).

Series (apply in order with `patch -p1` from a checkout of the ploader repo root,
file main.c):

  01-deadfile-earlyskip.diff   mark missing files dead for the invocation and skip
                               them BEFORE the readiness wait + ptrace attach
                               (saves ~2 s per missing entry per invocation;
                               removes repeated FAIL toasts). Latency R4.
  02-hoist-readiness.diff      one readiness wait per title (worst mode) instead of
                               one ~1.6 s minimum wait per plugin (main.c:784 inside
                               the per-plugin loop). Saves ~1.6 s x (n_plugins-1)
                               per invocation. Latency R5.
  03-steplog.diff              append one line per step to /data/plugins/ploader.log
                               with now_ms() deltas (toasts are ephemeral; this makes
                               composition's module_start duration and every stage
                               measurable on the next launch). Latency R3 (measures).
  04-quiescent-exit.diff       exit early once a matching title was processed and no
                               new process appeared for 15 s, instead of idling to
                               the 120 s cap after unretryable failures. Never
                               triggers before the game is first seen, so
                               late-game-start behavior is preserved. Latency R6.

Verified 2026-10-05: all four diffs applied in series with `patch -p1` against a copy
of the pinned original/main.c with zero offsets/rejects, and the final result is
byte-identical to the scripted transformation target (full compile requires PS5 SDK
headers, not available on this host). See REPORT.md.

Deliberately NOT proposed here:
- lowering pt_call's 30 s module_start cap (main.c:280): would turn slow-but-working
  module_starts into FAILs more often; measure first (03), decide later.
- trimming wait_stable's mode-2 stability gate (1000 ms + 600 ms settle, main.c:475):
  protects injection into a still-morphing module space; ~1 s/plugin upside not worth
  the risk.
- any change to entry/composition PRX behavior: ours-side changes are specified in
  OURS-SIDE-NOTE.md for the owners of those builds.

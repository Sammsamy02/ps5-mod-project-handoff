# Agent CH results pack — pump timing (Task A) + console-results doc (Task B)

Date: 2026-10-08 evening round. READ-ONLY over trees/logs; outputs written:
`CONSOLE-RESULTS-20261008.md` (this directory) and this file.

## Task A — marshal pump live timing (verdict: ADEQUATE, not an l26 tuning item)

Method: FIFO-matched every `[l22] ExecuteInGameThread queued/drained` pair by
(state, ref) in all six logs; parsed `r3_marshal_pumped` / `eigt_queue_depth`
from the B supplier-diag jsonl (t_ms millisecond clock); decoded NUL-interleaved
wide-char Lua lines for the zzdiag chunks.

Numbers:

- Warm-up (boot enqueue → first drain): B-final **12,173ms** (t_ms 538964 →
  r3_marshal_first_invoke 551137; log 18:36:29→18:36:41); zzdiag-v2 **9s**
  (19:09:37→19:09:46, post-reboot cold boot); A-live2 / A-live3 / zzdiag1
  drained **same wallclock second** as enqueue (carrier already firing).
- Steady state: **43/43 post-warm-up pairs same-second** (1s log resolution);
  totals 46 queued / 46 drained / 0 stranded / 0 orphan drains.
- ms upper bounds (B-final jsonl, enqueue→next heartbeat; heartbeat throttled
  at ~1,001ms so these are bounds): in-game window 320/367/309/369/325/309/319ms
  and one direct **15ms** bound; title-screen window 708/788ms.
- Heartbeat: 280 events / 280s continuous; worst gap 1,248ms (only ~250ms over
  the throttle floor) — carrier never stalled meaningfully.

Verdict: **adequate for a spawn-menu UX (sub-500ms) after warm-up.** The drain
runs synchronously inside the armed BP-override carrier invocation
(Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation / UserWidget:Tick
override family, 4–8 instances armed per session), so in-game latency ≈ one
carrier frame (17ms @60 / 33ms @30); all in-game bounds ≤369ms. The 9–12s
first-drain is one-time boot warm-up (carriers only fire once the engine
reaches their tick phase).

Carrier-dependence quantified: if every armed carrier instance stops being
invoked (full tick pause, or level transition teardown before the ≤1-scan/s
lazy re-arm), queued actions strand forever (no timeout by design). A 5-action
menu flow at 30fps ≈ 165ms serial worst case — fine. Only optional l26
evidence item (not tuning): a per-drain `eigt_drained_dt` diag event, since
`r3_marshal_pumped` is 1s-throttled and tonight's per-drain figures are bounds.

## Task B — CONSOLE-RESULTS-20261008.md assembled (this directory)

Contents: 5-run matrix; three findings (crash family SOLVED; UFunction nullptr
blocker; zzdiag-v2 FindFunction crash); instrumentation dividends; settings
deviation note; l26 open items; pump-timing appendix. Key facts behind it:

- Run mapping confirmed: B-final/live1 = one B session (identical first
  events); A-live2 = A original (cache-off, 18:43); A-live3 = A cache-on
  (18:50, flip at 18:47 per l25-settings-cacheon-1947.ini mtime); zzdiag1
  (18:58, v1); zzdiag2 (19:09, v2, crashed). B and A receipts show identical
  daemon pids → no reboot between B and A.
- Crash family SOLVED evidence: l24 log ends at "Adding shutdown listener";
  tonight A passes the same endpoint (Add enter → header Num 5→6→7(→8), Max 22,
  growth_required=0, returned in 11–12ms) and the engine dispatches
  NotifyUObjectDeleted into our listeners (1/4/2/2 lines, up to calls=12)
  with 0 klog fatals (B and A taps).
- UFunction nullptr: E0–E11 chunks + `[l22] property:` line in zzdiag2 at
  19:09:53; FindFirstOf works (cache-on), GetItemIDFromDeprecatedEnum returns
  nullptr UFunction → Scan failed in ALL five runs (16/6/11/3/2) → no menu.
- zzdiag-v2 crash: drained ref=6 → property error → E0–E11 → log ends
  mid-chunk = CE-108255-1; no klog tap live; console rebooted (eboot monitor
  00:15:31Z daemon-missing transitions).
- Instrumentation dividends: identical live GMalloc triple
  (0x970add8/0x97089b8/vptr 0x82c26e0, +0x40 true Realloc) in all runs
  matching AB2; real-array header dumps; engine-callback counts; 280-heartbeat
  pump trace; copyin "sink bank retired" state line every session;
  pad_l3r3_f7_dispatched=15.
- GMalloc values identical across B and all A runs — variant difference is
  registration-skip only, as designed.

## Run-matrix one-liners (per run)

1. **B (original)**: admitted, init clean, listeners SKIPPED by design, spawner
   started, 25 keybind fires, EIGT 16/16 (12.2s warm-up then same-second), no
   crash, clean end — proved the skip arm and the full pad→pump chain.
2. **A (original)**: admitted, init clean, 2 listener registrations on the real
   array (11ms), 1 engine callback, 8 keybind fires, EIGT 6/6 same-second, no
   crash — the l24 crash point passes with real registration; scan still
   failed (cache off).
3. **A (cache-on)**: admitted, 3 registrations (Num→8), 4 engine-callback
   lines (calls=12), 15 keybind fires, EIGT 11/11 same-second, no crash —
   cache flip legal and safe; scan still failed (nullptr UFunction, not
   FindFirstOf).
4. **A+cache+zzdiag-v1**: admitted, spawner+zzdiag started, 2 probe cycles
   drained same-second, chunks mostly eaten by the writer, no crash — probe
   harness works, writer limitation quantified.
5. **A+cache+zzdiag-v2**: admitted, 9s pump warm-up, one probe press drained
   ref=6 → nullptr-UFunction chunks E0–E11 → log ends mid-chunk = CE-108255-1
   crash (FindFunction probe), console rebooted after — the only crash of the
   night, deliberate-diagnostic, removed.

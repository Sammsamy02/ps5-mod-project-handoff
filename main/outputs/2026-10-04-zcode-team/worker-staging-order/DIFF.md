--- /tmp/staging-order-snap/console_batch.py.pre-staging-order.py	2026-10-05 07:19:28
+++ worker-console-batch-runner/console_batch.py	2026-10-05 07:38:31
@@ -12,7 +12,11 @@
 bootstrap reconciliation + stage-settings), and by
 worker-ninth-slot-ini-adoption with the NINTH-SLOT law (the cc1 carrier
 probe appended after the igf =0 line: 9 entries / 8 registered, per
-worker-carrier-probe-build/STAGE-PLAN.md). This tool executes, step by step
+worker-carrier-probe-build/STAGE-PLAN.md), and by
+worker-staging-order with the STAGING-ORDER LAW (launch9 incident fix:
+upload + verify every enabled plugin file FIRST, publish the enabling
+ploader.ini LAST, pre-publish guard, restore-ini rollback helper). This tool
+executes, step by step
 with gates, the plans frozen by:
   * worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md (PREFERRED single DLL
     run) + worker-dll-8fam-combined/EXPECTED-DELTA-8FAM.md
@@ -48,7 +52,11 @@
                    via the ONE bootstrap whole-file STOR (only when the pre-image needs
                    the historical line moves) + idempotent content-anchored ensure ops,
                    gated byte-exact against ploader.ini.canonical (8 registered) —
-                   supersedes stage-qdrv1/stage-observer inside `all`
+                   supersedes stage-qdrv1/stage-observer inside `all`.
+                   STAGING-ORDER LAW: ALL five PRXs are uploaded + law-verified FIRST;
+                   the enabling ini is published LAST, only after the pre-publish
+                   guard proves every enabled entry's file is present (an interrupted
+                   staging can never leave missing plugins enabled — launch9 fix)
   stage-observer   observer ridealong ONLY (legacy partial path as delivered):
                    PRX upload + SHT-strip readback law + idempotent ini append of
                    ONLY the observer line (coordinates with a qdrv1 line already
@@ -58,6 +66,14 @@
                    staged pair, upload the v3 package bytes, byte-exact readback BOTH
   rollback-ridealongs rollback per STAGE-PLAN-COMBINED §7: flip BOTH ridealong lines
                    to = 0 (single-character edits) with .bak + byte-verified readback
+  restore-ini      ORDER-LAW rollback helper: restore ploader.ini BYTE-EXACT from a
+                   locally saved pre-publish copy (default: the controller's saved
+                   9-entry canonical at outputs/2026-10-04-zcode-controller/
+                   launch9-evidence/ploader-ini-9entry-restore.txt) — freeze-classifies
+                   the console pre-state (never overwrites divergent content blind),
+                   runs the SAME pre-publish guard every ini publish obeys (refuses
+                   an ini whose enabled plugin files are missing), then STOR +
+                   byte-exact RETR readback
   game-evidence    post-launch evidence: post_launch_check + rung_check + CP0-CP7 checks
                    + the probe verdict sinks (xwp jsonl + fc1pool jsonl + cc1 census jsonl)
   all              recommended session order: dll8 -> e2e -> stage-ridealongs -> 
@@ -72,6 +88,12 @@
     skips every step that depends on a failed group (no re-dispatch, no replay)
   * dispatch steps are dispatched EXACTLY ONCE; on ambiguity the tool stops and
     records (reconcile-first law from the plans and AGENTS.md)
+  * STAGING-ORDER LAW (launch9 incident): every flow that publishes ploader.ini
+    uploads + verifies EVERY enabled plugin file FIRST (the five staged PRXs via
+    the SHT-strip readback law, pre-existing enabled lines via fresh presence
+    proofs) and publishes the ini LAST; the pre-publish guard loudly refuses any
+    publish that would enable a missing/0-byte plugin file. An interrupted
+    staging therefore always leaves the old (safe) ini on the console.
   * the tool never re-sends, never widens a gate, never writes outside its run dir
     and the console paths each plan explicitly owns.
 
@@ -335,7 +357,33 @@
     #   8-entry law: 400 B, sha bbc34c16621ad337632d0ff29a116b0aa6e86e45ad219e8ef7539b369a5a6fe1,
     #   7 registered (worker-batch-final-ini-adoption, delivered b00b3a69…)
     "post_launch_ok_lines_expected": 8,      # EXACTLY 8 `OK (<ms>): <file>`, ZERO FAIL
+}
+
+# ploader.ini RESTORE helper (worker-staging-order, 2026-10-04, launch9 incident):
+# restore the ini BYTE-EXACT from a locally saved pre-publish copy. The default
+# source is the controller's saved 9-entry canonical (launch9-evidence, sha equals
+# the pinned ploader.ini.canonical). The recorded POST-INCIDENT console state is
+# the 3-enabled minimal ini (sha 32d0c0d9…) — the ONLY non-noop pre-state the
+# helper accepts without an explicit operator override; anything else is
+# divergent/unowned content and is never overwritten blind (AGENTS.md).
+RESTORE_INI = {
+    "ini": "/data/plugins/ploader.ini",
+    "src_default": (WS + "/outputs/2026-10-04-zcode-controller/"
+                    "launch9-evidence/ploader-ini-9entry-restore.txt"),
+    "src_default_sha256": "fd9c73937b2b1e45d0cc22721c3e03d977cf027a7a335e5599577b2b7f7c3507",
+    "src_default_bytes": 440,
+    "classified_pre_states": {
+        # post-incident console state recorded by the controller after the manual
+        # recovery (3-enabled minimal ini); restoring over it is sanctioned
+        "post-incident-minimal-3enabled":
+            "32d0c0d9703e86bfc138e80a0256bdbcaa3f43174bf576112b443692793f2a31",
+    },
 }
+
+# Runtime override for the restore-ini flow (set by main() from --ini-src /
+# --allow-divergent-pre; defaults keep the flow deterministic in plan/selftest).
+RESTORE_SRC_PATH = RESTORE_INI["src_default"]
+RESTORE_ALLOW_DIVERGENT_PRE = False
 
 # Settings+loadlist lockstep restage pair (worker-settings-v3-package/package-v3;
 # bytes re-verified on disk 2026-10-04 before pinning; targets + freeze/readback
@@ -1984,8 +2032,8 @@
 
     fl.step(tag + ".3", "Step 2a — manage:check (ok / folder_exists / file_exists)", "verify",
             ["response contains ok / folder_exists / file_exists for the new entry"],
-            ["curl -sS %s/manage:check?filename=%s.elf" % (HTTP, alias)],
-            curl_get(HTTP + "/manage:check?filename=%s.elf" % alias, outfile="manage_check.%s.txt" % tag, expect_substr="ok", name="manage:check"))
+            ["curl -sS %s/manage:check?filename=%s.elf" % (HTTP, d["alias"])],
+            curl_get(HTTP + "/manage:check?filename=%s.elf" % d["alias"], outfile="manage_check.%s.txt" % tag, expect_substr="ok", name="manage:check"))
 
     fl.step(tag + ".4", "Step 2b — FTP readback identity gate (readback law)", "verify",
             ["readback sha256 == %s" % d["sha"],
@@ -2558,6 +2606,23 @@
             ["curl -sS -T %s %s%s" % (q["prx"], FTP, q["target_prx"]),
              "curl -sS %s%s | shasum -a 256" % (FTP, q["target_prx"])],
             upload_fn)
+
+    def guard_qdrv1_fn(ctx):
+        # the qdrv1.3 readback already proved qdrv1's bytes; the shared guard adds
+        # the presence proof for every OTHER enabled entry of the new ini
+        return ini_publish_guard_fn("qdrv1")(ctx)
+
+    fl.step("qdrv1.3b", "ORDER-LAW PRE-PUBLISH GUARD — every enabled entry of ploader.ini.new must "
+                        "have a verified-present file on the console BEFORE the ini STOR (qdrv1.4): "
+                        "qdrv1 was just verified byte-exact (qdrv1.3); the composition entry (and "
+                        "any other enabled entry) via fresh RETR presence proofs. ANY gap = loud "
+                        "refusal, ini NOT published (launch9 fix)", "upload",
+            ["guard REFUSES the publish on ANY missing/0-byte enabled entry (order law §2)",
+             "no-op decision -> nothing will be published -> guard not required (SKIP)",
+             "same dependency group as the STOR step: a refusal also skips it under "
+             "--continue-on-failure"],
+            ["curl -sS %s%s -o <run-dir>/guard-present-composition.prx" % (FTP, QDRV1["composition_line"])],
+            guard_qdrv1_fn)
 
     def stor_ini_fn(ctx):
         new_local = os.path.join(ctx.run_dir, "ploader.ini.new")
@@ -2783,60 +2848,40 @@
 
 
 def final_ini_decision_step(label):
-    """DEFINITIVE §3.3 sequence, step 1: BOOTSTRAP-or-ensure decision from the
-    Step-0 pre-image. Runs final_ini_plan; on the bootstrap status performs the
-    ONE sanctioned whole-file STOR of the canonical template + RETR byte-exact
-    diff, gated as its own INI-BOOTSTRAPPED verdict (never a silent rewrite);
-    ensure-only/noop statuses store nothing here."""
+    """DEFINITIVE §3.3 sequence, step 1 — STAGING-ORDER LAW revision: the
+    BOOTSTRAP-or-ensure DECISION ONLY. Runs final_ini_plan against the Step-0
+    pre-image, saves the LOCAL ploader.ini.new + decision JSON, and stops there.
+    NO ini byte reaches the console from this step: the ONE sanctioned bootstrap
+    whole-file STOR (and the ensure-append STOR) happen ONLY at the publish step,
+    AFTER every enabled plugin file has been uploaded + law-verified and the
+    pre-publish guard passed (order law: an interrupted staging must never leave
+    missing plugins enabled — the launch9 incident published the 9-entry ini at
+    this point and halted mid-flow with 4 enabled entries missing)."""
     def fn(ctx):
         pre = ctx.state.get("pre")
         status, new_text, note = final_ini_plan(pre, FINAL_ORDER)
         decision = {"flow": label, "law": "FINAL-INI-LAYOUT.md §3.3 (definitive 9-entry, "
                     "ninth-slot revision)",
-                    "status": status, "appended": note if status == "append" else []}
+                    "status": status, "appended": note if status == "append" else [],
+                    "publish": "DEFERRED: the ini STOR runs ONLY at the publish step, after the "
+                               "5 PRX uploads + SHT-strip verdicts + the pre-publish guard "
+                               "(staging-order law)"}
+        ctx.save("ploader.ini.new", new_text if status != "stop" else "")
         ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
         if status == "stop":
-            ctx.save("ploader.ini.new", "")
             return "FAIL", "AMBIGUOUS ini state — %s | pre-image preserved in the run dir; reconcile, never blind-edit" % note
         if status == "bootstrap":
-            # the ONE sanctioned whole-file STOR (historical line moves), with the
-            # .bak already written by the Step-0 pre-image step; RETR byte-exact diff
-            ctx.save("ploader.ini.new", new_text)
-            ctx.save("ini-bootstrap-verdict.txt", "INI-BOOTSTRAPPED: ONE whole-file STOR of "
-                     "ploader.ini.canonical (440 B, sha %s); .bak + byte-exact RETR diff "
-                     "follow" % FINAL_INI["canonical_sha256"])
-            rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T",
-                                     os.path.join(ctx.run_dir, "ploader.ini.new"),
-                                     FTP + FINAL_INI["ini"]], "BOOTSTRAP STOR canonical ploader.ini")
-            if rc != 0:
-                return "FAIL", "bootstrap STOR failed rc=%d — STOP, reconcile (never a second silent attempt)" % rc
-            p = os.path.join(ctx.run_dir, "ploader.ini.readback")
-            rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + FINAL_INI["ini"], "-o", p],
-                                "bootstrap RETR readback")
-            if ctx.dry:
-                ctx.state["pre"] = new_text
-                decision["status"] = "bootstrap-applied"
-                decision["stor"] = "done"
-                ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
-                return "PASS", "dry"
-            if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
-                return "FAIL", "bootstrap readback failed rc=%d — STOP and reconcile" % rc2
-            if open(p, errors="replace").read() != new_text:
-                return "FAIL", ("bootstrap readback != canonical bytes — the ONE sanctioned "
-                                "rewrite did not land byte-exact; STOP, reconcile (never a "
-                                "second silent attempt)")
-            ctx.state["pre"] = new_text
-            decision["status"] = "bootstrap-applied"
-            decision["stor"] = "done"
+            decision["stor"] = "bootstrap-pending"
             ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
-            return "PASS", "INI-BOOTSTRAPPED: canonical 9-line layout stored whole-file ONCE + readback byte-exact (%s)" % note
-        # "append" (ensure ops needed) or "noop" (already canonical)
-        ctx.save("ploader.ini.new", new_text if status == "append" else (pre or ""))
+            return "PASS", ("BOOTSTRAP REQUIRED (publish DEFERRED): the pre-image needs the ONE sanctioned "
+                            "whole-file STOR of ploader.ini.canonical — it fires ONLY at the publish step, "
+                            "after the 5 PRX law verdicts + the pre-publish guard (%s)" % note)
         decision["stor"] = "pending" if status == "append" else "none"
         ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
         if status == "noop":
             return "PASS", "pre-image is ALREADY byte-exact canonical — no-op (change nothing, store nothing)"
-        return "PASS", "INI-ENSURE-ONLY: content-anchored ensure ops ready (+%d line(s) %s)" % (
+        return "PASS", "INI-ENSURE-ONLY (publish DEFERRED): content-anchored ensure ops ready (+%d line(s) %s); " \
+                       "the STOR fires only after the 5 PRX law verdicts + the pre-publish guard" % (
             len(decision["appended"]), decision["appended"])
     return fn
 
@@ -2868,9 +2913,15 @@
 
 
 def final_ini_stor_fn(label):
-    """DEFINITIVE §3.3 sequence, step 3: STOR the verified local file + fresh
-    RETR byte-exact (skipped when the decision was a no-op; ALREADY DONE and
-    readback-verified when the bootstrap branch fired)."""
+    """DEFINITIVE §3.3 sequence, step 3 — THE PUBLISH (staging-order law: LAST).
+    The ONLY step of the flow that stores ploader.ini to the console; it runs
+    AFTER all five PRX uploads + SHT-strip law verdicts + the pre-publish guard:
+      * stor == "pending" (ensure-only): STOR the verified appended ini;
+      * stor == "bootstrap-pending": the ONE sanctioned whole-file STOR of
+        ploader.ini.canonical, gated as its own INI-BOOTSTRAPPED verdict
+        (never a silent rewrite, never a second attempt);
+      * stor == "none" (no-op): nothing is stored (idempotent re-run).
+    Every publish shape reads the ini back and requires byte-exact equality."""
     def fn(ctx):
         decision = {}
         try:
@@ -2880,28 +2931,149 @@
         stor = decision.get("stor")
         if stor == "none":
             return "PASS", "edit was a no-op — ini untouched on console (no STOR)"
-        if stor == "done":
-            return "PASS", "bootstrap branch already stored + readback-verified the canonical bytes (INI-BOOTSTRAPPED)"
         new_local = os.path.join(ctx.run_dir, "ploader.ini.new")
         if not os.path.exists(new_local) or os.path.getsize(new_local) == 0:
             return "FAIL", "edited ini missing (%s decision step must run first)" % label
-        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", new_local, FTP + FINAL_INI["ini"]],
-                           "STOR edited ploader.ini")
+        bootstrap = stor == "bootstrap-pending"
+        stor_label = "BOOTSTRAP STOR canonical ploader.ini" if bootstrap else "STOR edited ploader.ini"
+        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", new_local, FTP + FINAL_INI["ini"]], stor_label)
         if rc != 0:
-            return "FAIL", "ini STOR failed rc=%d" % rc
+            return "FAIL", "%s failed rc=%d — the console KEEPS its previous ini (order law: no partial " \
+                           "publish); STOP, reconcile, never a second blind attempt" % (stor_label, rc)
         p = os.path.join(ctx.run_dir, "ploader.ini.readback")
         rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + FINAL_INI["ini"], "-o", p],
-                            "ini readback")
-        if ctx.dry:
-            return "PASS", "dry"
+                            "publish readback")
         if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
             return "FAIL", "ini readback failed rc=%d — do not launch on an unverified ini" % rc2
         if open(p, errors="replace").read() != open(new_local, errors="replace").read():
-            return "FAIL", "ini readback != edited bytes — nothing else may change; STOP and reconcile"
-        return "PASS", "ini STOR + readback byte-identical (canonical end-state on console)"
+            return "FAIL", "ini readback != published bytes — nothing else may change; STOP and reconcile"
+        if bootstrap:
+            ctx.save("ini-bootstrap-verdict.txt", "INI-BOOTSTRAPPED: ONE whole-file STOR of "
+                     "ploader.ini.canonical (440 B, sha %s), published LAST per the staging-order "
+                     "law: after the 5 PRX uploads + SHT-strip law verdicts + the pre-publish "
+                     "guard; readback byte-exact" % FINAL_INI["canonical_sha256"])
+            decision["status"] = "bootstrap-applied"
+            decision["stor"] = "done"
+            ctx.state["pre"] = open(new_local, errors="replace").read()
+            ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
+            return "PASS", "INI-BOOTSTRAPPED: canonical 9-entry layout stored whole-file ONCE " \
+                           "(publish LAST, post-guard) + readback byte-exact"
+        return "PASS", "ini STOR + readback byte-identical (canonical end-state on console; " \
+                       "published LAST per the staging-order law)"
     return fn
 
 
+# --- STAGING-ORDER LAW (worker-staging-order, launch9 incident fix) -------------
+# LAW: upload + verify EVERY enabled plugin file FIRST; publish the enabling
+# ploader.ini LAST. The launch9 incident: stage-ridealongs published the 9-entry
+# ini (8 plugins enabled) BEFORE verifying the plugin files, halted at an xwp
+# readback check, and left 4 enabled entries (fc1pool, qh_driver, observer,
+# carrier) MISSING on the console — the next launch burned ~10-15 min of user
+# time on the dead entries. An interrupted staging must therefore always leave
+# the console ini in a state where every ENABLED entry's file is present (the
+# old ini), enforced by a pre-upload guard before every ini publish.
+
+STAGED_PRX_KEYS = (("xwp", XWP_ARMED), ("fc1", FC1POOL), ("qdrv1", QDRV1),
+                   ("observer", OBS), ("cc1", CC1))
+
+
+def ini_enabled_paths(new_text):
+    """Every path the ini would ENABLE at load (mode != 0; the plans never write
+    mode-1 lines, so for every combined-shape ini this equals the `= 2` set).
+    Also understands the legacy numbered [modules] shape (STAGE-PLAN-QDRV1 §1),
+    where every listed module is enabled. Returns (enabled_paths, problems);
+    an unparseable ini yields problems (never a silently empty enable set)."""
+    entries, _header_count, fatal = _parse_combined_ini(new_text)
+    if not fatal:
+        return [p for p, m, _r in entries if m != 0], []
+    paths = []
+    for ln in new_text.splitlines():
+        m = NUM_MODULE_RE.match(ln)
+        if m:
+            paths.append(m.group(2))
+    if paths:
+        return paths, []
+    return [], [fatal]
+
+
+def ini_publish_guard_fn(label, new_ini_name="ploader.ini.new"):
+    """ORDER-LAW pre-upload guard: BEFORE any ploader.ini STOR, every enabled
+    entry of the NEW ini must have a VERIFIED-PRESENT file on the console:
+      * the five staged PRXs — THIS run's SHT-strip law verdict JSON must record
+        ok (upload + readback law; the readback-only verification on a no-op
+        decision satisfies it equally — same established content law);
+      * every OTHER enabled entry (the historical pre-existing lines this tool
+        never uploads) — a fresh RETR presence proof: fetchable + non-empty +
+        sha recorded. A 0-byte or unfetchable file is exactly the launch9
+        missing-plugin signature.
+    ANY gap = loud refusal (FAIL, same 'ini' dependency group as the publish
+    step): the ini is NOT published and the console keeps the old ini. A no-op
+    decision publishes nothing, so the guard records SKIP-GUARD-NOT-NEEDED."""
+    def key_of(path):
+        for key, d in STAGED_PRX_KEYS:
+            if d["target_prx"] == path:
+                return key
+        return "pre-existing"
+
+    def fn(ctx):
+        new_local = os.path.join(ctx.run_dir, new_ini_name)
+        if not os.path.exists(new_local) or os.path.getsize(new_local) == 0:
+            return "FAIL", ("%s pre-publish guard: %s missing (the decision step must run "
+                            "first) — no ini publish" % (label, new_ini_name))
+        try:
+            decision = json.load(open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")))
+        except Exception:
+            decision = {}
+        if decision.get("status") == "noop":
+            return "PASS", ("SKIP-GUARD-NOT-NEEDED — decision was a no-op: nothing will be "
+                            "published, the console ini is untouched")
+        if ctx.dry:
+            return "PASS", "dry"
+        new_text = open(new_local, errors="replace").read()
+        enabled, problems = ini_enabled_paths(new_text)
+        if problems:
+            return "FAIL", "%s pre-publish guard: new ini unparseable: %s" % (label, problems[0])
+        verified = set()
+        for key, d in STAGED_PRX_KEYS:
+            try:
+                v = json.load(open(os.path.join(ctx.run_dir, "%s-readback-verdict.json" % key)))
+            except Exception:
+                continue
+            if v.get("ok") is True:
+                verified.add(d["target_prx"])
+        presence, missing = [], []
+        for p in enabled:
+            if p in verified:
+                presence.append({"path": p, "proof": "this-run SHT-strip readback law verdict (%s)" % key_of(p)})
+                continue
+            p_out = os.path.join(ctx.run_dir, "guard-present-%s" % os.path.basename(p))
+            rc, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + p, "-o", p_out],
+                               "guard RETR %s" % p)
+            if rc != 0 or not os.path.exists(p_out) or os.path.getsize(p_out) == 0:
+                missing.append(p)
+                continue
+            sha, n = local_sha(p_out)
+            presence.append({"path": p,
+                             "proof": "RETR presence (%s bytes, sha %s)" % (n, (sha or "?")[:12])})
+        ctx.save("%s-publish-guard.json" % label, json.dumps(
+            {"flow": label, "law": "staging-order pre-publish guard (launch9 fix)",
+             "enabled_entries": enabled, "law_verified": sorted(verified & set(enabled)),
+             "presence_proofs": presence, "missing": missing,
+             "decision_status": decision.get("status")}, indent=2))
+        if missing:
+            return "FAIL", ("%s PRE-PUBLISH GUARD REFUSED the ini publish: %d enabled entry(s) have "
+                            "NO verified-present file on the console: %s — publishing would ENABLE "
+                            "MISSING PLUGINS at the next launch (launch9 incident class). The old ini "
+                            "stays; upload + verify the files first (stage-ridealongs does this "
+                            "before its publish), then re-run."
+                            % (label, len(missing), ", ".join(missing)))
+        return "PASS", ("PRE-PUBLISH GUARD: all %d enabled entr(ies) verified present (%d via "
+                        "this-run SHT-strip law verdicts, %d via fresh RETR presence proofs) — "
+                        "ini publish may proceed" % (len(enabled), len(verified & set(enabled)),
+                                                     len(enabled) - len(verified & set(enabled))))
+    return fn
+
+
 def build_stage_ridealongs():
     """DEFINITIVE return-session staging (FINAL-INI-LAYOUT.md §3 + the
     CONSOLE-BATCH-CONSTANTS-BRIEF, ninth-slot revision per
@@ -2911,13 +3083,22 @@
     via the ONE bootstrap whole-file STOR (only when the pre-image needs the
     historical line moves) + idempotent content-anchored ensure ops, gated
     BYTE-EXACT against ploader.ini.canonical (8 registered).
-    Supersedes the 6-entry combined staging inside `all`."""
+    Supersedes the 6-entry combined staging inside `all`.
+    STAGING-ORDER LAW (worker-staging-order, launch9 incident fix): the ini
+    decision is LOCAL-ONLY; ALL FIVE PRXs are uploaded + law-verified FIRST
+    (ride.6..ride.9b); the pre-publish guard (ride.9c) then proves every
+    enabled entry's file is present; ONLY THEN does the publish step (ride.4)
+    store the enabling ini — an interrupted staging can never leave missing
+    plugins enabled. (Step id ride.4 deliberately keeps its historical handle;
+    it now runs LAST before the handoff.)"""
     fl = Flow("stage-ridealongs")
     fl.notes = ("DEFINITIVE return-session staging (FINAL-INI-LAYOUT §3, ninth-slot revision): "
                 "5 PRXs (xwp-armed igd1-6ebab07f2111, fc1pool fc1pool1-78508a2e2940, qdrv1, "
                 "observer, cc1-carrier cc1-0864d65c1baf) + the FINAL 9-entry ploader.ini "
                 "(8 registered; bootstrap whole-file STOR only when the historical line moves "
-                "require it; ensure ops otherwise), byte-exact vs ploader.ini.canonical.")
+                "require it; ensure ops otherwise), byte-exact vs ploader.ini.canonical. "
+                "STAGING-ORDER LAW: decision (local) -> 5x upload+law-verify -> pre-publish "
+                "guard -> ini publish LAST (launch9 fix: never enable missing plugins).")
 
     def prefix_gate_fn(ctx):
         # host-side cross-check: the computed prefix of each pinned bare ELF must
@@ -2999,20 +3180,23 @@
              "curl -sS -T <run-dir>/ploader.ini.pre %s%s.$TAG.bak" % (FTP, FINAL_INI["ini"])],
             ini_preimage_fn("final"))
 
-    fl.step("ride.2b", "§3.3 BOOTSTRAP decision — canonical-order pre-image -> ensure-only; "
-                       "historical aligned_entry/path_test-before-composition shape -> the ONE "
-                       "sanctioned whole-file STOR of ploader.ini.canonical + RETR byte-exact "
-                       "diff, verdict INI-BOOTSTRAPPED (never a silent rewrite, never a second "
-                       "attempt)", "ini",
+    fl.step("ride.2b", "§3.3 BOOTSTRAP/ensure DECISION (LOCAL ONLY — publish DEFERRED, order law): "
+                       "canonical-order pre-image -> ensure-only; historical "
+                       "aligned_entry/path_test-before-composition shape -> the ONE sanctioned "
+                       "whole-file STOR of ploader.ini.canonical is ARMED as bootstrap-pending; "
+                       "NO ini byte reaches the console from this step — the publish fires only at "
+                       "ride.4, AFTER the 5 PRX law verdicts + the pre-publish guard (launch9 fix: "
+                       "the incident published the ini HERE and halted mid-flow with 4 enabled "
+                       "entries missing)", "ini",
             ["STOP classes kept: duplicate / missing pre-existing / mode divergence / unknown "
              "line / unparseable — reconcile per rollback-runbook §4.3",
-             "bootstrap is gated as its own INI-BOOTSTRAPPED verdict with the .bak + "
-             "byte-exact RETR diff; ensure-only path stores nothing here",
+             "publish DEFERRED: this step stores NOTHING to the console (order law)",
              "cc1 ensure op (STAGE-PLAN §4): insert '/data/plugins/modbridge_carrier.prx = 2' "
              "immediately AFTER the '/data/plugins/modbridge_ingame_facts.prx = 0' line "
              "(content anchor, next free index — never displacing an existing line)",
-             "canonical: 9 entries, 8 registered, probes first, igf=0 second-to-last, carrier last"],
-            ["diff <run-dir>/ploader.ini.pre <run-dir>/ploader.ini.new  # bootstrap: whole file; ensure: the inserted line(s)"],
+             "canonical: 9 entries, 8 registered, probes first, igf=0 second-to-last, carrier last",
+             "the INI-BOOTSTRAPPED verdict is written at ride.4 when the guarded publish lands"],
+            ["diff <run-dir>/ploader.ini.pre <run-dir>/ploader.ini.new  # LOCAL diff: bootstrap = whole canonical; ensure = the inserted line(s)"],
             final_ini_decision_step("ridealongs"))
 
     fl.step("ride.3", "§3.3 byte-exact end-state proof of the LOCAL file BEFORE any STOR "
@@ -3021,19 +3205,11 @@
             ["byte-exact equality with ploader.ini.canonical replaces the legacy "
              "'5 registered' count gate",
              "the final verifier (worker-final-ini-layout/verify_final_ini_layout.py) must "
-             "print FINAL-INI-OK"],
+             "print FINAL-INI-OK",
+             "still LOCAL-only: the STOR happens at ride.4, after the uploads + guard"],
             ["python3 %s <run-dir>/ploader.ini.new   # FINAL-INI-OK" % FINAL_INI["final_verifier"]],
             final_ini_verify_local_fn("ridealongs"))
 
-    fl.step("ride.4", "§3.3 STOR edited ploader.ini + fresh RETR byte-exact (skipped when the "
-                      "decision was a no-op; already done + verified on the bootstrap branch)",
-            "ini",
-            ["readback must be byte-identical to the verified local file (plain file law)",
-             "no-op edit -> nothing is stored (idempotent re-run)"],
-            ["curl -sS -T <run-dir>/ploader.ini.new %s%s" % (FTP, FINAL_INI["ini"]),
-             "curl -sS %s%s -o <run-dir>/ploader.ini.readback" % (FTP, FINAL_INI["ini"])],
-            final_ini_stor_fn("ridealongs"))
-
     fl.step("ride.5", "SC-0 b — freshness baselines: UE4SS.log stat; observer log ABSENT; igf "
                       "jsonl + ALL THREE probe jsonl sinks baselined IF PRESENT (never delete "
                       "owned evidence; fresh growth proves this launch's verdicts)",
@@ -3111,6 +3287,35 @@
              "curl -sS %s%s -o <run-dir>/cc1.readback.prx" % (FTP, CC1["target_prx"]),
              "python3 - §3 snippet vs determinism/run1/bare-sdk/modbridge_carrier.prx"],
             prx_law_upload_fn(CC1, "cc1"))
+
+    fl.step("ride.9c", "ORDER-LAW PRE-PUBLISH GUARD — every =2 entry of ploader.ini.new must have "
+                       "a VERIFIED-PRESENT file on the console BEFORE the ini publish: the five "
+                       "staged PRXs via THIS run's SHT-strip law verdicts (ride.6..ride.9b); the "
+                       "historical pre-existing enabled lines (composition, aligned_entry, "
+                       "path_test) via fresh RETR presence proofs (fetchable + non-empty + sha "
+                       "recorded). ANY gap = loud refusal, ini NOT published, old ini stays "
+                       "(launch9 incident class: the console must never carry enabled entries "
+                       "whose files are missing)", "ini",
+            ["guard REFUSES the publish on ANY missing/0-byte enabled entry (order law §2)",
+             "no-op decision -> nothing will be published -> guard not required (SKIP)",
+             "same 'ini' dependency group as the publish step: a guard refusal also skips it "
+             "under --continue-on-failure"],
+            ["curl -sS %s/data/plugins/modbridge_supplier_composition.prx -o <run-dir>/guard-present-composition.prx" % FTP,
+             "curl -sS %s/data/plugins/modbridge_runtime_aligned_entry.prx -o <run-dir>/guard-present-aligned_entry.prx" % FTP,
+             "curl -sS %s/data/plugins/modbridge_path_test.prx -o <run-dir>/guard-present-path_test.prx" % FTP],
+            ini_publish_guard_fn("ridealongs"))
+
+    fl.step("ride.4", "§3.3 PUBLISH (STAGING-ORDER LAW: LAST) — STOR edited ploader.ini + fresh "
+                      "RETR byte-exact, ONLY after ride.9c proved every enabled entry's file "
+                      "present; bootstrap branch = the ONE sanctioned whole-file STOR of "
+                      "ploader.ini.canonical, verdict INI-BOOTSTRAPPED (never a silent rewrite, "
+                      "never a second attempt); skipped when the decision was a no-op", "ini",
+            ["readback must be byte-identical to the verified local file (plain file law)",
+             "no-op edit -> nothing is stored (idempotent re-run)",
+             "on any publish failure the console KEEPS its previous ini — no partial publish"],
+            ["curl -sS -T <run-dir>/ploader.ini.new %s%s" % (FTP, FINAL_INI["ini"]),
+             "curl -sS %s%s -o <run-dir>/ploader.ini.readback" % (FTP, FINAL_INI["ini"])],
+            final_ini_stor_fn("ridealongs"))
 
     fl.step("ride.10", "SC-6/SC-7 HANDOFF — arm ploader + USER launches Townfall (single launch "
                        "window; the definitive 9-entry ini: 8 registered plugins ride incl. the "
@@ -3183,6 +3388,19 @@
              "python3 - §3 snippet vs build-1/bare-sdk/modbridge_runtime_aligned_observer.prx"],
             prx_law_upload_fn(OBS, "observer"))
 
+    fl.step("obs.3b", "ORDER-LAW PRE-PUBLISH GUARD — every =2 entry of ploader.ini.new must have a "
+                      "verified-present file on the console BEFORE the ini STOR (obs.4): the "
+                      "observer PRX via obs.3's SHT-strip law verdict; the historical pre-existing "
+                      "enabled lines (and a qdrv1 line staged by an earlier flow) via fresh RETR "
+                      "presence proofs. ANY gap = loud refusal, ini NOT published (launch9 fix)",
+            "ini",
+            ["guard REFUSES the publish on ANY missing/0-byte enabled entry (order law §2)",
+             "no-op decision -> nothing will be published -> guard not required (SKIP)",
+             "same 'ini' dependency group as the STOR step: a refusal also skips it under "
+             "--continue-on-failure"],
+            ["curl -sS ftp://<console>/data/plugins/<enabled-entry> -o <run-dir>/guard-present-<enabled-entry>.prx   # presence proof per non-staged enabled entry"],
+            ini_publish_guard_fn("observer"))
+
     fl.step("obs.4", "SC-5 — STOR edited ploader.ini + fresh readback byte-exact (skipped when "
                      "the edit was a no-op)", "ini",
             ["readback must be byte-identical to the local append-only edit (plain file law)",
@@ -3295,6 +3513,189 @@
              "never relaunch an identical rejected binary"],
             ["curl -sS %s%s   # eyeball: both lines = 0, everything else unchanged" % (FTP, COMBINED_INI["ini"])],
             None)
+    return fl
+
+
+# --- restore-ini (staging-order rollback helper, launch9 incident) ---------------
+
+def build_restore_ini():
+    """ORDER-LAW rollback helper (worker-staging-order, launch9 incident): restore
+    /data/plugins/ploader.ini BYTE-EXACT from a locally saved pre-publish copy.
+    Default source: the controller's saved 9-entry canonical
+    (outputs/2026-10-04-zcode-controller/launch9-evidence/
+    ploader-ini-9entry-restore.txt); override with --ini-src. Sequence:
+      rs.0 verify the LOCAL source identity + shape (never restore from an
+           unparseable file);
+      rs.1 freeze-classify the console pre-state (already == source -> honest
+           no-op; the recorded post-incident 3-enabled minimal (sha 32d0c0d9…)
+           -> sanctioned; anything else -> FAIL, never overwrite divergent/
+           unowned content blind — --allow-divergent-pre records an explicit
+           operator override) + console .bak FIRST (rollback artifact);
+      rs.2 the SAME pre-publish guard every ini publish obeys — an ini whose
+           enabled plugin files are missing is REFUSED (so restoring the
+           9-entry canonical over the post-incident state requires the files
+           to be re-staged first: run stage-ridealongs, which uploads + law-
+           verifies all five PRXs BEFORE its own publish);
+      rs.3 STOR + fresh RETR readback, byte-exact."""
+    src = RESTORE_SRC_PATH
+    fl = Flow("restore-ini")
+    fl.notes = ("ORDER-LAW rollback helper: restore ploader.ini byte-exact from the locally saved "
+                "copy %s. Game CLOSED, ploader NOT armed. Refuses divergent console pre-states "
+                "(unless --allow-divergent-pre) and refuses to publish an ini whose enabled plugin "
+                "files are missing (launch9 incident class)." % src)
+
+    def src_preflight_fn(ctx):
+        if not os.path.exists(src) or os.path.getsize(src) == 0:
+            return "FAIL", "restore source %s missing/empty — nothing to restore from" % src
+        sha, n = local_sha(src)
+        text = open(src, errors="replace").read()
+        entries, header_count, fatal = _parse_combined_ini(text)
+        enabled, ep = ini_enabled_paths(text)
+        problems = []
+        if fatal:
+            problems.append(fatal)
+        if header_count != 1:
+            problems.append("expected exactly one %s header, found %d" % (FINAL_INI["header"], header_count))
+        if ep:
+            problems.append(ep[0])
+        is_canon = sha == FINAL_INI["canonical_sha256"] and n == FINAL_INI["canonical_bytes"]
+        ctx.save("restore-source-identity.json", json.dumps(
+            {"src": src, "sha256": sha, "bytes": n, "enabled_entries": enabled,
+             "byte_exact_canonical": is_canon, "problems": problems}, indent=2))
+        if problems:
+            return "FAIL", "restore source fails the ini shape law: %s" % "; ".join(problems)
+        return "PASS", "restore source verified: %d bytes, sha %s…, %d enabled entries%s" % (
+            n, (sha or "?")[:12], len(enabled),
+            " (byte-exact canonical)" if is_canon else "")
+
+    fl.step("rs.0", "RESTORE preflight — local source identity + ini shape law (default: the "
+                    "controller's saved 9-entry canonical ploader-ini-9entry-restore.txt, sha "
+                    "fd9c7393… / 440 B)", "preflight",
+            ["source exists, non-empty, parses as ONE-header ploader.ini, enabled entries extracted",
+             "a diverged/unparseable source is never restored from"],
+            ["shasum -a 256 %s" % src,
+             "stat -f %%z %s" % src,
+             "python3 - _parse_combined_ini(source)  # shape + enabled-entry census"],
+            src_preflight_fn)
+
+    def freeze_fn(ctx):
+        p = os.path.join(ctx.run_dir, "ploader.ini.pre")
+        rc, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + RESTORE_INI["ini"], "-o", p],
+                           "RETR current ploader.ini (freeze)")
+        if not os.path.exists(p) or os.path.getsize(p) == 0:
+            return "FAIL", ("no ploader.ini pre-image retrievable (curl rc=%s) — never restore over "
+                            "an unreadable console state; reconcile first" % rc)
+        pre_sha, pre_n = local_sha(p)
+        ctx.save("ploader.ini.pre.sha256", "%s  %d\n" % (pre_sha, pre_n))
+        src_sha, _src_n = local_sha(src)
+        tag = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
+        rc2, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", p,
+                                  FTP + RESTORE_INI["ini"] + "." + tag + ".bak"],
+                            "console backup ploader.ini.$TAG.bak (rollback artifact FIRST)")
+        if rc2 != 0:
+            return "FAIL", "could not write console .bak backup — STOP (never overwrite without a rollback artifact)"
+        if pre_sha == src_sha:
+            ctx.save("ploader-edit-decision.json", json.dumps(
+                {"flow": "restore-ini", "status": "noop",
+                 "why": "console ini already byte-identical to the restore source"}, indent=2))
+            return "PASS", "freeze: console ini is ALREADY byte-identical to the restore source — honest no-op"
+        if pre_sha in RESTORE_INI["classified_pre_states"].values():
+            state = [k for k, v in RESTORE_INI["classified_pre_states"].items() if v == pre_sha][0]
+            ctx.save("ploader-edit-decision.json", json.dumps(
+                {"flow": "restore-ini", "status": "append", "pre_state": state}, indent=2))
+            return "PASS", "freeze: console pre-state classified %r (recorded sha) — restore sanctioned" % state
+        if RESTORE_ALLOW_DIVERGENT_PRE:
+            ctx.save("ploader-edit-decision.json", json.dumps(
+                {"flow": "restore-ini", "status": "append",
+                 "pre_state": "DIVERGENT-PRE-ACCEPTED (explicit --allow-divergent-pre override)",
+                 "pre_sha256": pre_sha, "pre_bytes": pre_n}, indent=2))
+            return "PASS", ("freeze: DIVERGENT-PRE-ACCEPTED (operator override, recorded): console "
+                            "ini %s…/%s B matches neither the source nor a classified pre-state" % (
+                                (pre_sha or "?")[:12], pre_n))
+        return "FAIL", ("freeze REFUSED: console ini %s…/%s B matches neither the restore source nor "
+                        "a classified pre-state (%s) — divergent/unowned content is never overwritten "
+                        "blind; reconcile first or pass --allow-divergent-pre explicitly"
+                        % ((pre_sha or "?")[:12], pre_n,
+                           ", ".join(sorted(RESTORE_INI["classified_pre_states"]))))
+
+    fl.step("rs.1", "RESTORE freeze — RETR the current console ini, classify the pre-state "
+                    "(already-restored -> no-op; recorded post-incident 3-enabled minimal sha "
+                    "32d0c0d9… -> sanctioned; anything else -> REFUSED, never overwrite divergent "
+                    "content blind) + console .bak backup FIRST", "ini",
+            ["never overwrite divergent/unowned content (AGENTS.md); --allow-divergent-pre is a "
+             "loud, recorded operator override",
+             "console .bak with a distinct TAG before any STOR",
+             "noop classification stores nothing later (idempotent re-run)"],
+            ["curl -sS %s%s -o <run-dir>/ploader.ini.pre" % (FTP, RESTORE_INI["ini"]),
+             "shasum -a 256 <run-dir>/ploader.ini.pre",
+             "curl -sS -T <run-dir>/ploader.ini.pre %s%s.$TAG.bak" % (FTP, RESTORE_INI["ini"])],
+            freeze_fn)
+
+    def guard_prepare_fn(ctx):
+        # materialize the restore source as the guard's "new ini" + reuse the shared
+        # pre-publish guard (same enabled-entry law as every staging publish)
+        try:
+            decision = json.load(open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")))
+        except Exception:
+            decision = {}
+        if decision.get("status") == "noop":
+            return "PASS", "SKIP-GUARD-NOT-NEEDED — freeze classified the console as already restored"
+        dst = os.path.join(ctx.run_dir, "ploader.ini.restore")
+        with open(dst, "wb") as f:
+            f.write(open(src, "rb").read())
+        return ini_publish_guard_fn("restore", new_ini_name="ploader.ini.restore")(ctx)
+
+    fl.step("rs.2", "RESTORE pre-publish guard — the SAME enabled-entry law every ini publish "
+                    "obeys: an ini whose =2 entries have NO verified-present file on the console "
+                    "is REFUSED (launch9 class); restoring the 9-entry canonical over the "
+                    "post-incident state requires the missing PRXs to be re-staged first "
+                    "(stage-ridealongs uploads+verifies them BEFORE its publish)", "ini",
+            ["guard: every enabled entry verified present (this-run law verdicts or RETR presence)",
+             "0-byte/unfetchable enabled file = loud refusal, console ini untouched",
+             "no-op classification skips the guard (nothing will be published)"],
+            ["python3 console_batch.py ini_publish_guard_fn  # vs ploader.ini.restore"],
+            guard_prepare_fn)
+
+    def restore_stor_fn(ctx):
+        decision = {}
+        try:
+            decision = json.load(open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")))
+        except Exception:
+            pass
+        if decision.get("status") == "noop":
+            return "PASS", "console ini already byte-identical to the restore source — no STOR"
+        src_sha, src_n = local_sha(src)
+        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", src, FTP + RESTORE_INI["ini"]],
+                           "RESTORE STOR ploader.ini")
+        if rc != 0:
+            return "FAIL", "restore STOR failed rc=%d — console keeps its previous ini; reconcile" % rc
+        p = os.path.join(ctx.run_dir, "ploader.ini.readback")
+        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + RESTORE_INI["ini"], "-o", p],
+                            "restore readback")
+        if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
+            return "FAIL", "restore readback failed rc=%d — do not launch on an unverified ini" % rc2
+        rb_sha, rb_n = local_sha(p)
+        ok = (rb_sha, rb_n) == (src_sha, src_n)
+        ctx.save("restore-verdict.json", json.dumps(
+            {"flow": "restore-ini", "src": src, "src_sha256": src_sha, "src_bytes": src_n,
+             "readback_sha256": rb_sha, "readback_bytes": rb_n, "byte_exact": ok,
+             "pre_state": decision.get("pre_state", decision.get("status"))}, indent=2))
+        if not ok:
+            return "FAIL", ("restore readback %s…/%s != source %s…/%s — the restore did NOT land "
+                            "byte-exact; STOP and reconcile" % ((rb_sha or "?")[:12], rb_n,
+                                                                (src_sha or "?")[:12], src_n))
+        return "PASS", "restored byte-exact from the saved pre-publish copy (%d bytes, sha %s…) + readback verified" % (
+            src_n, (src_sha or "?")[:12])
+
+    fl.step("rs.3", "RESTORE publish — STOR the saved copy + fresh RETR readback, BYTE-EXACT "
+                    "(skipped on the already-restored no-op classification)", "ini",
+            ["readback sha256+bytes must equal the restore source (byte-exact law)",
+             "verdict receipt records source + readback identities",
+             "any STOR/readback failure leaves the console as found (never a blind retry)"],
+            ["curl -sS -T %s %s%s" % (src, FTP, RESTORE_INI["ini"]),
+             "curl -sS %s%s -o <run-dir>/ploader.ini.readback" % (FTP, RESTORE_INI["ini"]),
+             "shasum -a 256 <run-dir>/ploader.ini.readback"],
+            restore_stor_fn)
     return fl
 
 
@@ -4123,7 +4524,7 @@
 FLOWS = {"dll8": build_dll8, "dll7": build_dll7, "e2e": build_e2e,
          "stage-ridealongs": build_stage_ridealongs, "stage-observer": build_stage_observer,
          "stage-qdrv1": build_stage_qdrv1, "stage-settings": build_stage_settings,
-         "rollback-ridealongs": build_rollback_ridealongs,
+         "rollback-ridealongs": build_rollback_ridealongs, "restore-ini": build_restore_ini,
          "game-evidence": build_game_evidence, "all": build_all}
 
 
@@ -4243,6 +4644,64 @@
                    "modbridge-carrier-facts.jsonl"]:
         check("stage-ridealongs anchor %r" % anchor[:40], anchor in rd)
 
+    print("== selftest A3: STAGING-ORDER law — every ini publish comes LAST, behind the guard ==")
+    def steps_of(name):
+        flx = FLOWS[name]()
+        return dict((st["id"], st) for st in flx.steps), [st["id"] for st in flx.steps]
+    steps_r, ids_r = steps_of("stage-ridealongs")
+    idx = lambda sid: ids_r.index(sid)
+    check("order law: decision (ride.2b) + local verify (ride.3) + baselines (ride.5) precede ALL uploads",
+          idx("ride.2b") < idx("ride.3") < idx("ride.5") < idx("ride.6"), str(ids_r))
+    check("order law: the five PRX uploads run in ride.6..ride.9b BEFORE guard (ride.9c) and publish (ride.4)",
+          idx("ride.6") < idx("ride.7") < idx("ride.8") < idx("ride.9") < idx("ride.9b")
+          < idx("ride.9c") < idx("ride.4") < idx("ride.10"), str(ids_r))
+    check("order law: ride.2b stores NOTHING to the console (publish DEFERRED; the .bak backup lives in ride.2)",
+          not any("curl -sS -T" in c and "ploader.ini.new" in c for c in steps_r["ride.2b"]["cmds"]),
+          str(steps_r["ride.2b"]["cmds"]))
+    check("order law: ride.4 is the ONLY step carrying the ini publish STOR",
+          any("ploader.ini.new" in c and "-T" in c for c in steps_r["ride.4"]["cmds"])
+          and sum(1 for st in steps_r.values()
+                  for c in st["cmds"] if "curl -sS -T" in c and "ploader.ini.new" in c) == 1,
+          str([c for st in steps_r.values() for c in st["cmds"] if "ploader.ini.new" in c]))
+    check("order law: guard + publish + handoff chain — the guard refusal halts the publish",
+          steps_r["ride.9c"]["group"] == steps_r["ride.4"]["group"] == "ini")
+    check("order law: ride.2b/ride.4 plan text states the deferred publish (launch9 fix)",
+          "DEFERRED" in "\n".join(steps_r["ride.2b"]["gates"] + steps_r["ride.2b"]["title"].split())
+          and "LAST" in "\n".join(steps_r["ride.4"]["gates"] + steps_r["ride.4"]["title"].split()))
+    steps_o, ids_o = steps_of("stage-observer")
+    check("order law: stage-observer uploads its PRX (obs.3) before guard (obs.3b) before STOR (obs.4)",
+          ids_o.index("obs.3") < ids_o.index("obs.3b") < ids_o.index("obs.4"), str(ids_o))
+    steps_q, ids_q = steps_of("stage-qdrv1")
+    check("order law: stage-qdrv1 uploads its PRX (qdrv1.3) before guard (qdrv1.3b) before STOR (qdrv1.4)",
+          ids_q.index("qdrv1.3") < ids_q.index("qdrv1.3b") < ids_q.index("qdrv1.4"), str(ids_q))
+    steps_b, ids_b = steps_of("rollback-ridealongs")
+    check("order law: rollback-ridealongs only DISABLES lines (flip to = 0) — no enable-publish to guard",
+          any("= 0" in g for g in steps_b["rb.1"]["gates"]))
+    steps_v, ids_v = steps_of("restore-ini")
+    check("order law: restore-ini runs preflight -> freeze -> guard -> byte-exact publish",
+          ids_v == ["rs.0", "rs.1", "rs.2", "rs.3"], str(ids_v))
+    ctx = Ctx(None, dry=True); FLOWS["restore-ini"]().run(ctx); rsv = "\n".join(ctx.events)
+    for anchor in ["ploader-ini-9entry-restore.txt", "fd9c7393", "32d0c0d9", "byte-exact",
+                   ".bak", "pre-publish guard", "launch9"]:
+        check("restore-ini anchor %r" % anchor[:40], anchor in rsv)
+
+    print("== selftest A4: order-law guard helpers (pure) ==")
+    CANON_TXT = final_canonical_text()
+    en, probs = ini_enabled_paths(CANON_TXT)
+    check("guard: canonical enables exactly the 8 pinned entries (igf = 0 excluded)",
+          probs == [] and sorted(en) == sorted(p for p, m in FINAL_INI["expected_existing"].items() if m != 0),
+          str((probs, en)))
+    en2, _p2 = ini_enabled_paths("[modules]\n1=%s\n2=%s\n" % (QDRV1["composition_line"], QDRV1["module_line"]))
+    check("guard: legacy numbered §1 shape enables both listed modules",
+          en2 == [QDRV1["composition_line"], QDRV1["module_line"]], str(en2))
+    en3, probs3 = ini_enabled_paths("junk without = sign\n")
+    check("guard: unparseable ini yields problems (never a silently empty enable set)",
+          probs3 != [] and en3 == [], str((en3, probs3)))
+    en4, _p4 = ini_enabled_paths(CANON_TXT.replace("/data/plugins/modbridge_ingame_facts.prx = 0",
+                                                  "/data/plugins/modbridge_ingame_facts.prx = 2"))
+    check("guard: a mode-flipped line joins the enabled set (mode != 0 loads)",
+          sorted(en4) == sorted(en + ["/data/plugins/modbridge_ingame_facts.prx"]))
+
     ctx = Ctx(None, dry=True); FLOWS["stage-settings"]().run(ctx); st = "\n".join(ctx.events)
     for anchor in ["d8faa4ec", "4d98ecd7", "1448", "447", "lockstep", "FREEZE",
                    "UE4SS-settings.ini", "loadlist.txt", "MFA_E_SIZE", "9570ead3",
@@ -5005,11 +5464,10 @@
                             "(PREFERRED, default); 7 = ALLFAM7_DLL_TEST fallback attribution path. "
                             "EXACTLY ONE DLL stage runs per session either way.")
         return p
-
     p = sub.add_parser("plan", help="dry-run: print every step, gate and exact command; NO execution")
     p.add_argument("--only", default="all",
                    help="comma list: dll8,dll7,e2e,stage-ridealongs,stage-observer,stage-qdrv1,"
-                        "stage-settings,rollback-ridealongs,game-evidence,all")
+                        "stage-settings,rollback-ridealongs,restore-ini,game-evidence,all")
     p.add_argument("--dll", choices=("7", "8"), default=None,
                    help="DLL stage 'plan --only all' previews: 8 (preferred, default) or 7 (fallback)")
     add_flow("dll8", "8-family DLL payload-only stage (game closed) — PREFERRED single DLL run")
@@ -5019,7 +5477,9 @@
                                  "observer + cc1-carrier PRXs (SHT-strip readback law) + the FINAL 9-entry "
                                  "ploader.ini (bootstrap whole-file STOR only when the historical "
                                  "line moves require it; ensure ops otherwise; byte-exact canonical, "
-                                 "8 registered) (game closed, BEFORE the session) — supersedes "
+                                 "8 registered). ORDER LAW: all enabled plugin files are uploaded + "
+                                 "law-verified FIRST; the enabling ini is published LAST behind the "
+                                 "pre-publish guard (game closed, BEFORE the session) — supersedes "
                                  "stage-qdrv1/stage-observer inside `all`")
     add_flow("stage-observer", "observer ridealong ONLY (legacy partial path): PRX + SHT-strip readback "
                                "law + idempotent ini append of ONLY the observer line (game closed)")
@@ -5030,6 +5490,19 @@
                                "+ loadlist 447 B 4d98ecd7…), byte-exact readback BOTH (game closed)")
     add_flow("rollback-ridealongs", "STAGE-PLAN-COMBINED §7 rollback: flip BOTH ridealong lines to "
                                     "= 0 + byte-verified readback (game closed, ploader not armed)")
+    p_restore = add_flow("restore-ini", "ORDER-LAW rollback helper: restore ploader.ini BYTE-EXACT "
+                                        "from a locally saved pre-publish copy (default: the "
+                                        "controller's saved 9-entry canonical), behind the same "
+                                        "pre-publish guard + freeze-classified pre-state (game "
+                                        "closed, ploader not armed)")
+    p_restore.add_argument("--ini-src", default=RESTORE_INI["src_default"],
+                           help="local path of the ini copy to restore (default: "
+                                "outputs/2026-10-04-zcode-controller/launch9-evidence/"
+                                "ploader-ini-9entry-restore.txt, sha fd9c7393… / 440 B)")
+    p_restore.add_argument("--allow-divergent-pre", action="store_true",
+                           help="explicit operator override: allow restoring over a console ini "
+                                "that matches NEITHER the source NOR a classified pre-state "
+                                "(recorded loudly as DIVERGENT-PRE-ACCEPTED; default is REFUSE)")
     add_flow("game-evidence", "post-launch evidence + CP0-CP7 + probe verdict sinks incl. the cc1 "
                               "census (interactive user handoffs)")
     add_flow("all", "full recommended session: dll8 -> e2e -> stage-ridealongs -> stage-settings -> "
@@ -5044,6 +5517,10 @@
     if args.cmd == "selftest":
         return selftest()
     global SELECTED_DLL
+    global RESTORE_SRC_PATH, RESTORE_ALLOW_DIVERGENT_PRE
+    if getattr(args, "ini_src", None):
+        RESTORE_SRC_PATH = os.path.abspath(args.ini_src)
+        RESTORE_ALLOW_DIVERGENT_PRE = bool(getattr(args, "allow_divergent_pre", False))
     sel = getattr(args, "dll", None)
     if sel:
         SELECTED_DLL = "dll" + sel

===============================================================
--- tests/fake_console_drive.py (fake-console harness extension) ---
--- /tmp/staging-order-snap/fake_console_drive.py.pre-staging-order.py	2026-10-05 07:19:28
+++ worker-console-batch-runner/tests/fake_console_drive.py	2026-10-05 07:36:03
@@ -243,6 +243,9 @@
         self.e2e_cursor = 0          # lines of E2E35 visible so far
         self.recovery_dispatches = 0
         self.stor = {}
+        self.stor_sequence = []      # ordered ("bak"|"ini"|"prx", url) log: the
+                                     # staging-order law asserts the ini publish
+                                     # comes strictly AFTER every PRX STOR
         self.reset_deletes = []
         self.serve_8fam = False      # results-file mode: False = 7fam transcript, True = 8fam
         self.sht_law = False         # PRX readback channel mode: False = legacy full-identity
@@ -404,8 +407,10 @@
         # --- ploader.ini / qdrv1 ---
         if "ploader.ini" in joined:
             if data_target and data_target.endswith(".pre"):
+                self.stor_sequence.append(("bak", joined.split()[-1]))
                 return 0, ""  # console .bak backup STOR
-            if data_target:  # STOR of edited ini / PRX
+            if data_target:  # STOR of edited ini / restore copy
+                self.stor_sequence.append(("ini", joined.split()[-1]))
                 self.stor[joined.split()[-1]] = open(data_target).read()
                 return 0, ""
             if out_target:
@@ -416,11 +421,20 @@
                     return 0, ""
                 return 8, ""  # no prior ini
             return 0, self.stor.get(cb.QDRV1["ini"], "")
+        if "modbridge_supplier_composition.prx" in joined or \
+           "modbridge_runtime_aligned_entry.prx" in joined or \
+           "modbridge_path_test.prx" in joined:
+            # historical PRE-EXISTING console plugins (never uploaded by the tool):
+            # the staging-order pre-publish guard proves their presence via RETR
+            name = ("supplier_composition" if "supplier_composition" in joined
+                    else "aligned_entry" if "aligned_entry" in joined else "path_test")
+            return emit("PRE-EXISTING-CONSOLE-FSELF %s (fake bytes)\n" % name)
         if "modbridge_runtime_aligned_observer.prx" in joined:
             # observer ridealong PRX (STAGE-PLAN-COMBINED): readback follows the
             # SHT-strip channel law whenever the sim is in law mode
             if data_target:
                 self.prx_stors.append(joined.split()[-1])
+                self.stor_sequence.append(("prx", joined.split()[-1]))
                 return 0, ""
             content = (self.prx_readback_corruptor(cb.OBS) if self.prx_readback_corruptor
                        else self.law_readback(cb.OBS) if self.sht_law
@@ -433,6 +447,7 @@
         if "modbridge_qh_driver.prx" in joined:
             if data_target:
                 self.prx_stors.append(joined.split()[-1])
+                self.stor_sequence.append(("prx", joined.split()[-1]))
                 return 0, ""
             content = (self.prx_readback_corruptor(cb.QDRV1) if self.prx_readback_corruptor
                        else self.law_readback(cb.QDRV1) if self.sht_law
@@ -446,6 +461,7 @@
             # xwp launch probe (definitive layout entry 1): SHT-strip law channel
             if data_target:
                 self.prx_stors.append(joined.split()[-1])
+                self.stor_sequence.append(("prx", joined.split()[-1]))
                 return 0, ""
             content = (self.prx_readback_corruptor(cb.XWP_ARMED) if self.prx_readback_corruptor
                        else self.law_readback(cb.XWP_ARMED) if self.sht_law
@@ -459,6 +475,7 @@
             # fc1 launch probe (definitive layout entry 2): SHT-strip law channel
             if data_target:
                 self.prx_stors.append(joined.split()[-1])
+                self.stor_sequence.append(("prx", joined.split()[-1]))
                 return 0, ""
             content = (self.prx_readback_corruptor(cb.FC1POOL) if self.prx_readback_corruptor
                        else self.law_readback(cb.FC1POOL) if self.sht_law
@@ -472,6 +489,7 @@
             # cc1 carrier probe (NINTH-SLOT layout entry 9): SHT-strip law channel
             if data_target:
                 self.prx_stors.append(joined.split()[-1])
+                self.stor_sequence.append(("prx", joined.split()[-1]))
                 return 0, ""
             content = (self.prx_readback_corruptor(cb.CC1) if self.prx_readback_corruptor
                        else self.law_readback(cb.CC1) if self.sht_law
@@ -829,7 +847,167 @@
 check("STOP scenario uploads NOTHING and stores NOTHING (bootstrap never fires on an "
       "anomalous pre-image)",
       fc.prx_stors == [] and stored_ini() == HISTORICAL_INI + "/data/plugins/modbridge_surprise.prx = 2\n")
+
+# ---------------------------------------------------------------------------
+# STAGING-ORDER LAW scenarios (worker-staging-order, launch9 incident fix):
+# upload + verify EVERY enabled plugin file FIRST; publish the enabling ini
+# LAST behind the pre-publish guard; restore-ini rolls the ini back byte-exact.
+# ---------------------------------------------------------------------------
+
+print("== fake-console drive: ORDER LAW (a) — happy path: verify-then-publish; the "
+      "enabling ini STOR is strictly LAST ==")
+seed_ini(HISTORICAL_INI)
+fc.prx_stors = []
+fc.stor_sequence = []
+xwp_fetch_n["n"] = 0
+fc1_fetch_n["n"] = 0
+cc1_fetch_n["n"] = 0
+ctx = cb.Ctx(run_dir=fresh("order-law-happy"), dry=False, assume_yes=True)
+res = cb.build_stage_ridealongs().run(ctx)
+bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
+check("order-law happy path all steps PASS (decision local -> 5x upload+law -> guard -> publish)",
+      not bad, str(bad))
+check("console sequence is EXACTLY: .bak backup -> five PRX law STORs -> ONE ini publish STOR",
+      [s[0] for s in fc.stor_sequence] == ["bak", "prx", "prx", "prx", "prx", "prx", "ini"],
+      str(fc.stor_sequence))
+check("published ini is byte-exact canonical (published LAST)", stored_ini() == CANON_INI,
+      repr(stored_ini())[:120])
+check("INI-BOOTSTRAPPED verdict written by the PUBLISH step (post-guard)",
+      "INI-BOOTSTRAPPED" in open(os.path.join(ctx.run_dir, "ini-bootstrap-verdict.txt")).read())
+guard = json.load(open(os.path.join(ctx.run_dir, "ridealongs-publish-guard.json")))
+check("guard receipt: 8 enabled entries, 0 missing (5 this-run law verdicts + 3 presence proofs)",
+      len(guard["enabled_entries"]) == 8 and guard["missing"] == []
+      and len(guard["law_verified"]) == 5 and len(guard["presence_proofs"]) == 8, str(guard)[:240])
+
+print("== fake-console drive: ORDER LAW (b) — xwp readback law FAIL mid-flow: ini NEVER "
+      "published, old ini intact (the launch9 incident, now impossible) ==")
+seed_ini(HISTORICAL_INI)
+fc.prx_stors = []
+fc.stor_sequence = []
+fc.prx_readback_corruptor = truncated_readback
+ctx = cb.Ctx(run_dir=fresh("order-law-midflow-fail"), dry=False, assume_yes=True)
+res = cb.build_stage_ridealongs().run(ctx)
+fc.prx_readback_corruptor = None
+statuses = dict(res)
+check("ride.6 FAILS on the divergent xwp readback (the launch9 halt point)",
+      statuses.get("ride.6") == "FAIL", statuses.get("ride.6"))
+check("guard + publish SKIPPED after the halt",
+      statuses.get("ride.9c") == "SKIPPED_HALT" and statuses.get("ride.4") == "SKIPPED_HALT",
+      str((statuses.get("ride.9c"), statuses.get("ride.4"))))
+check("NO ini publish happened at all", not any(s[0] == "ini" for s in fc.stor_sequence),
+      str(fc.stor_sequence))
+check("console keeps the OLD ini byte-identical (safe state: its enabled files are present)",
+      stored_ini() == HISTORICAL_INI, repr(stored_ini())[:120])
+check("exactly initial STOR + ONE re-STOR of xwp, then abort (no retries, no publish)",
+      len(fc.prx_stors) == 2, str(fc.prx_stors))
+
+print("== fake-console drive: ORDER LAW (c) — a =2 entry whose file is MISSING/0-byte: "
+      "publish REFUSED ==")
+seed_ini(HISTORICAL_INI)
+fc.prx_stors = []
+fc.stor_sequence = []
+_rc_orig = fc.handle_curl
+
+
+def missing_path_test(argv, *a, **k):
+    joined = " ".join(argv)
+    if "modbridge_path_test.prx" in joined and "-o" in argv and "-T" not in argv:
+        open(argv[argv.index("-o") + 1], "wb").close()  # 0-byte file: the launch9 missing-plugin signature
+        return (0, "")
+    return _rc_orig(argv, *a, **k)
+
+
+fc.handle_curl = missing_path_test
+ctx = cb.Ctx(run_dir=fresh("order-law-missing"), dry=False, assume_yes=True)
+res = cb.build_stage_ridealongs().run(ctx)
+fc.handle_curl = _rc_orig
+statuses = dict(res)
+check("all five PRXs uploaded + law-verified FIRST (uploads unaffected by the missing file)",
+      len(fc.prx_stors) == 5 and statuses.get("ride.9b") == "PASS",
+      str((len(fc.prx_stors), statuses.get("ride.9b"))))
+check("ride.9c guard REFUSES the publish", statuses.get("ride.9c") == "FAIL", statuses.get("ride.9c"))
+check("the refusal NAMES the missing enabled entry (path_test)",
+      "modbridge_path_test.prx" in "\n".join(ctx.events))
+check("publish step SKIPPED — no partial publish", statuses.get("ride.4") == "SKIPPED_HALT",
+      statuses.get("ride.4"))
+check("console ini untouched (old ini intact)", stored_ini() == HISTORICAL_INI
+      and not any(s[0] == "ini" for s in fc.stor_sequence), str(fc.stor_sequence))
 
+print("== fake-console drive: restore-ini (order-law rollback helper) ==")
+MIN3 = (cb.FINAL_INI["header"] + "\n"
+        + "%s = 2\n" % C_LINE + "%s = 2\n" % E_LINE + "%s = 2\n" % P_LINE)  # 3-enabled post-incident shape
+ODD = cb.FINAL_INI["header"] + "\n%s = 1\n" % C_LINE
+cb.RESTORE_SRC_PATH = os.path.join(fresh("restore-src"), "saved-9entry.ini")
+with open(cb.RESTORE_SRC_PATH, "w") as f:
+    f.write(CANON_INI)
+
+# (d-1) divergent console pre-state REFUSED by default (never overwrite blind)
+seed_ini(ODD)
+fc.stor_sequence = []
+cb.RESTORE_ALLOW_DIVERGENT_PRE = False
+ctx = cb.Ctx(run_dir=fresh("restore-divergent"), dry=False, assume_yes=True)
+res = cb.build_restore_ini().run(ctx)
+statuses = dict(res)
+check("rs.1 freeze REFUSES the divergent console pre-state", statuses.get("rs.1") == "FAIL",
+      statuses.get("rs.1"))
+check("nothing published after the refusal (no ini STOR)",
+      not any(s[0] == "ini" for s in fc.stor_sequence), str(fc.stor_sequence))
+
+# (d-2) BYTE-EXACT restore over the 3-enabled post-incident state (recorded operator
+#       override), with EVERY enabled plugin file present on the console
+seed_ini(MIN3)
+fc.stor_sequence = []
+cb.RESTORE_ALLOW_DIVERGENT_PRE = True
+ctx = cb.Ctx(run_dir=fresh("restore-ok"), dry=False, assume_yes=True)
+res = cb.build_restore_ini().run(ctx)
+bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
+check("restore all steps PASS (override accepted, guard passed, publish landed)", not bad, str(bad))
+check("console ini restored BYTE-EXACT to the saved pre-publish copy", stored_ini() == CANON_INI,
+      repr(stored_ini())[:120])
+check("restore sequence: .bak backup first, then exactly ONE ini STOR",
+      fc.stor_sequence[0][0] == "bak" and [s[0] for s in fc.stor_sequence].count("ini") == 1,
+      str(fc.stor_sequence))
+check("restore verdict records byte-exact source+readback identities",
+      json.load(open(os.path.join(ctx.run_dir, "restore-verdict.json")))["byte_exact"] is True)
+
+# (d-3) re-run on the restored console is an honest no-op
+fc.stor_sequence = []
+ctx = cb.Ctx(run_dir=fresh("restore-noop"), dry=False, assume_yes=True)
+res = cb.build_restore_ini().run(ctx)
+bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
+check("restore re-run all steps PASS (already byte-identical)", not bad, str(bad))
+check("restore re-run stores NOTHING", stored_ini() == CANON_INI
+      and not any(s[0] == "ini" for s in fc.stor_sequence), str(fc.stor_sequence))
+
+# (d-4) guard refusal: the canonical restore is REFUSED while an enabled file is
+#       missing (0-byte fc1pool) — restoring would re-create the launch9 state
+seed_ini(MIN3)
+fc.stor_sequence = []
+_rc_orig2 = fc.handle_curl
+
+
+def missing_fc1(argv, *a, **k):
+    joined = " ".join(argv)
+    if "modbridge_fc1pool.prx" in joined and "-o" in argv and "-T" not in argv:
+        open(argv[argv.index("-o") + 1], "wb").close()  # 0-byte fc1pool on the console
+        return (0, "")
+    return _rc_orig2(argv, *a, **k)
+
+
+fc.handle_curl = missing_fc1
+ctx = cb.Ctx(run_dir=fresh("restore-guard"), dry=False, assume_yes=True)
+res = cb.build_restore_ini().run(ctx)
+fc.handle_curl = _rc_orig2
+statuses = dict(res)
+check("rs.2 guard REFUSES the canonical restore while fc1pool is missing/0-byte",
+      statuses.get("rs.2") == "FAIL", statuses.get("rs.2"))
+check("restore publish SKIPPED — console keeps the safe 3-enabled minimal ini",
+      statuses.get("rs.3") == "SKIPPED_HALT" and stored_ini() == MIN3,
+      str((statuses.get("rs.3"), stored_ini()[:60])))
+
+cb.RESTORE_SRC_PATH = cb.RESTORE_INI["src_default"]
+cb.RESTORE_ALLOW_DIVERGENT_PRE = False
+
 print("== fake-console drive: stage-settings (NEW-SETTINGS §3 lockstep restage, v3 pair) ==")
 fc.settings_staged_v3 = False  # console currently holds the recorded v2 pair
 ctx = cb.Ctx(run_dir=fresh("settings-v3"), dry=False, assume_yes=True)

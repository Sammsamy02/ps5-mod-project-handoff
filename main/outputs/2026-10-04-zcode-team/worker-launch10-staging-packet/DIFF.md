# DIFF — worker-launch10-staging-packet (2026-10-04)

Pre-change snapshots:
console_batch.py PRE-change snapshot sha256 6940971eb3de879637e14824f9953445b36bb21496af6022e61096fa7432040c (the just-fixed delivery)
tests/fake_console_drive.py PRE-change snapshot sha256 bd309ccb84d679bb4bbba8434d8fddd6ba4c49611d07a3521866ecd9ec8b4281

Post-change identities:
console_batch.py POST-change sha256 b982f794201a0d4dbde113c39274aefca41242a4625ca6c240aac2183f639303
tests/fake_console_drive.py POST-change sha256 f2134f0a1b5011120719dc2286db93ae162d2571b627dd514ffeeeb2babbdcde

Summary of the change: NEW `stage-launch10` flow in console_batch.py (L10 constants, l10_readback_law, build_stage_launch10, _l10_restore_old_prx_once, CLI subparser --fself/--expect-sha, FLOWS registry, docstring) + selftest section A5; NEW launch-10 scenarios (a)-(d) + composition-PRX/ini-drift branches in tests/fake_console_drive.py. NO existing flow, law, or fixture was altered (the fake's composition branch is superseded only for L10-seeded state; its historical fallback text is preserved).

--- /tmp/l10-snap-console_batch.py
+++ worker-console-batch-runner/console_batch.py
--- /tmp/l10-snap-console_batch.py	2026-10-05 08:00:38
+++ outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py	2026-10-05 08:04:29
@@ -15,7 +15,12 @@
 worker-carrier-probe-build/STAGE-PLAN.md), and by
 worker-staging-order with the STAGING-ORDER LAW (launch9 incident fix:
 upload + verify every enabled plugin file FIRST, publish the enabling
-ploader.ini LAST, pre-publish guard, restore-ini rollback helper). This tool
+ploader.ini LAST, pre-publish guard, restore-ini rollback helper), and by
+worker-launch10-staging-packet with the LAUNCH-10 STAGING flow (ONE console
+file changes: the composition PRX is replaced by the rebuilt launch-10
+FSELF; ploader.ini is verified STILL byte-equal and NEVER written; on any
+failed verification the OLD PRX is restored from the saved pre-image).
+This tool
 executes, step by step
 with gates, the plans frozen by:
   * worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md (PREFERRED single DLL
@@ -74,6 +79,18 @@
                    runs the SAME pre-publish guard every ini publish obeys (refuses
                    an ini whose enabled plugin files are missing), then STOR +
                    byte-exact RETR readback
+  stage-launch10   LAUNCH-10 STAGING packet (game CLOSED): replace /data/plugins/
+                   modbridge_supplier_composition.prx with the REBUILT launch-10
+                   composition FSELF (--fself + --expect-sha; placeholders until the
+                   build lands). Pre-image + console .bak FIRST, the FSELF is stored
+                   EXACTLY ONCE to its final name, the readback is verified by the
+                   content law against the LOCAL file (full identity, or ONLY the
+                   known ftpsrv SHT-strip tail-artifact class), ploader.ini is
+                   verified STILL byte-equal to the pinned 2-plugin config
+                   (sha 1216aab6…) — the flow NEVER writes the ini — then fresh
+                   pre-launch baselines + a final GO/NO-GO summary. On ANY failed
+                   verification the OLD PRX is restored byte-exact from the saved
+                   pre-image (ride.9c guard law, launch-10 form)
   game-evidence    post-launch evidence: post_launch_check + rung_check + CP0-CP7 checks
                    + the probe verdict sinks (xwp jsonl + fc1pool jsonl + cc1 census jsonl)
   all              recommended session order: dll8 -> e2e -> stage-ridealongs -> 
@@ -94,6 +111,12 @@
     proofs) and publishes the ini LAST; the pre-publish guard loudly refuses any
     publish that would enable a missing/0-byte plugin file. An interrupted
     staging therefore always leaves the old (safe) ini on the console.
+  * LAUNCH-10 GUARD LAW (ride.9c, launch-10 form): the stage-launch10 flow
+    writes exactly ONE console path (the composition PRX, after pre-image +
+    console .bak). Every verification failure restores the OLD PRX byte-exact
+    from the saved pre-image and refuses; ploader.ini is only ever READ
+    (frozen + re-checked byte-equal to the pinned 2-plugin config), never
+    written by this flow.
   * the tool never re-sends, never widens a gate, never writes outside its run dir
     and the console paths each plan explicitly owns.
 
@@ -441,8 +464,39 @@
     "C2": "/data/modbridge-ingame-facts.jsonl",
     "tick_line": "GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)",
     "mod_banner": "[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.",
+}
+
+# LAUNCH-10 STAGING packet (worker-launch10-staging-packet, 2026-10-04, per the
+# controller's staging facts). ONE file changes on the console:
+# /data/plugins/modbridge_supplier_composition.prx <- the REBUILT launch-10
+# composition FSELF. ploader.ini STAYS the recorded 2-plugin config (composition +
+# path_test enabled, sha 1216aab6…) — the flow verifies it STILL byte-equal
+# (frozen before, re-checked after) and NEVER writes it. Ride.9c guard law in its
+# launch-10 form: on ANY failed verification the console is returned to the OLD
+# PRX (byte-exact restore from the saved pre-image; the console .bak remains as
+# the operator's second rollback artifact) and the flow refuses.
+# The FSELF identity is a PLACEHOLDER until the build lands: <L10_FSELF_LOCAL_PATH>
+# / <L10_FSELF_SHA256> are filled via --fself / --expect-sha; the flow REFUSES to
+# stage while either is unfilled (no unidentified build is ever staged).
+L10 = {
+    "target_prx": "/data/plugins/modbridge_supplier_composition.prx",
+    "ini": "/data/plugins/ploader.ini",
+    "ini_sha256": "1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0",
+    "ue4ss_log": GAME["L"],
+    "runtime_status": GAME["S"],
+    "entry_log": GAME["D1"],
+    "diag": "/data/modbridge-supplier-diag.jsonl",
 }
 
+# Runtime overrides for the stage-launch10 flow (set by main() from --fself /
+# --expect-sha; empty defaults keep plan/selftest deterministic — the flow
+# refuses placeholders). L10_INI_EXPECTED_SHA stays pinned to the controller's
+# recorded 2-plugin config sha; the fake-drive suite may point it at a fixture
+# ini of the same shape to exercise the drift refusal.
+L10_FSELF_LOCAL_PATH = ""   # <L10_FSELF_LOCAL_PATH>
+L10_FSELF_SHA256 = ""       # <L10_FSELF_SHA256>
+L10_INI_EXPECTED_SHA = L10["ini_sha256"]
+
 # --------------------------------------------------------------------------
 # Verdict helpers (pure functions — fixture-tested by `selftest`)
 # --------------------------------------------------------------------------
@@ -3688,7 +3742,8 @@
             src_n, (src_sha or "?")[:12])
 
     fl.step("rs.3", "RESTORE publish — STOR the saved copy + fresh RETR readback, BYTE-EXACT "
-                    "(skipped on the already-restored no-op classification)", "ini",
+                    "(skipped on the already-restored no-op classification)",
+            "ini",
             ["readback sha256+bytes must equal the restore source (byte-exact law)",
              "verdict receipt records source + readback identities",
              "any STOR/readback failure leaves the console as found (never a blind retry)"],
@@ -3696,7 +3751,398 @@
              "curl -sS %s%s -o <run-dir>/ploader.ini.readback" % (FTP, RESTORE_INI["ini"]),
              "shasum -a 256 <run-dir>/ploader.ini.readback"],
             restore_stor_fn)
+    return fl
+
+
+# --- stage-launch10 (LAUNCH-10 STAGING packet, worker-launch10-staging-packet) ---
+
+def l10_readback_law(rb, local):
+    """LAUNCH-10 content law: verify the console readback of the replaced
+    composition PRX against the LOCAL launch-10 FSELF (no separate bare-sdk
+    reference is pinned for this build). OK requires EITHER full byte identity,
+    OR exactly the known ftpsrv SHT-strip tail-artifact class: the readback is
+    the LOCAL file's segment-covered prefix (max p_offset+p_filesz), body bytes
+    64..end byte-identical, every differing header byte inside SHT_HDR_ALLOWED,
+    and e_shnum zeroed in the readback header. Anything else — a different
+    length, a divergent body, a foreign header byte — is a corrupted staging:
+    STOP. Returns (ok, detail)."""
+    if rb == local:
+        return True, "readback byte-identical to the local FSELF (%d bytes)" % len(rb)
+    if len(rb) < 4 or rb[:4] != b"\x7fELF":
+        return False, "READBACK-FAIL not an ELF (magic %r)" % rb[:4]
+    if len(local) < 64 or local[:4] != b"\x7fELF":
+        return False, ("READBACK-FAIL local FSELF is not parseable as an ELF and the readback is "
+                       "not byte-identical — cannot attribute the divergence to the SHT-strip "
+                       "class; STOP")
+    try:
+        end = sht_prefix_end(local)
+    except struct.error:
+        return False, "READBACK-FAIL local FSELF program headers unparseable"
+    if len(rb) != end:
+        return False, ("READBACK-FAIL len=%d vs local FSELF %d bytes (segment-covered prefix %d) — "
+                       "neither full identity nor the SHT-strip class" % (len(rb), len(local), end))
+    if rb[64:end] != local[64:end]:
+        return False, "READBACK-FAIL body bytes 64..%d diverge from the local FSELF" % end
+    hdr_diff = [i for i in range(64) if rb[i] != local[i]]
+    outside = [hex(i) for i in hdr_diff if i not in SHT_HDR_ALLOWED]
+    if outside:
+        return False, "READBACK-FAIL header bytes outside the allowed SHT-strip set %s: %s" % (
+            sorted(hex(i) for i in SHT_HDR_ALLOWED), outside)
+    if struct.unpack_from("<H", rb, 60)[0] != 0:
+        return False, "READBACK-FAIL e_shnum not zeroed in the readback header"
+    return True, ("READBACK-OK len=%d (SHT-strip class of the local %d-byte FSELF: body "
+                  "byte-identical 64..%d, e_shnum=0)" % (len(rb), len(local), end))
+
+
+def build_stage_launch10():
+    """LAUNCH-10 STAGING packet (controller staging facts, 2026-10-04): replace
+    /data/plugins/modbridge_supplier_composition.prx with the REBUILT launch-10
+    composition FSELF (--fself + --expect-sha; the identity is a PLACEHOLDER
+    until the build lands and the flow refuses placeholders). ploader.ini STAYS
+    the recorded 2-plugin config (composition + path_test enabled, sha
+    1216aab6…) — it is read twice (frozen BEFORE any console change, re-checked
+    AFTER the replacement) and NEVER written by this flow. Ride.9c guard law,
+    launch-10 form: pre-image + console .bak FIRST; the FSELF is stored to its
+    FINAL name EXACTLY ONCE; the readback is verified by the content law against
+    the LOCAL file (full identity, or ONLY the ftpsrv SHT-strip tail-artifact
+    class); on ANY failed verification (upload ambiguity, readback law, ini
+    drift) the OLD PRX is restored byte-exact from the saved pre-image — ONE
+    bounded restore, no blind retries — and the flow refuses. Fresh pre-launch
+    baselines (UE4SS.log, runtime-status, entry log, supplier-diag jsonl) are
+    re-fetched into the run dir, then the GO/NO-GO summary. The next game launch
+    is USER-driven: the tool NEVER launches."""
+    src = L10_FSELF_LOCAL_PATH
+    expect_sha = (L10_FSELF_SHA256 or "").strip().lower()
+    expect_ini = L10_INI_EXPECTED_SHA
+    fl = Flow("stage-launch10")
+    fl.notes = ("LAUNCH-10 STAGING: ONE console file changes (%s <- the rebuilt launch-10 "
+                "composition FSELF, --fself/--expect-sha). Pre-image + console .bak first; "
+                "FSELF stored EXACTLY ONCE; readback content law vs the LOCAL file (SHT-strip "
+                "class handled); ploader.ini verified STILL byte-equal %s… and NEVER written; "
+                "fresh baselines; GO/NO-GO. On any failed verification the OLD PRX is restored "
+                "from the pre-image. The next launch is USER-driven — the tool never launches."
+                % (L10["target_prx"], expect_ini[:12]))
+
+    def preflight_fn_l10(ctx):
+        problems = []
+        if not src:
+            problems.append("<L10_FSELF_LOCAL_PATH> placeholder NOT filled — pass --fself <path>; "
+                            "the flow refuses to stage an unidentified build")
+        elif not os.path.exists(src) or os.path.getsize(src) == 0:
+            problems.append("local launch-10 FSELF missing/empty: %s" % src)
+        if not expect_sha:
+            problems.append("<L10_FSELF_SHA256> placeholder NOT filled — pass --expect-sha <sha256>")
+        elif not re.fullmatch(r"[0-9a-f]{64}", expect_sha):
+            problems.append("--expect-sha is not a lowercase hex sha256: %r" % expect_sha)
+        got_sha = got_n = None
+        if src and os.path.exists(src) and expect_sha and re.fullmatch(r"[0-9a-f]{64}", expect_sha):
+            got_sha, got_n = local_sha(src)
+            if got_sha != expect_sha:
+                problems.append("BUILD-ID LAW: %s hashes %s/%s, pinned --expect-sha %s — STOP, "
+                                "no console step runs" % (src, got_sha, got_n, expect_sha))
+            else:
+                with open(src, "rb") as f:
+                    magic = f.read(4)
+                ctx.state["l10_fself"] = {"path": src, "sha256": got_sha, "bytes": got_n,
+                                          "elf": magic == b"\x7fELF"}
+        ctx.save("l10-preflight.json", json.dumps(
+            {"fself": src, "expect_sha256": expect_sha, "got_sha256": got_sha,
+             "got_bytes": got_n, "problems": problems}, indent=2))
+        if problems:
+            return "FAIL", "; ".join(problems)
+        return "PASS", "launch-10 FSELF identity verified (%d bytes, sha %s…, ELF=%s)" % (
+            got_n, got_sha[:12], ctx.state["l10_fself"]["elf"])
+
+    fl.step("l10.0", "LAUNCH-10 preflight — the rebuilt FSELF identity must be PINNED "
+                     "(--fself + --expect-sha; placeholders are a loud refusal — no "
+                     "unidentified build is ever staged) and must hash to the pinned sha "
+                     "(BUILD-ID law)", "preflight",
+            ["local sha256(--fself) == --expect-sha (BUILD-ID law; no console step runs on mismatch)",
+             "unfilled <L10_FSELF_LOCAL_PATH>/<L10_FSELF_SHA256> placeholders = REFUSAL",
+             "identity recorded to l10-preflight.json"],
+            ["shasum -a 256 <L10_FSELF_LOCAL_PATH>",
+             "stat -f %%z <L10_FSELF_LOCAL_PATH>"],
+            preflight_fn_l10)
+
+    fl.step("l10.0b", "G0 reachability — pldmgr 8084 answers /list_payloads (else STOP — do "
+                      "not probe alternate ports)", "g0",
+            ["GET %s/list_payloads -> HTTP 200" % HTTP],
+            ["curl -sS %s/list_payloads" % HTTP],
+            curl_get(HTTP + "/list_payloads", outfile="list_payloads.stage-launch10.json",
+                     name="G0 list_payloads"))
+
+    def ini_freeze_fn(ctx):
+        p = os.path.join(ctx.run_dir, "l10-ploader.ini.pre")
+        rc, _out, _err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + L10["ini"], "-o", p],
+                                 "RETR ploader.ini (freeze — read-only)")
+        if ctx.dry:
+            return "PASS", "dry"
+        if rc != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
+            return "FAIL", ("no ploader.ini retrievable (curl rc=%s) — target unreadable; STOP "
+                            "before any console change" % rc)
+        sha, n = local_sha(p)
+        ctx.save("l10-ploader.ini.pre.sha256", "%s  %d\n" % (sha, n))
+        ctx.state["l10_ini_freeze"] = {"sha256": sha, "bytes": n}
+        if sha != expect_ini:
+            return "FAIL", ("ploader.ini does NOT match the pinned 2-plugin config (got %s…/%d B, "
+                            "pinned %s…) — staging REFUSED before any console change; reconcile the "
+                            "drift first; this flow never writes the ini"
+                            % ((sha or "?")[:12], n, expect_ini[:12]))
+        return "PASS", ("ploader.ini frozen byte-exact vs the pinned 2-plugin config "
+                        "(%d B, sha %s…) — read-only; NO step of this flow ever writes it"
+                        % (n, sha[:12]))
+
+    fl.step("l10.1", "INI FREEZE (read-only) — RETR ploader.ini and require byte-exact equality "
+                     "with the pinned 2-plugin config sha 1216aab6… BEFORE any console change; "
+                     "anything else refuses the staging at zero console writes", "ini-freeze",
+            ["pinned 2-plugin config: composition + path_test enabled (sha 1216aab6…)",
+             "drift here = REFUSAL before ANY console write (the old PRX is untouched by definition)",
+             "this flow NEVER issues an ini STOR — the ini is read at l10.1 and re-checked at l10.5"],
+            ["curl -sS %s%s -o <run-dir>/l10-ploader.ini.pre" % (FTP, L10["ini"]),
+             "shasum -a 256 <run-dir>/l10-ploader.ini.pre"],
+            ini_freeze_fn)
+
+    def prx_preimage_fn(ctx):
+        p = os.path.join(ctx.run_dir, "l10-composition.prx.pre")
+        rc, _out, _err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + L10["target_prx"], "-o", p],
+                                 "RETR pre-image of the current composition PRX")
+        if ctx.dry:
+            return "PASS", "dry"
+        if rc != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
+            return "FAIL", ("no pre-image retrievable for %s (curl rc=%s) — a replacement is staged "
+                            "ONLY with the old bytes saved; STOP" % (L10["target_prx"], rc))
+        sha, n = local_sha(p)
+        ctx.save("l10-composition.prx.pre.sha256", "%s  %d\n" % (sha, n))
+        ctx.state["l10_pre"] = {"sha256": sha, "bytes": n}
+        tag = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
+        rc2, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", p,
+                                  FTP + L10["target_prx"] + "." + tag + ".bak"],
+                            "console .bak backup of the current composition PRX")
+        if rc2 != 0:
+            return "FAIL", "could not write the console .bak — STOP (never replace without a rollback artifact)"
+        return "PASS", "pre-image secured (%d bytes, sha %s…) + console .bak written (rollback artifacts FIRST)" % (n, sha[:12])
+
+    fl.step("l10.2", "PRE-IMAGE + .bak FIRST — RETR the current composition PRX (readback sha + "
+                     "size saved to the run dir) and write the console .bak backup BEFORE any "
+                     "replacement; these are the ride.9c-style rollback artifacts", "preimage",
+            ["pre-image RETR + sha recorded BEFORE the replacement (Gate A0 pattern)",
+             "console .bak with a distinct TAG — the operator-visible rollback artifact",
+             "no pre-image = STOP (never blind-replace)"],
+            ["curl -sS %s%s -o <run-dir>/l10-composition.prx.pre" % (FTP, L10["target_prx"]),
+             "shasum -a 256 <run-dir>/l10-composition.prx.pre",
+             "curl -sS -T <run-dir>/l10-composition.prx.pre %s%s.$TAG.bak" % (FTP, L10["target_prx"])],
+            prx_preimage_fn)
+
+    def upload_fn(ctx):
+        if ctx.dry:
+            return "PASS", "dry"
+        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", src, FTP + L10["target_prx"]],
+                           "STOR launch-10 FSELF (EXACTLY ONCE)")
+        if rc != 0:
+            note = _l10_restore_old_prx_once(ctx, "upload STOR failed rc=%d (ambiguous whether bytes landed)" % rc)
+            return "FAIL", "upload STOR failed rc=%d — %s" % (rc, note)
+        ctx.state["l10_upload_stor"] = 1
+        return "PASS", "launch-10 FSELF stored to %s EXACTLY ONCE (no re-STOR; readback content law next)" % L10["target_prx"]
+
+    fl.step("l10.3", "UPLOAD EXACTLY ONCE — STOR the pinned launch-10 FSELF to its FINAL name "
+                     "(the ONE console write of this flow; on a failed STOR the pre-image is "
+                     "restored once and the flow refuses — no blind retry)", "upload",
+            ["the FSELF is stored ONCE to %s (in-place replacement; .bak + local pre-image already secured)" % L10["target_prx"],
+             "exactly-once law: ambiguity is reconciled, never re-sent",
+             "on failure: OLD PRX restored byte-exact from the pre-image, then refusal"],
+            ["curl -sS -T <L10_FSELF_LOCAL_PATH> %s%s" % (FTP, L10["target_prx"])],
+            upload_fn)
+
+    def readback_fn(ctx):
+        if ctx.dry:
+            return "PASS", "dry"
+        p = os.path.join(ctx.run_dir, "l10-composition.readback.prx")
+        rc, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + L10["target_prx"], "-o", p],
+                           "RETR readback of the replaced composition PRX")
+        if rc != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
+            note = _l10_restore_old_prx_once(ctx, "readback fetch failed rc=%s" % rc)
+            return "FAIL", "readback fetch failed rc=%s — %s" % (rc, note)
+        rb = open(p, "rb").read()
+        rb_sha, rb_n = local_sha(p)
+        local = open(src, "rb").read()
+        ok, detail = l10_readback_law(rb, local)
+        ctx.save("l10-composition.readback.sha256", "%s  %d\n" % (rb_sha, rb_n))
+        ctx.save("l10-readback-verdict.json", json.dumps(
+            {"target_prx": L10["target_prx"],
+             "law": "launch-10 content law vs the LOCAL FSELF (full identity, or ONLY the "
+                    "ftpsrv SHT-strip tail-artifact class: segment-covered prefix + "
+                    "SHT_HDR_ALLOWED header diff + e_shnum=0)",
+             "local_sha256": ctx.state.get("l10_fself", {}).get("sha256"),
+             "local_bytes": ctx.state.get("l10_fself", {}).get("bytes"),
+             "readback_sha256": rb_sha, "readback_bytes": rb_n,
+             "ok": ok, "detail": detail}, indent=2))
+        if not ok:
+            note = _l10_restore_old_prx_once(ctx, "readback failed the content law: %s" % detail)
+            return "FAIL", "READBACK MISMATCH — %s; %s" % (detail, note)
+        ctx.state["l10_readback_ok"] = True
+        return "PASS", "readback verified vs the LOCAL FSELF: %s" % detail
+
+    fl.step("l10.4", "READBACK CONTENT LAW — RETR the replaced PRX and verify it against the "
+                     "LOCAL launch-10 FSELF: full byte identity, or ONLY the known ftpsrv "
+                     "SHT-strip tail-artifact class (segment-covered prefix, body byte-identical, "
+                     "e_shnum zeroed); ANY other divergence restores the OLD PRX from the "
+                     "pre-image and refuses (no re-STOR of the new FSELF)", "verify",
+            ["ok = full identity OR the SHT-strip class (len == max(p_offset+p_filesz), body 64..end "
+             "identical, header diff within SHT_HDR_ALLOWED, e_shnum=0)",
+             "readback sha + size recorded regardless of verdict (l10-composition.readback.sha256)",
+             "law FAIL = corrupted staging: STOP, restore, never dispatch bytes-divergent content"],
+            ["curl -sS %s%s -o <run-dir>/l10-composition.readback.prx" % (FTP, L10["target_prx"]),
+             "shasum -a 256 <run-dir>/l10-composition.readback.prx"],
+            readback_fn)
+
+    def ini_still_fn(ctx):
+        p = os.path.join(ctx.run_dir, "l10-ploader.ini.post")
+        rc, _out, _err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + L10["ini"], "-o", p],
+                                 "RETR ploader.ini (STILL byte-equal check — read-only)")
+        if ctx.dry:
+            return "PASS", "dry"
+        if rc != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
+            return "FAIL", ("ploader.ini post-check unreadable (curl rc=%s) — cannot prove the "
+                            "config is untouched; STOP" % rc)
+        sha, n = local_sha(p)
+        ctx.save("l10-ploader.ini.post.sha256", "%s  %d\n" % (sha, n))
+        ctx.state["l10_ini_post"] = {"sha256": sha, "bytes": n}
+        if sha != expect_ini:
+            note = _l10_restore_old_prx_once(ctx, "ploader.ini drifted during staging (got %s…)" % (sha or "?")[:12])
+            return "FAIL", ("ploader.ini CHANGED during staging (got %s…/%s B, pinned %s…) — REFUSED; "
+                            "%s; this flow issued NO ini write — the drift came from outside; reconcile"
+                            % ((sha or "?")[:12], n, expect_ini[:12], note))
+        ctx.state["l10_ini_post_ok"] = True
+        return "PASS", ("ploader.ini STILL byte-equals the pinned 2-plugin config (sha %s…) — "
+                        "untouched; NO ini write exists anywhere in this flow" % sha[:12])
+
+    fl.step("l10.5", "INI STILL BYTE-EQUAL — re-RETR ploader.ini and require sha 1216aab6… "
+                     "AFTER the replacement; ANY drift refuses the staging and restores the OLD "
+                     "PRX (the flow never wrote the ini — the drift is external; reconcile)", "ini-post",
+            ["pinned 2-plugin config sha 1216aab6… must be unchanged before AND after the PRX swap",
+             "drift here = OLD PRX restored + loud refusal; NO ini change is ever made by this flow"],
+            ["curl -sS %s%s -o <run-dir>/l10-ploader.ini.post" % (FTP, L10["ini"]),
+             "shasum -a 256 <run-dir>/l10-ploader.ini.post"],
+            ini_still_fn)
+
+    L10_BASELINES = (("ue4ss_log", "baseline-UE4SS.log"),
+                     ("runtime_status", "baseline-runtime-status.txt"),
+                     ("entry_log", "baseline-entry-log.txt"),
+                     ("diag", "baseline-supplier-diag.jsonl"))
+
+    def baselines_fn(ctx):
+        manifest = {}
+        for key, out_name in L10_BASELINES:
+            ok, text = fetch_console_file(ctx, L10[key].lstrip("/"), out_name)
+            if ctx.dry:
+                continue
+            manifest[key] = {"path": L10[key], "file": out_name,
+                             "present": bool(ok and text != ""),
+                             "bytes": len(text.encode("utf-8", "replace")) if ok else 0}
+            if ok and text:
+                sha, _n = local_sha(os.path.join(ctx.run_dir, out_name))
+                manifest[key]["sha256"] = sha
+        ctx.state["l10_baselines"] = True
+        ctx.save("l10-baselines.json", json.dumps(
+            {"law": "fresh pre-launch baselines re-fetched into this run dir (supersedes the "
+                    "controller's launch10-prep captures as the comparison point for post-launch "
+                    "evidence; absence recorded honestly, never deleted)",
+             "post_launch": "the next game launch is USER-driven — the tool NEVER launches",
+             "baselines": manifest}, indent=2))
+        return "PASS", "fresh baselines re-fetched into the run dir: " + "; ".join(
+            "%s %s" % (k, ("present (%d B)" % v["bytes"]) if v["present"] else "absent/empty (recorded)")
+            for k, v in manifest.items())
+
+    fl.step("l10.6", "FRESH BASELINES — immediately pre-launch, re-fetch UE4SS.log, runtime-status, "
+                     "the entry log and /data/modbridge-supplier-diag.jsonl (if present) into THIS "
+                     "run dir; absence is recorded honestly (never delete owned evidence); the "
+                     "launch10-prep captures remain the older reference set", "baseline",
+            ["UE4SS.log / runtime-status.txt / entry log / supplier-diag.jsonl baselined with sha+size when present",
+             "absence is a valid recorded baseline (never a failure, never deleted)",
+             "post-launch evidence compares against THESE files (run_stage_launch10.sh post)"],
+            ["curl -sS %s%s -o <run-dir>/baseline-UE4SS.log" % (FTP, L10["ue4ss_log"]),
+             "curl -sS %s%s -o <run-dir>/baseline-runtime-status.txt" % (FTP, L10["runtime_status"]),
+             "curl -sS %s%s -o <run-dir>/baseline-entry-log.txt" % (FTP, L10["entry_log"]),
+             "curl -sS %s%s -o <run-dir>/baseline-supplier-diag.jsonl   # if present" % (FTP, L10["diag"])],
+            baselines_fn)
+
+    def go_nogo_fn(ctx):
+        checks = {
+            "preflight_fself_identity": bool(ctx.state.get("l10_fself")),
+            "ini_freeze_matched_pinned": bool(ctx.state.get("l10_ini_freeze")),
+            "pre_image_saved_and_bak": bool(ctx.state.get("l10_pre")),
+            "fself_uploaded_exactly_once": ctx.state.get("l10_upload_stor") == 1,
+            "readback_content_law_ok": ctx.state.get("l10_readback_ok") is True,
+            "ini_still_byte_equal": ctx.state.get("l10_ini_post_ok") is True,
+            "console_state_clean": ctx.state.get("l10_console_state", "new-prx-staged") == "new-prx-staged",
+            "baselines_recorded": ctx.state.get("l10_baselines") is True,
+        }
+        go = all(checks.values())
+        ctx.save("l10-go-nogo.json", json.dumps(
+            {"verdict": "GO" if go else "NO-GO", "checks": checks,
+             "target_prx": L10["target_prx"],
+             "ini_law": "ploader.ini pinned %s… — verified byte-equal at l10.1 and l10.5; "
+                        "this flow NEVER wrote it" % expect_ini[:12],
+             "next": ("USER launches the game (the tool NEVER launches); then run "
+                      "run_stage_launch10.sh post to fetch the post-launch evidence"
+                      if go else
+                      "reconcile; the console .bak and l10-restore-verdict.json document "
+                      "the console state; never blind-retry")}, indent=2))
+        if go:
+            return "PASS", ("GO — launch-10 staged: %s replaced + content-law verified; ploader.ini "
+                            "untouched (%s…); baselines recorded. Next: USER launches the game — "
+                            "the tool NEVER launches." % (L10["target_prx"], expect_ini[:12]))
+        return "FAIL", "NO-GO — failed gates: %s" % ", ".join(k for k, v in checks.items() if not v)
+
+    fl.step("l10.7", "GO/NO-GO SUMMARY — all launch-10 gates must be green (identity, ini freeze, "
+                     "pre-image+.bak, exactly-once upload, content-law readback, ini STILL "
+                     "1216aab6…, console state clean, baselines recorded); the verdict lands in "
+                     "l10-go-nogo.json and the next launch stays USER-driven (the tool NEVER "
+                     "launches)", "summary",
+            ["GO requires every gate green; anything else is a loud NO-GO with the failed gates named",
+             "runs even after a halt (rescue) so the NO-GO verdict + failed gates are always recorded",
+             "the tool NEVER launches the game — the launch window is the user's"],
+            ["cat <run-dir>/l10-go-nogo.json"],
+            go_nogo_fn, rescue=True)
     return fl
+
+
+def _l10_restore_old_prx_once(ctx, reason):
+    """Ride.9c guard law, launch-10 form: on any failed verification, return the
+    console to the OLD PRX — ONE bounded byte-exact STOR of the saved pre-image,
+    proven by a fresh RETR — and report. Never a second attempt (reconcile
+    first); the console .bak remains as the operator's rollback artifact;
+    ploader.ini is NEVER touched."""
+    pre = ctx.state.get("l10_pre")
+    if not pre:
+        ctx.state["l10_console_state"] = "unknown-no-preimage"
+        return "NO RESTORE POSSIBLE: no pre-image recorded this run"
+    if ctx.state.get("l10_restore_attempted"):
+        return "restore already attempted once this run (bounded; reconcile first — never replay)"
+    ctx.state["l10_restore_attempted"] = True
+    pre_local = os.path.join(ctx.run_dir, "l10-composition.prx.pre")
+    rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", pre_local, FTP + L10["target_prx"]],
+                       "RESTORE old composition PRX from the saved pre-image")
+    ok = False
+    rb_sha = rb_n = None
+    if rc == 0:
+        p = os.path.join(ctx.run_dir, "l10-restore.readback.prx")
+        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + L10["target_prx"], "-o", p],
+                            "restore readback")
+        if not ctx.dry and rc2 == 0 and os.path.exists(p):
+            rb_sha, rb_n = local_sha(p)
+            ok = (rb_sha, rb_n) == (pre["sha256"], pre["bytes"])
+    ctx.save("l10-restore-verdict.json", json.dumps(
+        {"reason": reason, "restored": ok, "pre_sha256": pre["sha256"],
+         "pre_bytes": pre["bytes"], "readback_sha256": rb_sha, "readback_bytes": rb_n,
+         "console_bak": L10["target_prx"] + ".<TAG>.bak"}, indent=2))
+    if ok:
+        ctx.state["l10_console_state"] = "old-prx-restored"
+        return ("OLD PRX RESTORED byte-exact (%d bytes, sha %s…) — console left as found; "
+                "reconcile before any retry" % (pre["bytes"], pre["sha256"][:12]))
+    ctx.state["l10_console_state"] = "RESTORE-FAILED"
+    return ("RESTORE FAILED (readback %s…/%s) — the console .bak %s.<TAG>.bak remains; "
+            "reconcile MANUALLY; no retry" % ((rb_sha or "?")[:12], rb_n, L10["target_prx"]))
 
 
 # --- stage-settings (NEW-SETTINGS §3 lockstep restage, v3 pair) -----------------
@@ -4525,6 +4971,7 @@
          "stage-ridealongs": build_stage_ridealongs, "stage-observer": build_stage_observer,
          "stage-qdrv1": build_stage_qdrv1, "stage-settings": build_stage_settings,
          "rollback-ridealongs": build_rollback_ridealongs, "restore-ini": build_restore_ini,
+         "stage-launch10": build_stage_launch10,
          "game-evidence": build_game_evidence, "all": build_all}
 
 
@@ -4702,6 +5149,110 @@
     check("guard: a mode-flipped line joins the enabled set (mode != 0 loads)",
           sorted(en4) == sorted(en + ["/data/plugins/modbridge_ingame_facts.prx"]))
 
+    print("== selftest A5: LAUNCH-10 staging packet (worker-launch10-staging-packet) ==")
+    steps_l, ids_l = steps_of("stage-launch10")
+    check("launch-10 order: preflight -> g0 -> ini freeze -> PRX pre-image -> upload EXACTLY ONCE "
+          "-> readback law -> ini STILL byte-equal -> baselines -> GO/NO-GO",
+          ids_l == ["l10.0", "l10.0b", "l10.1", "l10.2", "l10.3", "l10.4", "l10.5", "l10.6", "l10.7"],
+          str(ids_l))
+    check("launch-10 ini law: NO step carries any ploader.ini STOR (-T) — the flow never writes the ini",
+          not any("-T" in c and "ploader.ini" in c for st in steps_l.values() for c in st["cmds"]),
+          str([c for st in steps_l.values() for c in st["cmds"] if "ploader.ini" in c]))
+    check("launch-10 ini steps are read-only RETRs pinning sha 1216aab6… (freeze l10.1 + still l10.5)",
+          sum(1 for st in steps_l.values() for c in st["cmds"]
+              if "ploader.ini" in c and "-o" in c) >= 2
+          and all("1216aab6" in "\n".join(steps_l[sid]["gates"] + [steps_l[sid]["title"]])
+                  for sid in ("l10.1", "l10.5")))
+    check("launch-10 exactly-once law: exactly ONE non-.bak STOR command to the composition PRX, "
+          "after the pre-image/.bak step",
+          sum(1 for st in steps_l.values() for c in st["cmds"] if "-T" in c
+              and "modbridge_supplier_composition.prx" in c and ".bak" not in c) == 1
+          and ids_l.index("l10.2") < ids_l.index("l10.3"))
+    check("launch-10 rollback artifacts: the .bak backup lives in the PRE-image step (l10.2), "
+          "BEFORE the upload (l10.3)",
+          any(".bak" in c for c in steps_l["l10.2"]["cmds"]), str(steps_l["l10.2"]["cmds"]))
+    check("launch-10 GO/NO-GO is a rescue step (the NO-GO verdict is recorded even after a halt)",
+          steps_l["l10.7"]["rescue"] is True and "NO-GO" in steps_l["l10.7"]["title"])
+    ctx = Ctx(None, dry=True); FLOWS["stage-launch10"]().run(ctx); l10v = "\n".join(ctx.events)
+    for anchor in ["modbridge_supplier_composition.prx", "1216aab6", "SHT-strip", ".bak",
+                   "EXACTLY ONCE", "GO/NO-GO", "modbridge-supplier-diag.jsonl", "UE4SS.log",
+                   "runtime-status.txt", "townfall_runtime_aligned_entry.log",
+                   "the tool NEVER launches"]:
+        check("stage-launch10 anchor %r" % anchor[:40], anchor in l10v)
+
+    # launch-10 content law (pure): full identity OR ONLY the ftpsrv SHT-strip class
+    import tempfile
+    _l10_tmp = tempfile.mkdtemp(prefix="l10-selftest-")
+
+    def _l10_fixture_elf(path, body_len=1024):
+        """Synthetic launch-10 FSELF: a 64-bit ELF whose segment coverage ends BEFORE
+        EOF (a fake section-header tail follows) and whose e_shnum is NONZERO — a
+        console readback of it must exercise the SHT-strip tail-artifact branch."""
+        ehdr = bytearray(64)
+        ehdr[0:4] = b"\x7fELF"
+        ehdr[4] = 2
+        struct.pack_into("<HHI", ehdr, 16, 2, 0xf6, 1)
+        phoff = 64
+        struct.pack_into("<Q", ehdr, 32, phoff)                 # e_phoff
+        struct.pack_into("<H", ehdr, 54, 56)                    # e_phentsize
+        struct.pack_into("<H", ehdr, 56, 1)                     # e_phnum
+        struct.pack_into("<Q", ehdr, 40, phoff + 56 + body_len)  # e_shoff -> the tail
+        struct.pack_into("<H", ehdr, 58, 64)                    # e_shentsize
+        struct.pack_into("<H", ehdr, 60, 3)                     # e_shnum NONZERO
+        struct.pack_into("<H", ehdr, 62, 2)                     # e_shstrndx
+        ph = struct.pack("<IIQQQQQQ", 1, 5, 0, phoff + 56, phoff + 56, body_len, body_len, 8)
+        body = bytes((i * 31 + 7) % 256 for i in range(body_len))
+        tail = bytes((i * 13 + 3) % 256 for i in range(256))     # fake SHT tail past coverage
+        blob = bytes(ehdr) + ph + body + tail
+        with open(path, "wb") as f:
+            f.write(blob)
+        return path
+
+    _l10_fself = _l10_fixture_elf(os.path.join(_l10_tmp, "l10.fself"))
+    _local = open(_l10_fself, "rb").read()
+    _end = sht_prefix_end(_local)
+    check("launch-10 law fixture: segment-covered prefix ends before EOF (SHT tail exists)",
+          0 < _end < len(_local), "%d/%d" % (_end, len(_local)))
+    ok, det = l10_readback_law(_local, _local)
+    check("launch-10 law: full byte identity passes", ok, det)
+    _rb = bytearray(_local[:_end]); _rb[60:62] = b"\x00\x00"
+    ok, det = l10_readback_law(bytes(_rb), _local)
+    check("launch-10 law: SHT-strip tail-artifact readback (prefix + e_shnum=0) passes", ok, det)
+    ok, _ = l10_readback_law(bytes(_rb)[:-1], _local)
+    check("launch-10 law: truncated readback (neither identity nor the prefix) FAILS", not ok)
+    _bad_body = bytearray(bytes(_rb)); _bad_body[100] ^= 0xFF
+    ok, _ = l10_readback_law(bytes(_bad_body), _local)
+    check("launch-10 law: body divergence inside the prefix FAILS", not ok)
+    _bad_hdr = bytearray(bytes(_rb)); _bad_hdr[20] ^= 0xFF   # outside SHT_HDR_ALLOWED
+    ok, _ = l10_readback_law(bytes(_bad_hdr), _local)
+    check("launch-10 law: header byte outside the allowed SHT set FAILS", not ok)
+    ok, _ = l10_readback_law(_local[:-1], _local)
+    check("launch-10 law: length off by one vs both accepted shapes FAILS", not ok)
+    ok, _ = l10_readback_law(b"", _local)
+    check("launch-10 law: empty readback FAILS", not ok)
+
+    # placeholder + BUILD-ID refusals (l10.0 runs host-only; Ctx(run_dir=None) keeps it inert)
+    _saved_l10 = (globals()["L10_FSELF_LOCAL_PATH"], globals()["L10_FSELF_SHA256"])
+    globals()["L10_FSELF_LOCAL_PATH"] = ""
+    globals()["L10_FSELF_SHA256"] = ""
+    _st0 = [s for s in build_stage_launch10().steps if s["id"] == "l10.0"][0]
+    status, detail = _st0["fn"](Ctx(run_dir=None))
+    check("launch-10 preflight REFUSES unfilled placeholders (no unidentified build is staged)",
+          status == "FAIL" and "L10_FSELF_LOCAL_PATH" in detail and "L10_FSELF_SHA256" in detail, detail)
+    globals()["L10_FSELF_LOCAL_PATH"] = _l10_fself
+    globals()["L10_FSELF_SHA256"] = "0" * 64
+    _st0 = [s for s in build_stage_launch10().steps if s["id"] == "l10.0"][0]
+    status, detail = _st0["fn"](Ctx(run_dir=None))
+    check("launch-10 preflight REFUSES a BUILD-ID mismatch (hash != --expect-sha)",
+          status == "FAIL" and "BUILD-ID LAW" in detail, detail)
+    globals()["L10_FSELF_SHA256"] = hashlib.sha256(_local).hexdigest()
+    _st0 = [s for s in build_stage_launch10().steps if s["id"] == "l10.0"][0]
+    status, detail = _st0["fn"](Ctx(run_dir=None))
+    check("launch-10 preflight PASSES with the pinned fixture identity (ELF recorded)",
+          status == "PASS" and "ELF=True" in detail, detail)
+    globals()["L10_FSELF_LOCAL_PATH"], globals()["L10_FSELF_SHA256"] = _saved_l10
+
+
     ctx = Ctx(None, dry=True); FLOWS["stage-settings"]().run(ctx); st = "\n".join(ctx.events)
     for anchor in ["d8faa4ec", "4d98ecd7", "1448", "447", "lockstep", "FREEZE",
                    "UE4SS-settings.ini", "loadlist.txt", "MFA_E_SIZE", "9570ead3",
@@ -5467,7 +6018,7 @@
     p = sub.add_parser("plan", help="dry-run: print every step, gate and exact command; NO execution")
     p.add_argument("--only", default="all",
                    help="comma list: dll8,dll7,e2e,stage-ridealongs,stage-observer,stage-qdrv1,"
-                        "stage-settings,rollback-ridealongs,restore-ini,game-evidence,all")
+                        "stage-settings,rollback-ridealongs,restore-ini,stage-launch10,game-evidence,all")
     p.add_argument("--dll", choices=("7", "8"), default=None,
                    help="DLL stage 'plan --only all' previews: 8 (preferred, default) or 7 (fallback)")
     add_flow("dll8", "8-family DLL payload-only stage (game closed) — PREFERRED single DLL run")
@@ -5503,6 +6054,22 @@
                            help="explicit operator override: allow restoring over a console ini "
                                 "that matches NEITHER the source NOR a classified pre-state "
                                 "(recorded loudly as DIVERGENT-PRE-ACCEPTED; default is REFUSE)")
+    p_l10 = add_flow("stage-launch10", "LAUNCH-10 STAGING packet (game closed): replace "
+                                     "/data/plugins/modbridge_supplier_composition.prx with the "
+                                     "REBUILT launch-10 composition FSELF — pre-image + console "
+                                     ".bak FIRST, the FSELF stored EXACTLY ONCE, readback content "
+                                     "law vs the LOCAL file (ftpsrv SHT-strip tail-artifact class "
+                                     "handled), ploader.ini verified STILL byte-equal to the pinned "
+                                     "2-plugin config (sha 1216aab6…) and NEVER written, fresh "
+                                     "baselines, GO/NO-GO. On ANY failed verification the OLD PRX "
+                                     "is restored byte-exact from the saved pre-image")
+    p_l10.add_argument("--fself", default="",
+                       help="local path of the rebuilt launch-10 composition FSELF "
+                            "(<L10_FSELF_LOCAL_PATH>; REQUIRED — the flow refuses unfilled "
+                            "placeholders, no unidentified build is ever staged)")
+    p_l10.add_argument("--expect-sha", default="",
+                       help="pinned sha256 of the local FSELF (<L10_FSELF_SHA256>; REQUIRED — "
+                            "BUILD-ID law: the local file must hash to this or no console step runs)")
     add_flow("game-evidence", "post-launch evidence + CP0-CP7 + probe verdict sinks incl. the cc1 "
                               "census (interactive user handoffs)")
     add_flow("all", "full recommended session: dll8 -> e2e -> stage-ridealongs -> stage-settings -> "
@@ -5518,9 +6085,14 @@
         return selftest()
     global SELECTED_DLL
     global RESTORE_SRC_PATH, RESTORE_ALLOW_DIVERGENT_PRE
+    global L10_FSELF_LOCAL_PATH, L10_FSELF_SHA256
     if getattr(args, "ini_src", None):
         RESTORE_SRC_PATH = os.path.abspath(args.ini_src)
         RESTORE_ALLOW_DIVERGENT_PRE = bool(getattr(args, "allow_divergent_pre", False))
+    if getattr(args, "fself", None):
+        L10_FSELF_LOCAL_PATH = os.path.abspath(args.fself)
+    if getattr(args, "expect_sha", None):
+        L10_FSELF_SHA256 = args.expect_sha.strip().lower()
     sel = getattr(args, "dll", None)
     if sel:
         SELECTED_DLL = "dll" + sel

--- /tmp/l10-snap-fake_console_drive.py
+++ worker-console-batch-runner/tests/fake_console_drive.py
--- /tmp/l10-snap-fake_console_drive.py	2026-10-05 08:00:38
+++ outputs/2026-10-04-zcode-team/worker-console-batch-runner/tests/fake_console_drive.py	2026-10-05 08:10:25
@@ -19,6 +19,7 @@
 """
 
 import importlib.util
+import hashlib
 import json
 import os
 import shutil
@@ -418,16 +419,49 @@
                 if content:
                     with open(out_target, "w") as f:
                         f.write(content)
+                    self.l10_ini_retr_n["n"] += 1
+                    # launch-10 drift scenario: AFTER the freeze RETR (l10.1) the
+                    # console ini diverges, so the l10.5 STILL-byte-equal check sees it
+                    if self.l10_drift_after_first_ini_retr["on"] and self.l10_ini_retr_n["n"] == 1:
+                        self.stor[INI_URL] = L10_DIVERGED_INI
                     return 0, ""
                 return 8, ""  # no prior ini
             return 0, self.stor.get(cb.QDRV1["ini"], "")
-        if "modbridge_supplier_composition.prx" in joined or \
-           "modbridge_runtime_aligned_entry.prx" in joined or \
+        if "modbridge_supplier_composition.prx" in joined:
+            # launch-10 staging: the composition PRX is the ONE replaced file. STORs
+            # are recorded (console .bak keeps the tailing .$TAG.bak name); RETRs
+            # serve the last staged content (ELF -> SHT-strip view) or the seeded
+            # pre-image, falling back to the historical static text the pre-publish
+            # guard scenarios expect.
+            if data_target:
+                kind = "bak" if ".bak" in joined else "prx"
+                self.l10_stors.append((kind, joined.split()[-1], os.path.getsize(data_target)))
+                self.stor_sequence.append((kind, joined.split()[-1]))
+                if kind == "prx":
+                    with open(data_target, "rb") as f:
+                        self.l10_staged = f.read()
+                return 0, ""
+            content = (self.l10_staged if self.l10_staged is not None
+                       else self.l10_pre_bytes)
+            if content is None:
+                return emit("PRE-EXISTING-CONSOLE-FSELF supplier_composition (fake bytes)\n")
+            if self.l10_staged is not None and self.l10_corrupt_once["on"]:
+                # one-shot law-divergent readback (corrupts only a STAGED-content fetch,
+                # never the pre-image or the restore readback)
+                self.l10_corrupt_once["on"] = False
+                content = content[:-64] + b"CORRUPTED-L10-READBACK\n"
+            elif self.l10_staged is not None:
+                content = _l10_law_readback(content)
+            if out_target:
+                with open(out_target, "wb") as f:
+                    f.write(content)
+                return 0, ""
+            return 0, content.decode("utf-8", "replace")
+        if "modbridge_runtime_aligned_entry.prx" in joined or \
            "modbridge_path_test.prx" in joined:
             # historical PRE-EXISTING console plugins (never uploaded by the tool):
             # the staging-order pre-publish guard proves their presence via RETR
-            name = ("supplier_composition" if "supplier_composition" in joined
-                    else "aligned_entry" if "aligned_entry" in joined else "path_test")
+            name = ("aligned_entry" if "aligned_entry" in joined else "path_test")
             return emit("PRE-EXISTING-CONSOLE-FSELF %s (fake bytes)\n" % name)
         if "modbridge_runtime_aligned_observer.prx" in joined:
             # observer ridealong PRX (STAGE-PLAN-COMBINED): readback follows the
@@ -511,6 +545,29 @@
 fc._pending = 0
 fc._pending_next = 0
 fc.settings_staged_v3 = False
+# launch-10 staging state (worker-launch10-staging-packet): the composition PRX is
+# the ONE replaced console file. l10_pre_bytes seeds the OLD PRX content (None =
+# the historical static text the pre-publish guard scenarios expect); l10_staged
+# holds whatever was last STORed to the final name (the fake serves the ftpsrv
+# SHT-strip view of staged ELFs, verbatim for non-ELF content).
+fc.l10_pre_bytes = None
+fc.l10_staged = None
+fc.l10_stors = []            # ("bak"|"prx", url, nbytes) — every composition STOR, in order
+fc.l10_ini_retr_n = {"n": 0}  # ploader.ini RETR counter (l10.1 freeze = #1, l10.5 still = #2)
+fc.l10_drift_after_first_ini_retr = {"on": False}
+fc.l10_corrupt_once = {"on": False}
+
+
+def _l10_law_readback(content):
+    """Simulate the ftpsrv channel for a STORed ELF: the segment-covered prefix
+    with e_shnum zeroed; non-ELF content is served verbatim."""
+    try:
+        end = cb.sht_prefix_end(content)
+    except Exception:
+        return content
+    rb = bytearray(content[:end])
+    rb[60:62] = b"\x00\x00"
+    return bytes(rb)
 xwp_fetch_n = {"n": 0}
 fc1_fetch_n = {"n": 0}
 cc1_fetch_n = {"n": 0}
@@ -1007,7 +1064,184 @@
 
 cb.RESTORE_SRC_PATH = cb.RESTORE_INI["src_default"]
 cb.RESTORE_ALLOW_DIVERGENT_PRE = False
+
+# ---------------------------------------------------------------------------
+# LAUNCH-10 STAGING scenarios (worker-launch10-staging-packet): the composition
+# PRX is the ONE replaced console file; ploader.ini stays the pinned 2-plugin
+# config (composition + path_test) and is NEVER written; the readback content
+# law accepts full identity or ONLY the ftpsrv SHT-strip tail-artifact class.
+# ---------------------------------------------------------------------------
+
+print("== fake-console drive: stage-launch10 setup (fixture FSELF + pinned 2-plugin ini) ==")
+
+
+def l10_fixture_elf(path, body_len=1024):
+    """Synthetic launch-10 FSELF: segment coverage ends BEFORE EOF (a fake SHT
+    tail follows) and e_shnum is NONZERO — the fake console serves the SHT-strip
+    view of it, so the flow's content law must use its tail-artifact branch."""
+    import struct as _s
+    ehdr = bytearray(64)
+    ehdr[0:4] = b"\x7fELF"
+    ehdr[4] = 2
+    _s.pack_into("<HHI", ehdr, 16, 2, 0xf6, 1)
+    phoff = 64
+    _s.pack_into("<Q", ehdr, 32, phoff)                      # e_phoff
+    _s.pack_into("<H", ehdr, 54, 56)                         # e_phentsize
+    _s.pack_into("<H", ehdr, 56, 1)                          # e_phnum
+    _s.pack_into("<Q", ehdr, 40, phoff + 56 + body_len)      # e_shoff -> the tail
+    _s.pack_into("<H", ehdr, 58, 64)                         # e_shentsize
+    _s.pack_into("<H", ehdr, 60, 3)                          # e_shnum NONZERO
+    _s.pack_into("<H", ehdr, 62, 2)                          # e_shstrndx
+    ph = _s.pack("<IIQQQQQQ", 1, 5, 0, phoff + 56, phoff + 56, body_len, body_len, 8)
+    body = bytes((i * 31 + 7) % 256 for i in range(body_len))
+    tail = bytes((i * 13 + 3) % 256 for i in range(256))
+    with open(path, "wb") as f:
+        f.write(bytes(ehdr) + ph + body + tail)
+    return path
+
+
+L10_RUN_ROOT = fresh("l10")
+L10_FSELF_PATH = l10_fixture_elf(os.path.join(L10_RUN_ROOT, "modbridge_supplier_composition_launch10.fself"))
+L10_FSELF_SHA = hashlib.sha256(open(L10_FSELF_PATH, "rb").read()).hexdigest()
+L10_OLD_PRX = b"[fake] OLD composition FSELF bytes (pre-launch-10 console content)\n" * 4
+L10_TWO_PLUGIN_INI = cb.FINAL_INI["header"] + "\n%s = 2\n%s = 2\n" % (C_LINE, P_LINE)
+L10_INI_SHA = hashlib.sha256(L10_TWO_PLUGIN_INI.encode()).hexdigest()
+L10_DIVERGED_INI = L10_TWO_PLUGIN_INI + "%s = 2\n" % Q_LINE   # a third enabled line appears
+L10_TARGET_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.L10["target_prx"]
+check("launch-10 fixture ini is the 2-plugin config shape (composition + path_test)",
+      L10_TWO_PLUGIN_INI.count("= 2") == 2 and C_LINE in L10_TWO_PLUGIN_INI and P_LINE in L10_TWO_PLUGIN_INI,
+      repr(L10_TWO_PLUGIN_INI))
+
+
+def l10_reset(seed_text=L10_TWO_PLUGIN_INI, pre_bytes=L10_OLD_PRX):
+    seed_ini(seed_text)
+    fc.l10_pre_bytes = pre_bytes
+    fc.l10_staged = None
+    fc.l10_stors = []
+    fc.stor_sequence = []
+    fc.l10_ini_retr_n["n"] = 0
+    fc.l10_drift_after_first_ini_retr["on"] = False
+    fc.l10_corrupt_once["on"] = False
+
+
+def l10_arm():
+    cb.L10_FSELF_LOCAL_PATH = L10_FSELF_PATH
+    cb.L10_FSELF_SHA256 = L10_FSELF_SHA
+    cb.L10_INI_EXPECTED_SHA = L10_INI_SHA
+
+
+print("== fake-console drive: stage-launch10 (a) happy path: .bak -> upload EXACTLY ONCE -> "
+      "content-law readback -> ini STILL byte-equal -> baselines -> GO ==")
+l10_reset()
+l10_arm()
+ctx = cb.Ctx(run_dir=fresh("l10-happy"), dry=False, assume_yes=True)
+res = cb.build_stage_launch10().run(ctx)
+bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
+check("stage-launch10 happy path all steps PASS", not bad, str(bad))
+statuses = dict(res)
+check("console sequence is EXACTLY: composition .bak backup -> ONE FSELF STOR (final name)",
+      [s[0] for s in fc.l10_stors] == ["bak", "prx"] and fc.l10_stors[1][1] == L10_TARGET_URL,
+      str(fc.l10_stors))
+check("the FSELF landed byte-exact (fake stored the staged bytes) with NO re-STOR, NO restore",
+      fc.l10_staged == open(L10_FSELF_PATH, "rb").read() and len(fc.l10_stors) == 2)
+check("readback verified by the content law vs the LOCAL FSELF (SHT-strip tail-artifact branch)",
+      json.load(open(os.path.join(ctx.run_dir, "l10-readback-verdict.json")))["ok"] is True
+      and "SHT-strip class" in json.load(open(os.path.join(ctx.run_dir, "l10-readback-verdict.json")))["detail"])
+check("ploader.ini NEVER written (no ini STOR) and still the 2-plugin bytes",
+      not any(s[0] == "ini" for s in fc.stor_sequence) and stored_ini() == L10_TWO_PLUGIN_INI,
+      str(fc.stor_sequence))
+check("ini freeze (l10.1) + STILL-byte-equal (l10.5) both ran and pinned the 2-plugin config",
+      fc.l10_ini_retr_n["n"] == 2
+      and os.path.exists(os.path.join(ctx.run_dir, "l10-ploader.ini.pre.sha256"))
+      and os.path.exists(os.path.join(ctx.run_dir, "l10-ploader.ini.post.sha256")))
+check("fresh baselines re-fetched into the run dir (UE4SS.log + runtime-status + entry log + manifest)",
+      os.path.exists(os.path.join(ctx.run_dir, "baseline-UE4SS.log"))
+      and os.path.exists(os.path.join(ctx.run_dir, "baseline-runtime-status.txt"))
+      and os.path.exists(os.path.join(ctx.run_dir, "baseline-entry-log.txt"))
+      and os.path.exists(os.path.join(ctx.run_dir, "l10-baselines.json")))
+check("GO verdict recorded (next launch stays USER-driven)",
+      statuses.get("l10.7") == "PASS"
+      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "GO")
+
+print("== fake-console drive: stage-launch10 (b) readback law FAIL -> OLD PRX restored from the "
+      "pre-image, ini untouched, NO-GO recorded ==")
+l10_reset()
+fc.l10_corrupt_once["on"] = True
+ctx = cb.Ctx(run_dir=fresh("l10-mismatch"), dry=False, assume_yes=True)
+res = cb.build_stage_launch10().run(ctx)
+statuses = dict(res)
+check("l10.4 FAILS on the law-divergent readback (upload-verify mismatch)", statuses.get("l10.4") == "FAIL",
+      statuses.get("l10.4"))
+check("the OLD PRX was restored byte-exact from the pre-image (exactly ONE bounded restore STOR)",
+      fc.l10_staged == L10_OLD_PRX and [s[0] for s in fc.l10_stors] == ["bak", "prx", "prx"],
+      str(fc.l10_stors))
+check("the FSELF itself was stored EXACTLY ONCE (no re-STOR of the new payload)",
+      [s for s in fc.l10_stors if s[0] == "prx"][0][2] == os.path.getsize(L10_FSELF_PATH)
+      and sum(1 for s in fc.l10_stors if s[0] == "prx" and s[2] == os.path.getsize(L10_FSELF_PATH)) == 1)
+check("restore proven byte-exact (l10-restore-verdict.json)", json.load(
+    open(os.path.join(ctx.run_dir, "l10-restore-verdict.json")))["restored"] is True)
+check("ini STILL check SKIPPED after the halt; NO ini STOR ever; old ini bytes intact",
+      statuses.get("l10.5") == "SKIPPED_HALT" and not any(s[0] == "ini" for s in fc.stor_sequence)
+      and stored_ini() == L10_TWO_PLUGIN_INI)
+check("NO-GO verdict recorded by the rescue summary step", statuses.get("l10.7") == "FAIL"
+      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "NO-GO")
 
+print("== fake-console drive: stage-launch10 (c) ini DRIFT between freeze and re-check -> refusal, "
+      "OLD PRX restored, the flow still never wrote the ini ==")
+l10_reset()
+fc.l10_drift_after_first_ini_retr["on"] = True
+ctx = cb.Ctx(run_dir=fresh("l10-drift"), dry=False, assume_yes=True)
+res = cb.build_stage_launch10().run(ctx)
+statuses = dict(res)
+check("upload + content-law readback PASSED before the drift was seen",
+      statuses.get("l10.3") == "PASS" and statuses.get("l10.4") == "PASS", str((statuses.get("l10.3"), statuses.get("l10.4"))))
+check("l10.5 REFUSES the drifted ploader.ini", statuses.get("l10.5") == "FAIL", statuses.get("l10.5"))
+check("the refusal names the PINNED 2-plugin sha (fixture stand-in for 1216aab6…) and the external drift",
+      cb.L10_INI_EXPECTED_SHA[:12] in "\n".join(ctx.events) and "NO ini write" in "\n".join(ctx.events))
+check("the OLD PRX was restored byte-exact after the refusal",
+      fc.l10_staged == L10_OLD_PRX and json.load(
+          open(os.path.join(ctx.run_dir, "l10-restore-verdict.json")))["restored"] is True)
+check("baselines step SKIPPED after the refusal; NO ini STOR ever (drifted bytes remain — "
+      "the flow does not reconcile the ini itself)",
+      statuses.get("l10.6") == "SKIPPED_HALT" and not any(s[0] == "ini" for s in fc.stor_sequence)
+      and stored_ini() == L10_DIVERGED_INI)
+check("NO-GO verdict recorded by the rescue summary step", statuses.get("l10.7") == "FAIL"
+      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "NO-GO")
+
+print("== fake-console drive: stage-launch10 (d) missing / unpinned local FSELF -> refusal at "
+      "ZERO console writes ==")
+l10_reset()
+cb.L10_FSELF_LOCAL_PATH = os.path.join(L10_RUN_ROOT, "does-not-exist.fself")
+cb.L10_FSELF_SHA256 = L10_FSELF_SHA
+ctx = cb.Ctx(run_dir=fresh("l10-missing"), dry=False, assume_yes=True)
+res = cb.build_stage_launch10().run(ctx)
+statuses = dict(res)
+check("l10.0 REFUSES the missing local FSELF (BUILD-ID preflight)", statuses.get("l10.0") == "FAIL",
+      statuses.get("l10.0"))
+check("every console step SKIPPED — ZERO console writes",
+      all(statuses.get(s) == "SKIPPED_HALT" for s in ("l10.1", "l10.2", "l10.3", "l10.4", "l10.5", "l10.6"))
+      and fc.l10_stors == [] and not any(s[0] == "ini" for s in fc.stor_sequence))
+check("NO-GO verdict recorded by the rescue summary step", statuses.get("l10.7") == "FAIL"
+      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "NO-GO")
+cb.L10_FSELF_LOCAL_PATH = ""
+cb.L10_FSELF_SHA256 = ""
+ctx = cb.Ctx(run_dir=fresh("l10-placeholder"), dry=False, assume_yes=True)
+res = cb.build_stage_launch10().run(ctx)
+statuses = dict(res)
+check("l10.0 REFUSES unfilled <L10_FSELF_LOCAL_PATH>/<L10_FSELF_SHA256> placeholders",
+      statuses.get("l10.0") == "FAIL" and "placeholder" in "\n".join(ctx.events).lower(),
+      statuses.get("l10.0"))
+check("placeholder refusal: zero console writes",
+      fc.l10_stors == [] and not any(s[0] == "ini" for s in fc.stor_sequence))
+
+# restore the module defaults so later scenarios see the shipped pinned state
+cb.L10_FSELF_LOCAL_PATH = ""
+cb.L10_FSELF_SHA256 = ""
+cb.L10_INI_EXPECTED_SHA = cb.L10["ini_sha256"]
+fc.l10_pre_bytes = None
+fc.l10_staged = None
+fc.l10_stors = []
+
 print("== fake-console drive: stage-settings (NEW-SETTINGS §3 lockstep restage, v3 pair) ==")
 fc.settings_staged_v3 = False  # console currently holds the recorded v2 pair
 ctx = cb.Ctx(run_dir=fresh("settings-v3"), dry=False, assume_yes=True)

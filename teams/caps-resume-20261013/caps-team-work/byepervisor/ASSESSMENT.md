# ASSESSMENT — Byepervisor Reborn (PS5Dev/Byepervisor-Reborn) as an additional hypervisor component

- Author: agent JE, Mac-only research (no downloads, no builds, no console contact)
- Date: 2026-10-13
- Scope: documented capabilities only. Target context: our PS5 FW 13.60 stack = WebKit Autoloader + kstuff-lite v1.11 (pinned, hash-verified) + ploader + ShadowMount Plus (FTPSRV) + UE4SS-port PRX.
- Verdict up front: **Technically compatible and 13.60-supported, but TOO FRESH TO INTEGRATE.** Complement, not replacement. Recommend HOLD behind a manual pilot gate until the risks in section 7 clear.

Repo assessed: https://github.com/PS5Dev/Byepervisor-Reborn ("Somehow, Byepervisor returned...")
Pinned provenance point: single visible commit `ea04f7132b837087f552c57261cb800481a3a910` ("Second Commit", ChendoChap, **Oct 12, 2026**) — the repo is ~1 day old as of this assessment.

---

## 1. What is it? What does a PS5 hypervisor do in practice?

Byepervisor Reborn is a **PS5 hypervisor (HV) escape exploit**, not a patcher/HEN. The PS5 runs the game OS as a guest under a Sony hypervisor enforced by AMD nested paging (NPT) with per-core VMCBs (VM Control Blocks). "Defeating the hypervisor" in practice means getting code or writes into the host (root 0) view, which controls the NPT/VMCB state that defines the guest/host boundary. Practical downstream powers (all documented for this family of bugs):

- Disable XOM / "xotext" (execute-only kernel text) so kernel `.text` becomes **readable and writable** from the guest kernel — kernel dumps, kernel patching, hooking.
- Write the nested-paging root itself (VMCB `NESTED_CTRL`, `NESTED_CR3`, `Control2`) — the strongest position below bare metal.
- Foundation capabilities the original Byepervisor demonstrated on old FW: HEN (fself+fpkg), SELFs decryption over TCP, kernel dumps.

Reborn's bug (per README + soniciso1's writeup `s3-resume-startup_mp-ap-launch.md`): during **S3 resume (rest mode)**, `startup_mp` (HCALL 0x0f) re-opens the one-shot AP-startup window; the 15 parked AP cores sit at the host NPT root (`cur_npt == 0`) where VMCBs are visible, and the hypercall accepts a guest-supplied start RIP/CR3. A one-shot kernel hook launches attacker shellcode at a fixed physical page (0x3FFFF000) with kernel CR3; the `break_hv_payload` runs in supervisor mode, walks the HV `pcpus[]` array, locates all 16 VMCBs, and the payload then clears NX on its own page and clears the XOTEXT bit + sets RW on every kernel `.text` page. Success handshake is a cookie (0xCAFE133741414141) surfaced as "Welcome back, hypervisor is defeated!" plus a notification dumping the first 8 bytes of kernel `.text` as proof.

Important precondition (from the writeup): this is a **post-compromise boundary crossing** — it requires kernel R/W + ROP already (i.e., an existing kernel exploit chain). It is not an unprivileged entry.

## 2. Firmware support (we are on 13.60)

From the README:

- **13.60 — fully supported** (our firmware). 13.20 supported. 13.00 listed but untested.
- Bug is believed exploitable down to **6.xx** with new `include/offsets/` headers and switch-statement work in `kgadget()`, `kdlsym()`, `rawoff()`.
- **14.00 not included** (no public 14.00 kernel exploit to precondition it).
- Porting note from the HV writeup: offsets are firmware-specific and fragile (e.g., between 10.00 and 13.60 `hv_heap_offset` shifted `0x2a048` → `0x2a028`).

Caveat: open issues show 13.60 is not smooth for everyone (see section 7).

## 3. How is it loaded? Coexistence with WebKit Autoloader + kstuff

Documented load path (README):

1. Run the kernel exploit chain to reach **elfldr** (ps5-payload-dev/elfldr).
2. Send `byepervisor.elf` to the console (elfldr accepts payloads on **port 9021**).
3. Follow notifications → **enter rest mode → resume** (the trigger). Prerequisite setting: Power Saving → "Supply Power to USB Ports" = **Always**.

Coexistence with our stack:

- **WebKit Autoloader**: untouched and still required. Reborn rides *downstream* of the kernel entry; on 13.60 it does not remove our WebKit dependency. Note: elfldr's documented persistence is across **rest mode and game launches**, not across full reboot — same per-boot WebKit entry as today.
- **kstuff-lite v1.11**: same delivery channel (ELF over elfldr 9021). Operationally identical to what we already do; nothing in Reborn touches autoload.txt, ploader, or /data/plugins. Order would be: WebKit chain → kstuff-lite (as today) → send byepervisor.elf → rest/resume.
- **Untested interaction**: Reborn installs a one-shot IDT/TSS hook on core 0 targeting `hv_mp_startup_call`; kstuff also modifies the kernel. No documented data on running both. Reborn has idempotence guards (a `STATE_ARMED` marker in a kernel cave; reruns skip reinstall), but kstuff+Reborn sequencing on 13.60 is unverified anywhere.
- Delivery is ELF-over-network to elfldr — not an FTP SELF upload — so the fake-signing rules in AGENTS.md (make_fself) do not apply to `byepervisor.elf` itself. If staged on /data via FTP it is a data file; still verify sha256 with a decrypt-OFF (`SITE SELF` off) readback per PS5 rules.

## 4. Capabilities

From README + `main.c` (commit ea04f71):

- One-shot core-0 hook (IST 6) on the kernel HV MP-startup symbol; redirects the AP start RIP to the staged payload page (0x3FFFF000) with kernel CR3; ROP gadgets copy `break_hv` from a kernel data cave into that page.
- `break_hv_payload/` — flat binary, **all code runs in supervisor/kernel mode** (this is the HV-context piece).
- Post-resume result: **XOM/xotext disabled, kernel write enabled** — kernel dump and patch becomes possible.
- Demo kernel `.text` write (0x41414141) proving the primitive.
- Auto PS5 Pro vs base detection via `sceKernelGetMainSocId()` (`0x840fd0` = Pro); different HV physical addresses (base `0x062800000`, Pro `0x8E0800000`; Sony gave the Pro's extra 2GB to the HV).
- **No patches included — no HEN.** README: "bring your own patches scenario." It is an enabler/primitive, not an end-user feature set.

What you do NOT get (documented): persistent HV resident (hook is uninstalled after success; the *defeated state* persists for the power session via the changed page-table/VMCB state), 14.00 support, HEN/FPKG features, dev-kit equivalence.

## 5. Relationship to kstuff (replace / complement / conflict)

**Complement today; potential future underpinning; not a replacement.**

- kstuff-lite v1.11 is our kernel *patcher* (the functional layer). Reborn ships no patches and provides the *substrate* (writable kernel text, XOM off) more robustly than guest-side tricks, because the write-protection defeat happens at the HV/NPT level rather than as ordinary kernel patches.
- It does not remove the need for kstuff-lite on 13.60: the kernel exploit chain that reaches elfldr is still the precondition, and no HEN ships with Reborn.
- Conflict risk: both modify the same kernel; kstuff already provides kernel R/W on our setup, so Reborn's kernel-level effects are partly redundant with it. The genuinely new value is HV-context control (VMCB/NPT) and a cleaner base for future kstuff-style patchers (community narrative: enables e.g. Linux-on-13.60 work). Exact coexistence is unverified — see risks.

## 6. License

- **Byepervisor-Reborn: NO LICENSE file** (repo tree confirms none). Default = all rights reserved; redistribution/derivative distribution is not permitted without permission. Building/running locally from the public pinned source for private research is the scene norm but legally unlicensed — pin the commit hash and do not redistribute artifacts.
- Contrast: original PS5Dev/Byepervisor (1.xx–2.xx, Specter et al.) is **Unlicense**, credits ChendoChap, flatz, fail0verflow, Znullptr, kiwidog, sleirsgoevy. Reborn credits theflow0 (bug discoverer) + soniciso1's writeups; LLM-assisted rediscovery acknowledged.

## 7. Risks and limitations

1. **Maturity: 1 visible commit, no releases/tags, repo is ~1 day old** (Oct 12, 2026). Supply-chain discipline requires building from the pinned commit hash ourselves; there is no release artifact to hash-pin.
2. **Open 13.60 stability reports (dated Oct 13, 2026 — today):**
   - Issue #8: console **powers off and reboots** when run on 13.6 (contradicts the README's "99.9% stable" claim; single unverified report).
   - Issue #6: **PS5 Pro kernel trap 12 (page fault in kernel mode)** when the hook activates on 13.60; works on standard units.
3. **Failure mode is a shutdown on resume** — a hard power-cut at the one moment the kernel/HV is being rewritten. Worst possible time for our /data (plugins, games) and for the kstuff paused-state discipline in AGENTS.md (failed launches leave kstuff paused until clean exit; a mid-session shutdown is the messy path).
4. **New operation class for our stack**: we never currently exercise a rest-mode suspend/resume cycle in the mod ladder. elfldr's rest-mode persistence is documented upstream, but our ploader/ftpsrv/UE4SS PRX behavior across S3 is untested by us.
5. **Narrow timing window** (post-`resumectx`, pre-`acpi_wakeup_cpus`) with an arm/verify/disarm state machine — fragility is inherent; the reference writeup required a breakpoint-armed ROP chain to hit it.
6. **No license** (section 6) — legal restriction on redistribution; provenance must be commit-hash pinned.
7. **Offsets are firmware-pinned**; community PRs are still filling gaps (11.60 PR #7; 12.20/12.40 requests #3/#5) — 13.60 exists but field reports vary.
8. **Model detection**: if our unit were misdetected (Pro vs base), wrong HV physical address → crash. Verify against the bracketed hex in the notification.
9. **Firmware-update bait**: HV bugs are exactly what Sony patches; irrelevant to us only as long as we never update FW (we don't).

## 8. Compatibility with our stack (summary table)

| Component | Impact | Notes |
|---|---|---|
| FW 13.60 | Supported | "Fully supported" per README; but see issues #6/#8 |
| WebKit Autoloader | Required, unchanged | Reborn is downstream of the kernel chain |
| kstuff-lite v1.11 | Coexists, unverified ordering | Same elfldr channel; no HEN in Reborn so kstuff stays primary |
| elfldr (port 9021) | Required | Already in our path; documented rest-mode persistence |
| ploader / autoload.txt | Untouched | No config changes; ploader stays last line of autoload.txt |
| ShadowMount Plus (FTPSRV) | Untouched | Only used for optional /data staging + decrypt-OFF hash verify |
| UE4SS PRX | Untouched | No interaction documented; game-launch ladder unaffected |
| Rest-mode cycle | NEW requirement | USB power "Always" setting; new risk surface for us |
| Fake-signing rules | Not triggered | ELF via elfldr, not a SELF upload |

## 9. Does it support "kernel reads with no game running" (requirement #1)?

**Yes — after a successful arm + rest/resume cycle, for the rest of that power session.** The HV-defeated state is system-wide (page-table/VMCB state), not tied to a process; kernel R/W is then available to elfldr payloads sitting at the home screen, no game running.

Two honest caveats:

- **Entry still requires the WebKit chain once per boot** (no documented persistence across full power-off). So the no-game property is session-scoped, same as our current kstuff-lite flow.
- Our existing stack **already provides kernel reads with no game running** (kstuff-lite loaded via elfldr persists across game launches). Reborn's marginal value for #1 is *depth and robustness* (HV-level write-protection defeat instead of guest-side patches), not a new capability class.

## 10. What would 3-restart acceptance look like?

Proposed ladder, honoring AGENTS.md PS5 rules (no rapid-fire launch tests, decrypt-OFF verify, one variable per session, preserve raw provenance, read receipts before submissions):

**Pre-flight (Mac only)**
- Build locally from pinned commit `ea04f7132b837087f552c57261cb800481a3a910`; record sha256 of `byepervisor.elf`; never redistribute (no license).
- Confirm our unit is a base PS5 (not Pro) and check model-detect hex on first run; confirm "Supply Power to USB Ports = Always" on console (user action).
- Snapshot current working stack state (autoload.txt, /data/plugins listing, kstuff-lite hash) for before/after comparison.

**Per-iteration (x3, each starting from a clean FULL power-off, minimum spacing per the no-rapid-fire rule)**
1. Cold boot → WebKit Autoloader → kstuff-lite v1.11 (existing pinned flow) → confirm elfldr listening on 9021.
2. Send pinned `byepervisor.elf` (nc to 9021) → expect "Enter sleep and resume!" notification; record STATE_ARMED marker text.
3. Manual rest mode → resume.
4. **Pass signal**: "hypervisor is defeated" + cookie 0xCAFE133741414141 + first 8 bytes of kernel `.text` shown; **fail signal** = console shutdown on resume (documented failure mode; issue #8 pattern) or any trap/panic (issue #6 pattern).
5. **#1 requirement probe with NO game running**: minimal read-only elfldr probe reading kernel `.text` base bytes; log output; compare against the notification's 8 bytes.
6. **Stack-intact check**: ftpsrv reachable; ploader `/loadpayload` cycle OK; (optionally deferred to a separate session) UE4SS PRX loads in one game launch, clean exit observed, kstuff pause/resume behavior normal.
7. Full power-off before the next iteration (never rest-mode chaining).

**Pass criteria**: 3/3 successful HV defeats on resume; 3/3 no-game kernel reads; 0 unexpected shutdowns/panics; 0 changes needed to ploader/autoload/kstuff state; before/after stack snapshot identical.

**Abort criteria (stop immediately, preserve logs, no same-day retries)**: any trap 12, power-off on resume, elfldr/ftpsrv breakage, or kstuff pause-state anomaly. One failed iteration = do not attempt the remaining two that day.

**Post-acceptance only**: consider it pilot-listed as an optional manual step, never part of the automated daily ladder, until community 13.60 base-unit stability is broadly confirmed.

---

## Sources

- https://github.com/PS5Dev/Byepervisor-Reborn (README, tree, issues/PRs; commit ea04f7132b837087f552c57261cb800481a3a910 by ChendoChap, 2026-10-12)
- https://raw.githubusercontent.com/PS5Dev/Byepervisor-Reborn/main/main.c (hook, patches, Pro detection, cookie handshake)
- https://github.com/soniciso1/ps5-hv-writeups (s3-resume-startup_mp-ap-launch.md — bug mechanics, preconditions, firmware fragility)
- https://github.com/PS5Dev/Byepervisor (original; capability set, Unlicense, credits)
- https://github.com/ps5-payload-dev/elfldr (port 9021, rest-mode/game-launch persistence, deployment)
- https://hardwear.io/talks/byepervisor-how-we-broke-the-ps5-hypervisor (background talk)
- r/PS5_Jailbreak threads (community FW-support discussion)
- /WORKSPACE/Projects/ai-game-studio/AGENTS.md — PS5 CONSOLE RULES + PS5 MODDING MISSION sections

# l18 single-test plan — NOT started

Local build approval does not authorize console contact or changes. Await the
user's explicit PRX-only upload OK and confirmation that Townfall is closed with
FTP on. No remote launch, payload dispatch, configuration change or deletion.

Before an approved upload, read-only checks must establish the actual settings:
the current loadlist declares UE4SS-settings.ini as 1947 bytes. The retained
1947-byte settings copy disables HookProcessInternal and
HookProcessLocalScriptFunction, but the saved loadlist does not prove its current
contents. Read the current loaded settings and require both flags to be 0 before
testing this XOM build; stop and report a difference, without editing anything.

Under separate explicit approval, use the established procedure:

1. Obtain a fresh executable backup with curl -Q "SITE SELF" on the same FTP
   connection. Confirm decrypt-off response, full hash and SELF magic; preserve
   that exact live PRX as rollback. Expect l16 only if the fresh evidence agrees.
2. Upload only the final fake-signed l18 PRX to its owned /data/plugins path.
   Read it back decrypt-off; require the full BUILD-REPORT SHA and SELF magic.
3. Preserve ini/loadlist/autoload/autotune/mod files. Create a NEW klog capture,
   report that it is connected, then wait for ONE user launch.
4. The user runs their existing loader and launches Townfall once, loads their
   save and presses L3+R3. No subsequent launch before the read-only harvest.

Harvest and associate each record with this launch's PID/timestamps/fresh log
boundaries; an installed artifact or Ploader OK alone is not engine success:

- Startup: every acquired copyin FD, unlink results, code_image_cache_ok,
  code_image_verified_pins, bytes_read, close result, configured path/reason,
  image elapsed time and the original 60000ms admission budget.
- Pool: full admission table, source, outcome and elapsed time; report whether
  the mod remains installed or logs Stopping mod.
- FName: exact constructor VirtualQuery needed/region/protection/flags; PT_LOAD
  and any actual module-info segment; identity/base facts; source
  authenticated-disk-XOM-signature or fresh-live-copyin; signature result;
  ByteProperty native constructor roundtrip and binding result.
- Downstream: last runtime/UE4SS line, controller borrow/open/read results,
  actual L3+R3/F7 input event, carrier selection and delivery, original widget
  creation/AddToViewport, menu or spawn evidence. Absence remains absence.
- Crash: correlate klog/registers/module+offset if present; do not manufacture
  a backtrace. Read autotune.ini and list remaining owned sink files; delete none.

Map the result to the existing SMOKE-CHECKLIST. Name all observed blockers and
propose ONE next change without applying it. A missing menu is not evidence
that signature verification failed: identify the last completed boundary.

The l18 disk prefix is authenticated static-image evidence, never a fresh live
code-byte observation. It grants no inline-hook write permission. SCO/LoadMap
detours, readable vtables, writable UFunction slots, controller and original
mod menu/spawn retain the limits stated in the XOM audit and build report.

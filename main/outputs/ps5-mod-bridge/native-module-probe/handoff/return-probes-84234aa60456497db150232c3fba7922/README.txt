RETURN-ONLY PROBES: A0 AND A2

Both files are native module candidates with zero imports, no init-array callback,
no file writes and no notifications. They use the same embedded module identity.
A0 exports module_start returning 0. A2 exports module_start returning -1.
The source is probe.c; the only source-level selection is MODBRIDGE_START_RESULT.
Compiled return instructions, local packaging and wrapper roundtrips are checked.
Console loading and entry execution are NOT yet proven.

Files and paths
  modbridge_entry_A0.prx -> /data/plugins/modbridge_entry_A0.prx
  modbridge_entry_A2.prx -> /data/plugins/modbridge_entry_A2.prx
  ploader-A0.ini and ploader-A2.ini are separate Townfall section templates.
  Ploader reads /data/plugins/ploader.ini, not the template filenames.

Test ONE variant per fresh game process
1. Fully close Townfall before changing its test selection.
2. Preserve your existing ploader.ini. Copy the two probe files to the paths above.
3. Apply only the A0 Townfall section to ploader.ini, preserving other game sections.
   Disable the previous modbridge_log_probe entry and any other Townfall probe.
4. Run your existing Ploader, then launch Townfall. Record its exact notification,
   any printed numeric error, and whether the game keeps running or crashes.
5. Fully close Townfall. Apply only the A2 section; resend Ploader and launch again.
   A full game restart is required because both probes have the same embedded identity.
6. Restore your original Townfall configuration after both tests.

Interpretation limits
Ploader 1.1.0 bases OK/FAIL on the load/start API return and supplies no pointer
for the separate module-start result. We have not established whether a negative
module-start result changes that API return on this native PS5 path.
- A0 OK / A2 FAIL: consistent with entry-return sensitivity; report both outcomes.
- Both OK: entry may run with its result ignored; this does NOT prove it was skipped.
- Both FAIL: the baseline module was not accepted; entry execution remains unknown.
Neither probe creates a log or shows its own notification. That absence is expected.
A-open is intentionally deferred until these results can be interpreted.

No Ploader, game files or console settings have been modified by this local build.

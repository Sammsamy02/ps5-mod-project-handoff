# #5 Crash-log symbolization — Mac-only

crash_report.py is an offline postmortem tool, with8 passing behavior tests. It scopes symbols to an explicit module and refuses mismatched PRX/sidecar hashes; nearest matches outside function bounds stay module-only. This is not a live debugger.

Fresh local evidence: the newer packaging sidecar passes complete JSON parsing, duplicate-key checks and the existing loader's hash pairing against packaged PRX06913711…; bare extraction PRX2fb013ef… is separately verified. Full39,100,809-byte sidecar SHA2c3c1c61… and source stability are recorded in symbol-input-audit/ROOT-LOADER-PAIRING-REPLAY.json. Only two exact dynamic entry/exit anchors are loaded. No .symtab exists; partial-link -r reconstruction remains approximate and unused. No duplicate generator or build was needed.

Preserved capture replay still yields two RIPs at libkernel.sprx+0x224c, both MODULE_ONLY. Current local symbol pairing does not bind that capture to this build, prove deployed identity or provide full internal symbols. The earlier stand-in sidecar9ef26a8f…/old PRX7fead02d… mismatch remains historical refusal evidence; it is not the status of the newly observed pair.

With a sidecar, use the exact artifact its prx_sha256 names (currently packaged SELF); without a sidecar, direct dynsym requires bare ELF. README.md gives the distinction. Next: retain real final-layout symbols plus a producer receipt tying them to bare/packaged bytes, and capture-module/build association. #4+#5 launch should validate capture identity; do not deliberately crash the current build to manufacture proof. No console contact, uploads, builds or original writes.

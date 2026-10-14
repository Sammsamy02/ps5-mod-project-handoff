# l16 retained probe handles — approved local plan

User approved local build/host tests only. No console contact or uploads. Preserve frozen l15, PC mod files, admission, FName ABI/signature/roundtrip, pad and shared deadline.

- Reproduce actual l15 late _openat EPERM using the production extracted probe and real host file descriptors.
- Acquire a bounded bank of exclusive game-created read/write probe handles during module_start. Consume each once; fresh kernel write-copyin followed by positional same-handle readback, no late lookup/create/reopen. Keep fault ledger and all deadline checks; close handles on failed startup and acknowledged stop.
- Compare cached prologues: reject disk/startup-only cache as replacement for a fresh live check; it can miss changes between startup and call. Retained handles preserve freshness.
- Log UID/EUID/GID/EGID, public sandbox indicator, named jail query with honest unavailable/error reporting; startup and delayed snapshots plus complete probe file-failure records.
- Host red/green, error/partial/fault/budget/cleanup cases; public SDK import evidence, target compile, full launch11-derived two-build/import/package/manifest gates.
- Report-only universal Lua file IO options and mod API gaps/call order.
- Artifact/receipt report; HOLD for explicit upload approval.

Resource design: one single-use O_RDWR handle per probe (64 maximum, inherited existing probe-attempt bound), positional readback at offset0. Initial file creation performs no target reads. Process exit closes descriptors; acknowledged module_stop and failed-startup scope explicitly close all. Evidence files are retained, never blindly deleted. Creation latency is reported separately; OS calls cannot be preempted by the 60s admission budget.

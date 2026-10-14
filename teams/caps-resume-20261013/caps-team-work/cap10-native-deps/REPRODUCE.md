# Dependency diagnostic reproduction

This task executed one diagnostic against an existing approved binary; it did not modify the frozen `cap10-mono-build` experiment.

The command, working directory, controlled environment additions, previous-proof reference, and before/after hashes are in `evidence/OWNER-DEPENDENCY-TRACE.json`. The diagnostic tool is `tools/trace_owner_once.py`; it refuses to overwrite an existing diagnostic receipt. It launches the x86_64 Mach-O directly so macOS selects Rosetta without a protected intermediary stripping `DYLD_PRINT_*`. It sets:

```text
MONO_LOG_LEVEL=debug
MONO_LOG_MASK=dll
DYLD_PRINT_LIBRARIES=1
DYLD_PRINT_APIS=1
MONO_CRASH_NOFILE=1
```

It preserves the successful owner configuration prefix, removes `MONO_PATH`/`MONO_ENV_OPTIONS`/`MONO_AOT_CACHE`, directs temporary files to this new experiment, and disables core files.

The saved output can be reanalyzed without another owner execution:

```sh
python3 tools/analyze_trace.py
```

The resulting summary is `evidence/DEPENDENCY-TRACE-SUMMARY.json`. Full raw output is in `logs/OWNER-DEPENDENCY-TRACE.stdout.log` and `logs/OWNER-DEPENDENCY-TRACE.stderr.log`. Source and link corroboration are separate read-only receipts.

No target runtime, console, PRX, network service, or mod was executed or changed.

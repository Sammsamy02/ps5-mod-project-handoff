# Shared Win32 identity and UTC time imports

Stage `cg-common-win32-identity-time-20261013-16`, copied from isolated stage 15. Shared candidate and original DLLs remain untouched.

The five unchanged DLL samples all statically import `GetCurrentProcessId`, `GetCurrentThreadId`, and `GetSystemTimeAsFileTime`. Add exact zero, zero, and one argument catalog rows, respectively, using the existing sealed context bridge. Process ID follows host process identity. Thread ID is a stable process-wide DWORD assigned once per active host thread, shared across shim instances. File time is UTC real time in 100 ns ticks from 1601-01-01; invalid null output and clock failure stop the mod through the existing hook.

First add a failing host test. It must show catalog-only rows remain unresolved, then seal dedicated RX pages, call each through Win64, verify stable and distinct thread IDs, verify process ID and FILETIME layout/epoch against the host, and verify null pointer refusal. Run the existing targeted and loader regressions, package exact-base patch, manifest, evidence, and update the reviewer handoff.

This does not implement SRW locks, condition variables, `DisableThreadLibraryCalls`, other missing imports, loader auto-installation, or console executable-memory behavior. No claim that the unchanged DLLs execute.

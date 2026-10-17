# Stage 61: guarded VirtualProtect

Base: exact stage 60. Add one owned whole-committed-region `VirtualProtect` route for read-only/read-write transitions. The old-protection output must pass guarded guest-write validation before changing the region. Update both OS and guarded map permissions with rollback on failure. Refuse unowned, partial, uncommitted, executable, and invalid output cases. Test the Win64 thunk red then green, run focused ASan, prior heap/thread/virtual-memory suites, 15 prior runners, unchanged fixture, pinned PS5 target link, exact-base patch replay, and evidence CRC.

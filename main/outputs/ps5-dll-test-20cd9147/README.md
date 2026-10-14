# PS5 owned DLL execution test

Send `modbridge-owned-dll-test.elf` through the existing Payload Manager / ELF
loader. This is a standalone payload, with its own SDK entry point. It does not
use Ploader or a PRX configuration and does not establish gameplay integration.

The test loads two self-compiled DLLs embedded in the ELF, executes their exports
and exercises the implemented Windows module/heap/dynamic TLS APIs. It records
JSONL to `/tmp/modbridge-owned-dll-candidate.jsonl` and supported stdout.

Successful execution evidence for this build is a record with:

- `build_id` = `20cd914761004ad5855ebf73ea4e1db3`
- `execution.completed` = 1
- `execution.dll_calls_started` and `execution.dll_calls_returned` = 3
- `results` = [86, 93]
- final `cleanup.retired` = 1, with no runtime failure.

A load notification alone does not establish DLL execution. No visible gameplay
change is expected. If native memory protection refuses execute permission,
the record reports its exact native return code and zero DLL-call counts.
If the payload starts but no output is accessible, report the launcher's message;
result delivery remains a separate unknown, not an assumed successful run.

Artifact SHA-256:
`6d9b0ad773faaf77fef4ed1550b65d449f1ea3cee50daddd1329e50c9fda27e5`.
The artifact built locally and executed its DLLs on the host; PS5 execution is
unproven until a real target result is retrieved.

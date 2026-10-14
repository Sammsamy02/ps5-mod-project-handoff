# ps5debug-NG injection path: static source audit

Checked September 29, 2026 through the GitHub connector. No build, device contact, injection or source execution. Findings describe inspected source, not confirmed runtime behavior.

Repository: https://github.com/Pharaoh2k/ps5debug-NG

Inspected blobs:

- `debugger/source/proc.c`: `d04c3859d14d459b8eb0f21426f0064ce280126c`
- `debugger/source/proc_elf.c`: `80e7c4c2915dfc40e8017b204d574821e9e33011`
- `build.sh`: `327e64a201ea73d52eb26e75562e628bc17d2ee8`

These are file blob hashes, not whole-repository commit pins. Source references below use the moving master branch.

## Protocol descriptions do not establish nonexecuting load

In [proc.c](https://github.com/Pharaoh2k/ps5debug-NG/blob/master/debugger/source/proc.c), `proc_install_handle` (870–878) returns success with an RPC stub value of zero. `proc_call_handle` (881–909) sets the six integer argument registers and obtains RAX. This interface is not arbitrary floating-point/aggregate C++ ABI marshalling.

`proc_elf_handle` (916–946) reaches `SYS_PROC_ELF`; `proc_elf_rpc_handle` (949–984) reaches `SYS_PROC_ELF_RPC` and returns an entry address.

In [proc_elf.c](https://github.com/Pharaoh2k/ps5debug-NG/blob/master/debugger/source/proc_elf.c), both `sys_proc_elf` (697–700) and `sys_proc_elf_rpc` (702–705) call `sys_proc_call_remote_func` (649–694). The latter loads the ELF, optionally writes the entry address to its output, and still sets the thread instruction pointer to the loaded entry (682), sets RDI (683), applies registers (692) and detaches (693). The inspected source has no branch skipping that execution-state change for ELF_RPC.

Implication: do not design an inspection probe assuming ELF_RPC only loads a module and returns its address without executing it. This contradicts that nonexecuting interpretation of the protocol documentation. Runtime behavior needs verification on a controlled approved probe; no upstream fix was attempted.

## Limited native loader, not a Windows compatibility layer

`proc_load_elf_inject` (462–637) checks ELF magic, accepts executable/shared-object types, maps PT_LOAD segments and iterates RELA entries. The relocation loop applies numeric type 8 (`R_X86_64_RELATIVE`) and skips other types. This is not a general import-resolving plugin host or a PE/DLL loader. The remote function routine also ignores its passed payload-length argument; no robust inspection guarantees are inferred.

The release build script requires Ubuntu 22.04 and Clang/LLD 18.1.8. Its byte-sensitive build requirements cannot be assumed satisfied by this macOS workspace. No dependencies were installed or cross-build attempted.

## Consequences for universal translation research

Separate the shared translator from the mechanism used to enter a game. A working standalone payload, a native host extension, a remotely injected ELF and a translated Windows plugin are different capabilities. Validate their loading, argument passing, initialization and cleanup before connecting them to engine bindings. No source finding here establishes firmware-13.60 game integration or Stellar Blade compatibility.

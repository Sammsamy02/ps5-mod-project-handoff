# Context-bound Win64-to-SysV integer bridge

**Goal:** Remove the hidden-context ABI barrier for Windows API and CRT functions whose guest arguments and return fit the integer/pointer calling class. This is a reusable mechanism across DLLs and games, not a per-game binary edit.

**Design:** Add a 32-byte per-export trampoline that loads a fixed context pointer, a SysV target, and a shared thunk address, then jumps to the thunk. Shared assembly thunks for guest arities 0..8 preserve Win64 RSI/RDI/XMM6-15, shift integer arguments so the context is SysV argument 1, move guest arguments 6..8 to SysV stack slots, call the target with correct alignment, restore nonvolatiles, and return. A selector refuses arities above eight. Stamping writes only into caller-owned writable memory; mapping and W^X transition remain the caller's responsibility.

**Limits:** Mixed floating/integer signatures, 9..16-byte aggregates, variadic functions, SEH, semantic Windows API behavior, and console executable-memory permission remain separate gates. Do not publish shim rows or change the loader in this stage.

**Proof:** Test-first synthetic `ms_abi` callers reach SysV targets with a unique context and exact arguments for arities 0..8 through stamped RW->RX memory. Verify the 32-byte layout and unsupported-arity refusal. Re-run existing bridge and fixture checks. Package an exact-base patch and host evidence without touching Claude or the shared candidate.

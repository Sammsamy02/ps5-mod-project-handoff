# UE4SS Lua dependency portability

The actual Lua dependency pinned from UE4SS now builds for the PS5 SDK target. Six bounded host cases passed: arithmetic/table operations, coroutine yield/return, controlled error, instruction limit, time limit and allocator limit. The native ELF compiled and linked but **was not executed**. UE4SS and Lua mods remain Unsupported in the app.

The observed UE4SS snapshot is `e3ba1016562d6c0868c410d0a71e88bfcdbf691b`, with LuaRaw tree `d2b649c002c6652ec2b51ce31c060ce51455e395`, reporting Lua5.4.7. Its32 standard C units match the official Lua5.4.7 release byte-for-byte. The experiment replaced the additional Windows lock glue with an owned recursive pthread shim in scratch only; it did not substitute a different Lua release or build the full UE4SS C++ graph. Source acquisition, license notices and exact tool/source records are retained in the managed worktree.

The host embedding opened only the coroutine library, used trusted text fixtures and a256KiB logical allocator cap, and registered no OS/I/O/package/debug globals. The5ms hook and allocator observations are host measurements, not PS5 timing or total process-memory guarantees. The native build uses SDKv0.43 and verified LLVM21.1.8, including its explicit matching archiver; unresolved imports are standard libc/math/time/pthread rather than Lua symbols. The threading shim's buildability does not prove console threading/lifecycle behavior.

Unreal reflection/property layouts, object discovery, engine-owned values and allocators, ProcessEvent/game-thread hooks, C++/Unreal ABI and exact game/build behavior remain unresolved. A portable VM alone runs none of those UE4SS bindings and establishes no working Stellar Blade mod.

Detailed focused evidence is in `.superpowers/native-observations/ue4ss-lua-vm-experiment-1.md` and `.superpowers/native-dependency-spike/ue4ss-lua-vm-experiment-1/logs/` in the managed worktree. No console/game/account/FTP operation, native execution, product change, full review, sanitizer sweep or checksum package occurred in this experiment. The lighter [experiment process](ps5-mod-bridge/EXPERIMENTS.md) applies.

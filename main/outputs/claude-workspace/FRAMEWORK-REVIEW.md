# Framework Architecture Review Request

## Context
The runtime team built a game extension framework that makes the PS5 behave like a PC for mods. It's host-tested but not yet deployed to the console. We'd like your architectural review: is this well-designed? What would you improve? Should we continue on this path?

## Architecture Summary

### Component 1: Game Data Reader
- **What it does**: Before any mod reads game memory, it checks if the address is valid (mapped and accessible). Invalid reads return an error instead of crashing the game.
- **How**: A service checks the operating system's memory map for the game process, then performs the read only if safe.
- **Test coverage**: 100/100 assertions (uses the same service as the operating system's own memory manager)
- **Design decision**: We chose a "check-then-read" pattern (like Windows' structured exception handling) rather than a "read-and-recover" pattern, because the PS5 has no exception recovery in user code.

### Component 2: Pattern Finder
- **What it does**: Mods search for engine functions by their byte patterns (AOB/signature scanning), the standard technique on PC.
- **How**: Loads a decrypted copy of the game binary into our own memory space, then scans that copy. The live game address is computed by adding the per-boot memory layout offset.
- **Test coverage**: 35/35 assertions — all 4 known engine function addresses recovered exactly
- **Integration**: Plugs into the existing mod runtime's signature scanning system (no changes to mod-facing API)
- **Design decision**: Shadow-copy approach (scan our own copy, never the game's live code pages) because the console's code pages are read-protected.

### Component 3: Plugin Hooks
- **What it does**: Mods redirect engine function calls (like Detours/MinHook on PC).
- **How**: Swaps function pointers in the engine's own data structures (vtables, UFunction objects). No code modification needed.
- **Test coverage**: 40/40 + 10/10 (two separate test suites for different hook types)
- **Design decision**: Data-side function pointer swaps instead of code rewriting, because code pages are write-protected. This covers the majority of PC hook use cases.

### Component 4: Early Plugin Loading
- **What it does**: The mod framework initializes before the game engine starts.
- **How**: A 10-31 byte patch to the game's executable adds our framework as a library dependency. The console's own library loader brings it in.
- **Test coverage**: Patch applied and verified offline; the modified binary was re-parsed and confirmed correct
- **Design decision**: Library dependency injection (like PC DLL loading) rather than a separate injector process. Timing improves from ~70 seconds after game start to before engine initialization.

### Component 5: Remote Inspector
- **What it does**: Developers view the running game's state from a PC (like attaching a debugger).
- **How**: A server in the game process speaks the standard GDB remote protocol. Any debugger (GDB, LLDB) connects over TCP.
- **Test coverage**: 72/72 protocol conformance checks; verified with real LLDB session (attach, read/write memory, read registers, disassemble, detach)
- **Design decision**: GDB remote protocol (industry standard) rather than a custom protocol, so existing tools work without modification.

### Component 6: Input Bridge
- **What it does**: Multiple programs read controller and keyboard input simultaneously.
- **How**: Reads the input state directly from the operating system's input driver data. No device opening or exclusivity conflicts.
- **Test coverage**: 17/17 (current approach using shared handles); kernel-level design complete for universal access
- **Design decision**: Borrowed handle approach (share the game's already-open device) as the immediate solution; kernel-level reader as the universal solution.

### Component 7: Mod File Loader
- **What it does**: Users drop mod files in a folder, the game loads them.
- **How**: Places mod containers in the game's own content directory using the engine's native naming convention. The engine discovers and loads them automatically.
- **Design decision**: Container projection (let the engine do the work) rather than file interception (complex, error-prone). A diagnostic container was already staged once; we identified a metadata flag fix needed.

### Component 8: Texture Converter
- **What it does**: PC texture formats are automatically translated to the console's format.
- **How**: The compressed pixel data is identical between PC and console — only the container header and block ordering differ. The converter rewrites the header and reorders blocks.
- **Design decision**: Pure data permutation (no transcoding) because the underlying compression format is the same. Verified against the console's native format specification.

## Integration Points for Your App

The framework exposes these integration points that your mod manager app can use:

1. **Mod installation**: Drop files → framework loads them (Component 7)
2. **Compatibility checking**: The pattern finder can verify engine function references (Component 2)
3. **Version detection**: The game data reader can read version metadata (Component 1)
4. **Asset verification**: The texture converter can check if PC assets are convertible (Component 8)

## Questions for Your Review

1. Is the architecture sound for a production mod framework?
2. Are the test coverage levels adequate?
3. Any design decisions you'd change?
4. How should the app integrate with these components?
5. What's missing for a complete mod ecosystem?
6. Should we prioritize any component for earlier deployment?

## Source Locations (if you want to dive deeper)
- Pattern finder: `outputs/2026-10-04-zcode-team/worker-code-scan/`
- Plugin hooks: `outputs/2026-10-04-zcode-team/worker-r3-ladder/` and `worker-gate5-loadmap/`
- File loader: `outputs/2026-10-04-zcode-team/worker-file-overlay/`
- Texture converter: `outputs/2026-10-04-zcode-team/worker-asset-convert/`
- Build system: `outputs/2026-10-04-zcode-team/worker-kernel-build/`

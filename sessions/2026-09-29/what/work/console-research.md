# PS5 console prerequisites — research, 2026-09-29

## User context

The user reports a jailbroken PS5 on firmware 13.6, interpreted provisionally as 13.60. The working jailbreak chain, homebrew loader, first target game, game edition, update, and console connection details have not yet been supplied. No hardware connection or console test has been performed.

## Source findings

- [Relapse Exploit](https://github.com/ntfargo/Relapse-Exploit) lists 7.00–13.60, an ELF loader, and kernel read/write in its documented chain. These are upstream statements, not verification of the user's console or of all homebrew packages.
- [etaHEN](https://github.com/etaHEN/etaHEN) documents plugins, file transfer, an ELF loader, application integration, and homebrew services. This README does not establish that every feature or bundled component works on the user's 13.60 configuration.
- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) provides a community development route for exploited-console payloads. [websrv](https://github.com/ps5-payload-dev/websrv) is an existing payload/homebrew hosting reference. Inspect and pin appropriate SDK sources before building; do not use Sony's proprietary SDK as a dependency.
- [Pharaoh2k's ps5debug-NG](https://github.com/Pharaoh2k/ps5debug-NG) claims 13.60 support, process metadata/memory maps, remote calls, and target-process ELF loading. Its [installer source](https://github.com/Pharaoh2k/ps5debug-NG/blob/master/installer/source/main.c) contains a distinct 13.60 branch. Runtime support and stability remain hardware acceptance items. One advertised RPC-install command is expressly documented as a stub; feature discovery must distinguish that command from actual ELF injection.
- [kstuff-lite](https://github.com/EchoStretch/kstuff-lite) exists as a platform component. Its current top-level README is a technical optimization description, so it should not be used alone to infer a firmware support guarantee.

## Engineering implications

Being able to run a payload, install a homebrew title, inspect a game, and inject working native code into a game are different capabilities. The manager should record each separately. Firmware recognition by a source switch is also different from a test result on physical hardware.

A standalone console payload uses console APIs and execution rules. Injecting a PC DLL into the native PS5 game additionally requires platform loader support, ABI compatibility, correct game symbols/layouts/hooks, and any rendering/runtime dependencies. A generic Windows API shim cannot establish all of those.

## Recommended first console probe

1. User supplies the working chain/loader version and one owned installed title, identifying PS4 versus native PS5 edition.
2. Confirm a simple native payload can run and report firmware and its own process information.
3. Enumerate or explicitly select the target and capture title/build identity without modifying it.
4. Test staging, readback, rename, interruption recovery, and restoration in a project-owned scratch directory, separate from game files.
5. If runtime hooks are needed, first verify process mapping and a minimal in-target native logging probe. Treat native code injection as a distinct capability requiring an appropriate working backend.
6. Attempt one controlled game mod only after an exact-build adapter and restoration route are available. Record observable in-game behavior and return to the original state.

## Current evidence level

Documentation and source inspection only. No payload compiled here, no console service contacted, no game mod installed, and no Windows loader port established.

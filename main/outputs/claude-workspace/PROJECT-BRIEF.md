# PS5 Game Mod Manager — Project Brief

## Goal
Build a mod manager app that lets users browse, install, and manage mods for PS5 games — the same experience as PC mod managers like Vortex or MO2.

## Current State

### What's built (by the runtime team):
The game extension framework is complete and host-tested. It provides:
- A plugin loading system (mods load before the game engine starts)
- A file overlay system (drop mod files in a folder, the game loads them)
- A texture format converter (PC formats → console format, automatic)
- An input sharing system (multiple programs read controller input)
- A remote inspection tool (developers can view game state from a PC)
- A game function finder (locates engine functions for mods to use)
- A plugin hook system (mods can extend game behavior)
- A safe game data reader (mods read game data without crash risk)

All of this means: mod developers write code exactly like they do for PC.

### What's built (by you, Claude):
- The mod manager app UI (host-proven, never ran on console yet)
- The mod installer pipeline (download → prepare → install, 42/42 tests)
- The mod file format handlers (Lua mods, PAK containers, config files)
- The NexusMods integration (API client, download manager)

### What you were working on:
1. **Version matcher** — read the installed PS5 game version, match it to the right PC patch, choose the right mod file
2. **Unreal Engine name mapper** — convert PC mod references (function names, asset paths) to PS5 equivalents
3. **Content checker** — verify every asset/class/function a mod references actually exists in the PS5 build
4. **Install flow wiring** — show Compatible / Partially compatible / Incompatible with reasons

## Your code
- App: `outputs/ps5-mod-bridge/` (the web UI, installer, API)
- Your branch: `claude/townfall-item-spawner-hyj9eg`
- Test mods: `outputs/2026-10-04-zcode-team/worker-*` (various mod format tests)

## Next steps
1. Continue the version matcher and name mapper
2. Wire them into the install flow
3. The Item Spawner mod (`outputs/2026-10-04-zcode-team/worker-itemspawner-bundle/bundle/`) is the first test case
4. Think about supporting other mod engines (what would a generic "mod engine loader" look like?)

## Key game info
- Game: Silent Hill: Townfall (PPSA33286), Unreal Engine 5.6
- Version: 02.003.000
- The game runs from a local copy on the console
- Mods are Lua scripts + config files + optional asset containers

#!/usr/bin/env python3
"""add_build_markers.py <B2|B3|B4> — append that labelled build's NEW log-line
markers to markers-declared.json (overnight 2026-10-12), so verify_build.sh
proves each build's new code is really compiled into the PRX.

Idempotent (an id already present is left alone). Prints how many markers it
added and the new total; exits 1 if the manifest would end up with zero."""
import json, os, sys

M = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'markers-declared.json')
SETS = {
    'B2': [
        ('b2-mem-line', '[MEM] state=', 'utf-16le',
         'candidate/src/LuaMod.cpp ExecuteInGameThread queue-depth block (Output::send(STR(...)) wide literal)'),
        ('b2-lua-mem-note', '[lua-mem: gc_kb=', 'utf-8',
         'candidate/src/LuaMadeSimple/LuaMadeSimple.cpp call_function LUA_ERRMEM note (fmt::format narrow literal)'),
    ],
    'B3': [
        ('b3-foreachuobject', '[FOU] ForEachUObject: ', 'utf-16le',
         'candidate/src/LuaMod.cpp ForEachUObject (Output::send(STR(...)) wide literal)'),
        ('b3-uenum-foreachname', '[UEN] ForEachName: expected ', 'utf-16le',
         'candidate/src/LuaType/LuaUEnum.cpp ForEachName diagnostics (wide literal)'),
    ],
    'B4': [
        ('b4-shim-write-area-dir', '[SHIM] write-area dir ', 'utf-16le',
         'candidate/src/LuaPathShim.cpp ensure_parent_dirs_once (wide literal; trailing space separates it from "[SHIM] write-area ")'),
    ],
    'B6': [
        ('b6-mem-lua-backend', '[MEM] lua-backend=', 'utf-16le',
         'candidate/src/UE4SSProgram.cpp r9_install_lua_backing_allocator result line (wide literal, both gmalloc and libc branches)'),
        ('b6-refused-line', '[REFUSED] kind=', 'utf-8',
         'candidate/src/LuaMod.cpp refused_log::emit snprintf format (narrow literal)'),
        ('b6-rate-construct', '[B6-RATE] StaticConstructObject constructed total=', 'utf-16le',
         'candidate/src/LuaMod.cpp StaticConstructObject per-widget log rate limit (wide literal)'),
    ],
    'B6c': [
        ('b6c-guard-line', '[R9-GUARD] game-thread hook contained an exception', 'utf-16le',
         'candidate/src/LuaMod.cpp engine-called hook catch-all (wide literal)'),
        ('b6c-mem-fail-line', '[MEM] FAIL mod=', 'utf-16le',
         'candidate/src/LuaMod.cpp start_mod rebase failure line (wide literal)'),
    ],
    'B6e': [
        ('b6e-perf-line', '[PERF] win_ms=', 'utf-16le', 'candidate/src/LuaMod.cpp r9perf::emit_and_reset (wide literal)'),
        ('b6e-perf-map', '[PERF-MAP] mod=', 'utf-16le', 'candidate/src/LuaMod.cpp start_mod (wide literal)'),
        ('b6e-perf-async', '[PERF-ASYNC] mod=', 'utf-16le', 'candidate/src/LuaMod.cpp update_async (wide literal)'),
    ],
    'B6g': [
        ('b6g-index-active', '[R9-IDX] object index active: built from ', 'utf-16le', 'candidate/src/UObjectGlobals.cpp r9idx::Query first-success line (wide literal)'),
    ],
    'B6i': [
        ('b6i-notify-line', '[R9-NOTIFY] NotifyOnNewObject delivered from the object index', 'utf-16le', 'candidate/src/LuaMod.cpp r9_feed_notify_from_index (wide literal)'),
        ('b6i-api-ms', ' api_ms={:.1f}', 'utf-16le', 'candidate/src/LuaMod.cpp [PERF] per-API cost (wide literal)'),
    ],
    'B6l': [
        ('b6l-notify-stats', '[R9-NOTIFY] stats delivered=', 'utf-16le', 'candidate/src/LuaMod.cpp notify executor stats (wide literal)'),
        ('b6l-prop-perf', ' | prop calls={} cache_hits={}', 'utf-16le', 'candidate/src/LuaMod.cpp [PERF] property step costs (wide literal)'),
    ],
    'B6o': [
        ('b6o-direct-slots', 'whole array) direct-slots={}', 'utf-16le', 'candidate/src/UObjectGlobals.cpp r9idx::Query first-success line, direct slot reads flag (wide literal)'),
        ('b6o-idx-watch', ' idx_ms={:.1f} idx_watch={}', 'utf-16le', 'candidate/src/LuaMod.cpp [PERF] watched free slots (wide literal)'),
    ],
    'R10': [
        ('b7-lua-cpp-probe-cxx', '[LUA-CPP] mode=cxx-throw err_status=', 'utf-16le', 'candidate/src/LuaMod.cpp on_program_start B7 probe line'),
        ('b7-lua-cpp-probe-longjmp', '[LUA-CPP] mode=longjmp err_status=', 'utf-16le', 'candidate/src/LuaMod.cpp B7 probe line (Lua-as-C build prints this)'),
        ('b7-lua-cpp-probe-unknown', '[LUA-CPP] mode=UNKNOWN ran=', 'utf-16le', 'candidate/src/LuaMod.cpp B7 probe line (mixed result)'),
        ('b7-probe-narrow-literal', 'b7-probe-void', 'utf-8', 'candidate/src/LuaMadeSimple/LuaMadeSimple.cpp lua_cpp_probe narrow literal'),
        ('b7-h4-boundary-contained', '[B7-H4] contained a non-std exception', 'utf-16le', 'candidate/src/ScriptHookThunks.cpp game-boundary containment'),
        ('r10-name-index', '[R9-NAME] object-name index active: ', 'utf-16le', 'candidate/src/UObjectGlobals.cpp ISA track name index line'),
        ('r10-world-ev-live', '[WORLD-EV] route live: ', 'utf-16le', 'candidate/src/LuaMod.cpp HOOKS track route line'),
        ('r10-world-ev-settle', 'held={} settling={}', 'utf-16le', 'candidate/src/LuaMod.cpp HOOKS r3 stats line (settle FIFO)'),
        ('r10-fields-facts', '[FIELDS] staged layout facts: members=', 'utf-16le', 'candidate/src/UnrealInitializer.cpp FIELDS track line'),
        ('r10-pace-stats', 'paced_skips watch/match/exec=', 'utf-16le', 'candidate/src/LuaMod.cpp B6q R9Pacer notify stats'),
    ],
    'R13': [
        ('cap04-mods-started', '[EARLY] stage=mods-started t_ms={} mods={}', 'utf-16le', 'candidate/src/UE4SSProgram.cpp [EARLY-4]'),
        ('cap04-loadmap-native-off', '[EARLY] stage=loadmap-native state=off reason=ps5-no-code-detours owner=world-ev t_ms={}', 'utf-16le', 'candidate/src/LuaMod.cpp [EARLY-4]'),
        ('cap04-first-pump', '[EARLY] stage=first-pump t_ms={} src={}', 'utf-16le', 'candidate/src/LuaMod.cpp [EARLY-4]'),
        ('cap04-startup-hooks', '[EARLY] stage=startup-hooks t_ms={} loadmap_pre={} loadmap_post={} initgamestate={} beginplay={}', 'utf-16le', 'candidate/src/LuaMod.cpp [EARLY-4]'),
        ('cap04-replay-request', '[EARLY] stage=replay-request t_ms={} startup_hooks={} replay={}', 'utf-16le', 'candidate/src/LuaMod.cpp [EARLY-4]'),
        ('cap04-startup-replay', '[EARLY] stage=startup-replay t_ms={} decision={} candidates={} baseline={} excluded_new={} gamestates={} snapshot_ms={}', 'utf-16le', 'candidate/src/LuaMod.cpp [EARLY-4]'),
        ('cap04-first-map-seen', "[EARLY] stage=first-map-seen t_ms={} src={} world='{}'", 'utf-16le', 'candidate/src/LuaMod.cpp [EARLY-4]'),
    ],
    'R14': [
        ('cap05-crashtest-line', '[CAP05-CRASHTEST] deliberate crash requested by a diagnostic mod', 'utf-16le', 'candidate/src/LuaMod.cpp cap05 ModBridgeCrashTest (wide literal)'),
        ('cap05-crashtest-global', 'ModBridgeCrashTest refused: wrong or missing confirmation token', 'utf-8', 'candidate/src/LuaMod.cpp cap05 refusal text (narrow literal)'),
        ('cap02-autoprofile-shadow', '[AutoProfile] shadow parsed={} verdict={} pack={}', 'utf-16le', 'candidate/src/UE4SSProgram.cpp cap02 shadow line (wide literal)'),
    ],
    'R15': [
        ('cap09-nmod-loading', '[NMOD] loading C++ mod {} from {}/dlls (if the log stops here, the module load hung)', 'utf-16le', 'candidate/src/UE4SSProgram.cpp cap09 start_native_mods'),
        ('cap09-nmod-start-returned', '[NMOD] {}: start returned started={}', 'utf-16le', 'candidate/src/UE4SSProgram.cpp cap09 start line'),
        ('cap09-native-refused-at-start', 'Native C++ mod {} refused at start: {}', 'utf-16le', 'candidate/src/Mod/CppMod.cpp cap09 ABI-v1 refusal'),
        ('cap11-gui-imgui-assert', '[GUI] imgui assert (host) {} at {}:{}', 'utf-16le', 'candidate/src/Mod/NativeGuiEngineBackend.inc cap11 host assert line'),
    ],
    'R16': [
        ('cap07s2-pakmount-call', '[PAKMOUNT] call=%u ret=0x%llx before=%u after=%u order=%d verdict=%u', 'utf-8', 'cap07 step 2 RuntimePakMount result line (narrow snprintf)'),
        ('cap07s2-pakmount-status', '[PAKMOUNT] status=%u call=%u ret=0x%llx', 'utf-8', 'cap07 step 2 mount status line'),
        ('cap07s2-pakmount-disabled', '[PAKMOUNT] disabled: no pakmods directory', 'utf-8', 'cap07 step 2 no-op when the pakmods folder is absent'),
        ('cap07s2-pakmount-admission', '[PAKMOUNT] startup admission: proofs=%u/6 extent=0x%llx', 'utf-8', 'cap07 step 2 startup pin admission (cache open)'),
        ('cap07s2-pakmount-precall-refuse', '[PAKMOUNT] pre-call refuse: ', 'utf-8', 'cap07 step 2 refusal before any engine call'),
        ('cap07s2-pakmount-stopped', '[PAKMOUNT] stopped: ', 'utf-8', 'cap07 step 2 stop after a possible side effect'),
    ],
    'R17': [
        ('cap11fu-font-baked', '[GUI] font baked size=', 'utf-8', 'NativeGuiEngineBackend.inc / NativeGuiHost.hpp: sharp font bake (#11 follow-ups)'),
        ('cap10-stage', 'CAP10_STAGE', 'utf-8', 'cap10/native/MonoOwner.cpp emit_result'),
        ('cap10-image', 'CAP10_IMAGE', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-resolve', 'CAP10_RESOLVE', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-run', 'CAP10_RUN', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-throw', 'CAP10_THROW', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-recover', 'CAP10_RECOVER', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-done', 'CAP10_DONE', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-cookie1', 'CAP10_COOKIE1', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-cookie2', 'CAP10_COOKIE2', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-r1', 'CAP10_R1_CALLS', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-r2', 'CAP10_R2_CALLS', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-t', 'CAP10_T_CALLS', 'utf-8', 'MonoOwner.cpp'),
        ('cap10-net-refused', 'CAP10_NET_REFUSED', 'utf-8', 'cap10/native/Cap10OfflineNet.cpp'),
        ('cs-probe-requested', '[CS] C# probe requested by an enabled C# mod: starting the C# runtime', 'utf-16le', 'candidate/src/UE4SSProgram.cpp'),
        ('cs-not-started', '[CS] C# runtime not started: no enabled C# mod requested it', 'utf-16le', 'candidate/src/UE4SSProgram.cpp'),
    ],
    'R17b': [
        ('cap07s3-pakmount-source', '[PAKMOUNT] source=%s dir=%s', 'utf-8', 'NativePakMountAdapter.inl: chosen pak-mod source (configured or game-paks)'),
        ('cap07s3-pakmount-configured-hidden', '[PAKMOUNT] configured pakmods not visible: %s (errno %d) type=%d', 'utf-8', 'NativePakMountAdapter.inl: configured pakmods miss with errno'),
        ('cap07s3-pakmount-disabled-why', '[PAKMOUNT] disabled: no pakmods directory (game paks: %s)', 'utf-8', 'NativePakMountAdapter.inl: opt-out with the game-paks reason'),
        ('cap07s3-gamepaks-miss', ' project Content folder in ', 'utf-8', 'PakRequestProducer.hpp game_pak_mods_directory miss reason'),
    ],
    'R11': [
        ('r11-world-ev-pending', '[WORLD-EV] route pending: object index not usable yet', 'utf-16le', 'candidate/src/LuaMod.cpp R11-WORLD wait line'),
        ('r11-world-ev-waited', 'class-checked (tries={}, waited_ms={})', 'utf-16le', 'candidate/src/LuaMod.cpp R11-WORLD route-live line'),
    ],
    'B6p': [
        ('b6p-idx-cut', ' idx_watch={} idx_cut={}', 'utf-16le', 'candidate/src/LuaMod.cpp [PERF] budgeted watch passes (wide literal)'),
    ],
}


def main(argv):
    if len(argv) != 1 or argv[0] not in SETS:
        print('usage: add_build_markers.py <%s>' % '|'.join(SETS))
        return 2
    d = json.load(open(M))
    have = {m['id'] for m in d['markers']}
    added = 0
    for mid, text, enc, src in SETS[argv[0]]:
        if mid in have:
            continue
        d['markers'].append({'id': mid, 'fix': 'overnight 2026-10-12 labelled build ' + argv[0],
                             'canonical_marker': text.split(' ')[0],
                             'required': [{'text': text, 'encodings': [enc], 'source': src}],
                             'note': 'added by add_build_markers.py before build ' + argv[0]})
        added += 1
    if not d['markers']:
        print('FAIL: zero markers')
        return 1
    json.dump(d, open(M, 'w'), indent=1)
    print('added %d marker(s) for %s; manifest now has %d' % (added, argv[0], len(d['markers'])))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

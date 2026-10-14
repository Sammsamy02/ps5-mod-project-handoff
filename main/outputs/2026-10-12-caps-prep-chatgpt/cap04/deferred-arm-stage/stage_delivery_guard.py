#!/usr/bin/env python3
"""Exact-source, exact-once deferred LoadMap Lua-delivery guard staging.

Dry-run by default. Optional derived output stays in this prep directory;
there is no apply-to-runtime option. Source-only, uncompiled and undeployed.
"""
import sys
sys.dont_write_bytecode = True
import argparse
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SOURCE = ROOT / 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp'
SOURCE_SHA = '895a13657e69dae60835b3eea3113d6990eb29538c4b6c09e41cb132fad2b307'


def prefix(phase):
    return ('    static auto lua_load_map_' + phase + '_callback(Unreal::Hook::TCallbackIterationData<bool>& CallbackIterationData, Unreal::UEngine* Engine, Unreal::FWorldContext& WorldContext, Unreal::FURL URL, Unreal::UPendingNetGame* PendingGame, Unreal::FString& Error) -> void\n'
            '    {\n')


GUARD = '''#ifdef __PROSPERO__
        // Eager mode (0) is unchanged. Deferred native Lua delivery is admitted
        // only after BOTH registrations succeed (2). A partial registration is
        // inert while pending (1) or refused (3); WORLD-EV retains its fallback.
        // Do not synchronously unregister here: arming runs inside a callback.
        const int delivery_state = refused_log::loadmap_state.load(std::memory_order_acquire);
        if (delivery_state != 0 && delivery_state != 2)
        {
            return;
        }
#endif
'''
OLD_REFUSAL = 'the LoadMap detour stays off for this process lifetime (no re-arm); Lua RegisterLoadMapPreHook/PostHook callbacks are delivered by the data-only object-index route instead'
NEW_REFUSAL = 'native Lua LoadMap delivery stays disabled for this process lifetime (no re-arm); any partial native registration remains inert; Lua RegisterLoadMapPreHook/PostHook callbacks are delivered by the data-only object-index route instead'
REPLACEMENTS = [(prefix(phase), prefix(phase) + GUARD) for phase in ('pre', 'post')]
REPLACEMENTS.append((OLD_REFUSAL, NEW_REFUSAL))
OLD_PUBLICATION = '''            m_loadmap_deferred_arm_pending.store(true, std::memory_order_release);
            refused_log::eager_detour_block_skipped.store(true, std::memory_order_release);
            refused_log::loadmap_state.store(1, std::memory_order_release);
'''
NEW_PUBLICATION = '''            // Publish the prepared route/state before a carrier can acquire pending.
            // No startup state write may overwrite the armer's final 2/3 result.
            refused_log::loadmap_state.store(1, std::memory_order_release);
            refused_log::eager_detour_block_skipped.store(true, std::memory_order_release);
            m_loadmap_deferred_arm_pending.store(true, std::memory_order_release);
'''
REPLACEMENTS.append((OLD_PUBLICATION, NEW_PUBLICATION))


def need(ok, why):
    if not ok:
        raise ValueError(why)


def transform(raw):
    need(hashlib.sha256(raw).hexdigest() == SOURCE_SHA, 'source hash mismatch; already staged or owner source changed')
    text = raw.decode('utf-8')
    # Validate every anchor before producing any derived bytes or writing files.
    for before, after in REPLACEMENTS:
        need(text.count(before) == 1, 'replacement anchor is not exact-once')
        need(after not in text, 'replacement already present')
    for before, after in REPLACEMENTS:
        text = text.replace(before, after)
    return text.encode('utf-8')


def write_derived(path, raw):
    path = Path(path).resolve()
    need(path.is_relative_to(HERE) and path.suffix == '.cpp', 'derived output must be a .cpp inside deferred-arm-stage')
    need(not path.exists(), 'refusing to overwrite existing derived output')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as stream:
        stream.write(raw)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, default=SOURCE)
    ap.add_argument('--derived-out', type=Path)
    args = ap.parse_args()
    raw = args.source.read_bytes()
    derived = transform(raw)
    need(args.source.read_bytes() == raw, 'source changed during staging')
    if args.derived_out:
        write_derived(args.derived_out, derived)
    print(json.dumps({'label': 'source-only staged; not applied, compiled or deployed',
                      'files_checked': 1, 'items_checked': len(REPLACEMENTS),
                      'source': str(args.source.resolve()), 'source_sha256': SOURCE_SHA,
                      'derived_sha256': hashlib.sha256(derived).hexdigest(),
                      'source_unchanged': True, 'runtime_apply_supported': False,
                      'derived_output_written': bool(args.derived_out)}, indent=2))


if __name__ == '__main__':
    main()

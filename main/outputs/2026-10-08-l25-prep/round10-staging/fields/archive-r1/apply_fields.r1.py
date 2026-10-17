#!/usr/bin/env python3
"""apply_fields.py — FIELDS track (round 10) runtime-tree patch.

Usage: python3 apply_fields.py [ROOT]   (ROOT default: the real candidate tree)
Every replacement must match its anchor exactly once, else nothing is written and the script
exits non-zero naming the anchor. Touches: src/UObjectGlobals.cpp, src/UObjectArray.cpp,
src/UnrealInitializer.cpp.
"""
import os, sys

DEFAULT = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate'

GUARD = '''        // [FIELDS] explicit version gate (round 10): TUObjectArray::Chunks is the 4.8-4.10
        // indirect-array layout (FFixedUObjectArray). It does not exist on 4.11+ (5.x uses the
        // chunked FUObjectItem array, ForEachUObject_Chunked), so this walk must never read it
        // there. The callers already dispatch on Version::IsAtMost(4, 10); this makes the gate
        // local, so field_gate.py proves the read is dead on the target engine version.
        if (!Version::IsAtMost(4, 10))
        {
            Output::send<LogLevel::Error>(STR("[FIELDS] indirect-array walk refused: TUObjectArray::Chunks exists only on UE 4.8-4.10\\n"));
            return;
        }
'''

EDITS = [
    ('src/UObjectGlobals.cpp',
     'ForEachUObject_IndirectArray head',
     '''    static auto ForEachUObject_IndirectArray(const ForEachUObjectCallback& Callable) -> void
    {
        GUOBJECTARRAY_PROFILE_ITER_BEGIN()
        if (!GUObjectArray)
        {
            return;
        }
''',
     '''    static auto ForEachUObject_IndirectArray(const ForEachUObjectCallback& Callable) -> void
    {
''' + GUARD + '''        GUOBJECTARRAY_PROFILE_ITER_BEGIN()
        if (!GUObjectArray)
        {
            return;
        }
'''),
    ('src/UObjectGlobals.cpp',
     'ForEachUObject_IndirectArrayInRange head',
     '''    static auto ForEachUObject_IndirectArrayInRange(int32_t Start, int32_t End, const ForEachUObjectCallback& Callable) -> void
    {
        if (!GUObjectArray)
        {
            return;
        }
''',
     '''    static auto ForEachUObject_IndirectArrayInRange(int32_t Start, int32_t End, const ForEachUObjectCallback& Callable) -> void
    {
''' + GUARD + '''        if (!GUObjectArray)
        {
            return;
        }
'''),
    ('src/UObjectArray.cpp',
     'FUObjectItem::GetRefCountInternal forwarder',
     '''    int32_t FUObjectItem::GetRefCountInternal()
    {
        return const_cast<const FUObjectItem*>(this)->GetRefCountInternal();
    }
''',
     '''    int32_t FUObjectItem::GetRefCountInternal()
    {
        return const_cast<const FUObjectItem*>(this)->GetRefCountInternal(); // FIELD-GATE-OK: FUObjectItem::RefCount is not in the PS5 layout; GetRefCountInternal has no caller in this runtime (round-10 grep), and if one is ever added the generated accessor throws instead of reading a guessed offset
    }
'''),
    ('src/UnrealInitializer.cpp',
     'LayoutPreflight summary line',
     '''                Output::send(STR("[LayoutPreflight] checked={} startup-required={} conditional-map={} runtime-deferred={} missing-required={}\\n"),
                    joined.checks.size(),joined.required,joined.conditional,joined.deferred,joined.missing_required);
''',
     '''                Output::send(STR("[LayoutPreflight] checked={} startup-required={} conditional-map={} runtime-deferred={} missing-required={}\\n"),
                    joined.checks.size(),joined.required,joined.conditional,joined.deferred,joined.missing_required);
                // [FIELDS] round 10: proves which compiled declaration the launch staged
                // (74 member facts before the round-10 declaration edit, 81 after it).
                Output::send(STR("[FIELDS] staged layout facts: members={} vtables={}\\n"),
                    staged?staged->members.size():size_t{0},staged?staged->vtables.size():size_t{0});
'''),
]


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else DEFAULT
    texts = {}
    for rel, label, old, new in EDITS:
        p = os.path.join(root, rel)
        if p not in texts:
            if not os.path.isfile(p):
                print('apply_fields: FAIL missing file %s' % p); return 2
            texts[p] = open(p, encoding='utf-8').read()
        n = texts[p].count(old)
        if n != 1:
            print('apply_fields: FAIL anchor "%s" in %s matched %d times (need 1)' % (label, rel, n)); return 1
        texts[p] = texts[p].replace(old, new, 1)
    for p, t in texts.items():
        with open(p, 'w', encoding='utf-8') as f:
            f.write(t)
    print('apply_fields: OK %d edits in %d files under %s' % (len(EDITS), len(texts), root))
    return 0


if __name__ == '__main__':
    sys.exit(main())

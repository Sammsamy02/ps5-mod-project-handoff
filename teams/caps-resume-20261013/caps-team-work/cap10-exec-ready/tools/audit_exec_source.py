#!/usr/bin/env python3
"""Source-correlate the recorded host requests without repeating execution."""
from pathlib import Path
import hashlib
import json
import re

HERE = Path(__file__).resolve().parents[1]
TEAM = HERE.parent
HOST = TEAM / 'cap10-mono-build/runtime-work'
TARGET = TEAM / 'cap10-port-closure/target-work'
proof = json.loads((HERE / 'evidence/EXEC-HOST-CHECK.json').read_text())
assert proof['verdict'] == 'GENUINE_FINAL_OWNER_EXECUTABLE_MEMORY_OBSERVATION_COMPLETE'
events = []
for line in proof['recorded_events']:
    match = re.match(r'EXEC_EVENT index=(\d+) operation=(\w+) length=(\d+) protection=(\d+) flags=(\d+) failed=(\d+) errno=(\d+) caller_offset=(\S+) image=(\S+) symbol=(.*)', line)
    assert match, line
    index, operation, length, protection, flags, failed, error, offset, image, symbol = match.groups()
    events.append(dict(index=int(index), operation=operation, length=int(length), protection=int(protection), flags=int(flags),
                       failed=bool(int(failed)), error=int(error), caller_offset=offset, image=image, symbol=symbol))
exec_events = [event for event in events if event['protection'] & 4]
assert len(events) == 40 and len(exec_events) == 1
assert exec_events[0]['operation'] == 'mmap' and exec_events[0]['length'] == 4096 and exec_events[0]['protection'] == 6
assert exec_events[0]['symbol'] == 'mono_valloc' and exec_events[0]['caller_offset'] == '0x29f309'
assert all(not event['failed'] for event in events)
anchors = [
    ('mono/mini/driver.c', 'case MONO_AOT_MODE_INTERP:', 12, 'AOT-only + interpreter + precompiled AOT trampolines'),
    ('mono/mini/interp/interp.c', 'get_interp_to_native_trampoline (void)', 20, 'Interpreter native calls choose precompiled interp_to_native_trampoline'),
    ('mono/mini/interp/interp.c', 'mono_native_to_interp_trampoline = (MonoFuncV)mono_aot_get_trampoline', 9, 'AOT-only native-to-interpreter entry selects precompiled trampoline'),
    ('mono/mini/mini-trampolines.c', 'mono_create_ftnptr_arg_trampoline (gpointer arg, gpointer addr)', 18, 'AOT-only function-pointer arguments select AOT trampoline pool'),
    ('mono/mini/aot-runtime.c', '#ifdef MONOTOUCH\n#define USE_PAGE_TRAMPOLINES', 6, 'Mach remapped page trampolines disabled outside MONOTOUCH'),
    ('mono/mini/aot-runtime.c', 'get_numerous_trampoline (MonoAotTrampoline', 40, 'Finite precompiled corlib trampoline pool; exhaustion errors rather than synthesizing code'),
    ('mono/mini/aot-runtime.c', 'mono_aot_get_static_rgctx_trampoline (gpointer ctx, gpointer addr)', 19, 'Select precompiled code address and write arguments into GOT data'),
    ('mono/utils/mono-mmap.c', '#if defined(__APPLE__) && defined(MAP_JIT) && defined(TARGET_OSX)', 32, 'Observed Darwin one-page writable/executable probe; absent for target'),
    ('mono/utils/mono-mmap.c', 'mono_mprotect (void *addr, size_t length, int flags)', 23, 'Flags become real OS protection; EXEC would require SDK executable-memory route'),
    ('mono/utils/mono-codeman.c', '#define MONO_PROT_RWX', 3, 'General code allocator still requests RWX when reached'),
    ('mono/utils/mono-codeman.c', 'mono_code_manager_reserve_align (MonoCodeManager *cman, int size, int alignment)', 16, 'Read-only domain code manager rejects code reservation'),
    ('mono/mini/mini-runtime.c', 'if (mini_debug_options.collect_pagefault_stats)', 21, 'Optional page-fault debug path; AOT-only domain code manager becomes read-only'),
    ('mono/mini/aot-runtime.c', 'res = mono_mprotect (start, mono_pagesize (), MONO_MMAP_READ|MONO_MMAP_WRITE|MONO_MMAP_EXEC);', 6, 'Optional AOT page-fault debug handler retains executable mprotect requirement'),
]
source = []
for name, needle, count, meaning in anchors:
    host = HOST / name
    target = TARGET / name
    text = target.read_text()
    assert host.read_bytes() == target.read_bytes(), name
    position = text.index(needle)
    line = text[:position].count('\n') + 1
    source.append(dict(path=str(target), sha256=hashlib.sha256(target.read_bytes()).hexdigest(), matches_host_source=True,
                       line=line, meaning=meaning, excerpt='\n'.join(text.splitlines()[line - 1:line - 1 + count])))
host_disasm = (HERE / 'evidence/host-valloc-disassembly.txt').read_text()
target_disasm = (HERE / 'evidence/target-valloc-protect-disassembly.txt').read_text()
assert '29f2f1:' in host_disasm and '29f309:' in host_disasm and 'is_hardened_runtime' in host_disasm
assert '<mono_valloc>:' in target_disasm and '<mono_mprotect>:' in target_disasm
assert 'get_darwin_version' not in target_disasm and 'is_hardened_runtime' not in target_disasm
config = {}
for label, work in [('host', HOST), ('target', TARGET)]:
    text = (work / 'config.h').read_text()
    config[label] = {name: bool(re.search(r'^#define ' + name + r' 1$', text, re.M)) for name in ['HAVE_MMAP', 'HOST_AMD64', 'TARGET_OSX', 'DISABLE_JIT', 'ENABLE_MONOTOUCH']}
assert config['host']['TARGET_OSX'] and not config['target']['TARGET_OSX'] and config['target']['DISABLE_JIT']
result = dict(verdict='RECORDED_HOST_REQUESTS_SOURCE_AND_TARGET_OBJECT_CORRELATED',
              executable_host_events=exec_events, executable_mprotect_events=[], events=events, configuration=config, source_paths=source,
              measured_scope='Exact final signal-ready owner with pinned v2 genuine host runtime and full,interp,static corlib; one process, one owner lifecycle',
              observed_result='One Darwin capability-probe mmap with WRITE|EXEC; no other executable mmap or executable mprotect observed',
              target_inference='For this same admitted interpreter/AOT route and matching precompiled corlib, source paths select precompiled trampolines and the observed Darwin probe is compiled out on target',
              target_behavior_proven=False, executable_memory_provider_generally_unnecessary=False,
              remaining_provider_dependency='Static image text must be executable and normally relocated by the target loader. Any genuine PROT_EXEC mprotect path still needs the existing unresolved kernel_mprotect SDK hook; optional AOT page-fault debugging and other code-generating paths are not covered/admitted by this proof.',
              tracer_coverage='All mmap/mprotect imports of the actual pinned host Mono dylib; controls verified interposition, and all 40 owner-phase events have Mono caller addresses. No Mach VM allocation/protection/remap imports are present in that dylib.',
              limitations=['Not a process-wide kernel VM syscall audit: dyld mappings before trace start and libc-internal/private VM mechanisms are not observed.',
                           'No console execution, target hook behavior, firmware permissions, arbitrary managed assemblies, profiler modes, alternate AOT options or future workloads were tested.',
                           'The diagnostic generates/writes no machine code and executes no newly generated code. The genuine precompiled runtime/corlib executes normally and performs its own Darwin permission probe.'])
(HERE / 'evidence/EXEC-SOURCE-CORRELATION.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({key: result[key] for key in ['verdict', 'observed_result', 'target_inference', 'target_behavior_proven', 'remaining_provider_dependency']}, indent=2))

#!/usr/bin/env python3
"""Exact current-source cumulative bridge/observer/real-owner source stage; never apply."""
import sys
sys.dont_write_bytecode=True
import hashlib,json,difflib
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]
SOURCE=Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp')
SOURCE_SHA='44499a0aaeb617c80f9cee3ad62222cdffe9f45b4b71cc663fb25cce616150f1'
INCLUDE=b'#include <UE4SSProgram.hpp>\n'
INIT=b'    auto UE4SSProgram::init() -> void\n'
CALLBACK=b'                if (!m_shutdown_requested->load(std::memory_order_acquire) &&\n                    m_native_context.on_engine_initialized)\n                    m_native_context.on_engine_initialized();'
SCOPED=b'''                if (!m_shutdown_requested->load(std::memory_order_acquire) &&
                    m_native_context.on_engine_initialized)
                {
                    Cap10BridgeAdmissionScope admission{this, m_shutdown_requested.get()};
                    m_native_context.on_engine_initialized();
#if defined(MODBRIDGE_CAP10_ADMITTED_MONO_PORT) && MODBRIDGE_CAP10_ADMITTED_MONO_PORT == 1
                    (void)Cap10Host::run_embedded_probe_once();
#endif
                }'''
def changes():
 return [(INCLUDE,INCLUDE+b'#ifdef __PROSPERO__\n#include "Cap10InvocationObservation.hpp"\n#if defined(MODBRIDGE_CAP10_ADMITTED_MONO_PORT) && MODBRIDGE_CAP10_ADMITTED_MONO_PORT == 1\n#include "MonoOwner.hpp"\n#endif\n#include <atomic>\n#include <cstdint>\n#include <limits>\n#endif\n'),(INIT,(BASE/'native/observer_fragment.inc').read_bytes()+INIT),(CALLBACK,SCOPED)]
def transform(raw):
 if hashlib.sha256(raw).hexdigest()!=SOURCE_SHA:raise ValueError('current original hash drift/reapplication')
 for before,after in changes():
  if raw.count(before)!=1 or after in raw:raise ValueError('anchor not exact-once')
 for before,after in changes():raw=raw.replace(before,after)
 return raw
def inverse(raw):
 for before,after in reversed(changes()):
  if raw.count(after)!=1:raise ValueError('inverse anchor not exact-once')
  raw=raw.replace(after,before)
 if hashlib.sha256(raw).hexdigest()!=SOURCE_SHA:raise ValueError('inverse hash mismatch')
 return raw
def main():
 raw=SOURCE.read_bytes();new=transform(raw)
 if inverse(new)!=raw or SOURCE.read_bytes()!=raw:raise ValueError('source instability')
 (BASE/'native/current-owner.patch').write_text(''.join(difflib.unified_diff(raw.decode().splitlines(True),new.decode().splitlines(True),fromfile='original/UE4SSProgram.cpp',tofile='staged/UE4SSProgram.cpp')))
 print(json.dumps({'source_sha256':SOURCE_SHA,'derived_sha256':hashlib.sha256(new).hexdigest(),'anchors':3,'exact_inverse':True,'applied':False,'full_source_retained':False},indent=2))
if __name__=='__main__':main()

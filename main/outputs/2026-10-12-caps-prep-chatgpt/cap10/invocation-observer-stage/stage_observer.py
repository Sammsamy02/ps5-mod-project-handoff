#!/usr/bin/env python3
"""Cumulative pinned source-only bridge/observer stager. No apply operation."""
import sys
sys.dont_write_bytecode = True
import argparse, hashlib, json
from pathlib import Path
HERE=Path(__file__).resolve().parent
SOURCE=HERE.parents[3]/'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp'
SOURCE_SHA='121f2383e02e77e638dff31cd0e4b75fdd58df1715f77e7f3a1b6cacc01123da'
INCLUDE=b'#include <UE4SSProgram.hpp>\n'
INIT=b'    auto UE4SSProgram::init() -> void\n'
CALLBACK=b'                if (!m_shutdown_requested->load(std::memory_order_acquire) &&\n                    m_native_context.on_engine_initialized)\n                    m_native_context.on_engine_initialized();'
SCOPED=b'                if (!m_shutdown_requested->load(std::memory_order_acquire) &&\n                    m_native_context.on_engine_initialized)\n                {\n                    Cap10BridgeAdmissionScope admission{this, m_shutdown_requested.get()};\n                    m_native_context.on_engine_initialized();\n                }'
def need(ok,message):
    if not ok: raise ValueError(message)
def replacements():
    fragment=(HERE/'observer_fragment.inc').read_bytes()
    return [(INCLUDE, INCLUDE+b'#ifdef __PROSPERO__\n#include "Cap10InvocationObservation.hpp"\n#include <atomic>\n#include <cstdint>\n#include <limits>\n#endif\n'),(INIT,fragment+INIT),(CALLBACK,SCOPED)]
def transform(raw):
    need(hashlib.sha256(raw).hexdigest()==SOURCE_SHA,'source hash mismatch: empty/drift/reapply refused')
    changes=replacements()
    for before,after in changes:
        need(raw.count(before)==1,'anchor must be exact-once')
        need(after not in raw,'already staged')
    for before,after in changes:raw=raw.replace(before,after)
    return raw
def inverse(raw):
    for before,after in reversed(replacements()):
        need(raw.count(after)==1,'inverse anchor must be exact-once')
        raw=raw.replace(after,before)
    need(hashlib.sha256(raw).hexdigest()==SOURCE_SHA,'inverse source mismatch')
    return raw
def write_derived(path,raw):
    path=Path(path).resolve()
    need(path.is_relative_to(HERE) and path.suffix=='.cpp','output must be a new .cpp inside invocation-observer-stage')
    need(not path.exists(),'refusing overwrite')
    path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('xb') as stream:stream.write(raw)
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=SOURCE)
    parser.add_argument('--derived-out',type=Path)
    args=parser.parse_args();raw=args.source.read_bytes();derived=transform(raw)
    need(args.source.read_bytes()==raw,'source changed during staging')
    need(inverse(derived)==raw,'inverse mismatch')
    if args.derived_out:write_derived(args.derived_out,derived)
    print(json.dumps({'label':'source-only; uncompiled/unapplied/no managed execution', 'source':str(args.source.resolve()),'source_sha256':SOURCE_SHA,'derived_sha256':hashlib.sha256(derived).hexdigest(),'anchors_checked':3,'inverse_exact':True,'source_unchanged':True,'derived_output_written':bool(args.derived_out),'runtime_apply_supported':False},indent=2))
if __name__=='__main__':main()

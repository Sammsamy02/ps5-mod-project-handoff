#!/usr/bin/env python3
"""Pinned source-only bridge stager; dry-run default, no runtime apply operation."""
import sys
sys.dont_write_bytecode = True
import argparse
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[3] / 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp'
SOURCE_SHA = '121f2383e02e77e638dff31cd0e4b75fdd58df1715f77e7f3a1b6cacc01123da'
INIT = '    auto UE4SSProgram::init() -> void\n'
CALLBACK = '''                if (!m_shutdown_requested->load(std::memory_order_acquire) &&
                    m_native_context.on_engine_initialized)
                    m_native_context.on_engine_initialized();'''
SCOPED_CALLBACK = '''                if (!m_shutdown_requested->load(std::memory_order_acquire) &&
                    m_native_context.on_engine_initialized)
                {
                    Cap10BridgeAdmissionScope admission{this, m_shutdown_requested.get()};
                    m_native_context.on_engine_initialized();
                }'''
FRAGMENT = '''#ifdef __PROSPERO__
    // CAP10 source-stage: callback-thread-only admission, not a managed host.
    // Program lifetime follows the synchronous owner/init-return contract.
    // The future host must separately retain the containing native module.
    namespace
    {
        struct Cap10BridgeState
        {
            UE4SSProgram* program{};
            std::atomic_bool* shutdown{};
        };
        thread_local Cap10BridgeState cap10_bridge_state{};
        struct Cap10BridgeAdmissionScope
        {
            Cap10BridgeState previous;
            Cap10BridgeAdmissionScope(UE4SSProgram* program, std::atomic_bool* shutdown) noexcept
                : previous(cap10_bridge_state)
            {
                cap10_bridge_state = {program, shutdown};
            }
            Cap10BridgeAdmissionScope(const Cap10BridgeAdmissionScope&) = delete;
            Cap10BridgeAdmissionScope& operator=(const Cap10BridgeAdmissionScope&) = delete;
            ~Cap10BridgeAdmissionScope() noexcept { cap10_bridge_state = previous; }
        };
    }

    extern "C" __attribute__((visibility("default")))
    std::int32_t modbridge_cap10_abi_version() noexcept { return 1; }

    // reply must designate caller-owned writable int32 storage for this call.
    // A null check is not validation of an arbitrary non-null address.
    extern "C" __attribute__((visibility("default")))
    std::int32_t modbridge_cap10_runtime_probe(std::int32_t cookie, std::int32_t* reply) noexcept
    {
        if (!reply) return -4;
        try
        {
            const Cap10BridgeState state = cap10_bridge_state;
            if (!state.program || !state.shutdown ||
                state.shutdown->load(std::memory_order_acquire) ||
                !state.program->is_program_started()) return -3;
            *reply = cookie;
            return 0;
        }
        catch (...) { return -5; }
    }
#endif

'''
REPLACEMENTS = [(INIT, FRAGMENT + INIT), (CALLBACK, SCOPED_CALLBACK)]


def need(ok, message):
    if not ok: raise ValueError(message)


def transform(raw):
    need(hashlib.sha256(raw).hexdigest() == SOURCE_SHA, 'source hash mismatch; drift or already staged')
    text = raw.decode('utf-8')
    for before, after in REPLACEMENTS:
        need(text.count(before) == 1, 'replacement anchor must be exact-once')
        need(after not in text, 'replacement already present')
    for before, after in REPLACEMENTS:
        text = text.replace(before, after)
    return text.encode('utf-8')


def write_derived(path, raw):
    path = Path(path).resolve()
    need(path.is_relative_to(HERE) and path.suffix == '.cpp', 'output must be a .cpp within bridge-stage')
    need(not path.exists(), 'refusing overwrite')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as stream: stream.write(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--derived-out', type=Path)
    args = parser.parse_args()
    raw = args.source.read_bytes()
    derived = transform(raw)
    need(args.source.read_bytes() == raw, 'source changed during staging')
    if args.derived_out: write_derived(args.derived_out, derived)
    print(json.dumps({'label':'source-only; not compiled, linked, applied or managed-executed',
                      'source':str(args.source.resolve()), 'source_sha256':SOURCE_SHA,
                      'derived_sha256':hashlib.sha256(derived).hexdigest(),
                      'anchors_checked':len(REPLACEMENTS), 'source_unchanged':True,
                      'runtime_apply_supported':False, 'derived_output_written':bool(args.derived_out)}, indent=2))


if __name__ == '__main__': main()

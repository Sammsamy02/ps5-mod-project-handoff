"""Source transformation tests only: no compiled/runtime C++ execution."""
import sys
sys.dont_write_bytecode = True
import hashlib
import importlib.util
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[3] / 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp'
ORIGINAL = SOURCE.read_bytes()
STAGER = HERE / 'stage_bridge.py'


def module():
    if not STAGER.exists():
        return None
    spec = importlib.util.spec_from_file_location('cap10_stage', STAGER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class StageContract(unittest.TestCase):
    def stage(self):
        mod = module()
        self.assertIsNotNone(mod, 'exact-source bridge stager absent')
        return mod

    def test_callback_window_is_inside_original_gates(self):
        mod = module()
        text = ORIGINAL.decode() if mod is None else mod.transform(ORIGINAL).decode()
        self.assertIn('Cap10BridgeAdmissionScope admission{this, m_shutdown_requested.get()};', text)
        start = text.index('std::lock_guard<std::recursive_mutex> callback_guard(m_engine_ready_callback_mutex);', text.index('auto UE4SSProgram::init()'))
        block = text[start:text.index('            if (m_shutdown_requested->load', start)]
        self.assertEqual(block.count('m_native_context.on_engine_initialized();'), 1)
        self.assertLess(block.index('if (!m_shutdown_requested'), block.index('Cap10BridgeAdmissionScope admission'))
        self.assertLess(block.index('Cap10BridgeAdmissionScope admission'), block.index('m_native_context.on_engine_initialized();'))

    def test_native_fragment_has_real_state_and_restore_before_return(self):
        mod = self.stage()
        frag = mod.FRAGMENT
        self.assertIn('#ifdef __PROSPERO__', frag)
        self.assertIn('thread_local Cap10BridgeState cap10_bridge_state{};', frag)
        self.assertIn('previous(cap10_bridge_state)', frag)
        self.assertIn('~Cap10BridgeAdmissionScope() noexcept { cap10_bridge_state = previous; }', frag)
        self.assertIn('state.program->is_program_started()', frag)
        self.assertIn('state.shutdown->load(std::memory_order_acquire)', frag)
        self.assertNotIn('get_program(', frag)
        probe = frag[frag.index('modbridge_cap10_runtime_probe('):]
        self.assertLess(probe.index('if (!reply) return -4;'), probe.index('try'))
        self.assertLess(probe.index('return -3;'), probe.index('*reply = cookie;'))
        self.assertIn('catch (...) { return -5; }', probe)
        self.assertEqual(probe.count('*reply ='), 1)
        self.assertIn('modbridge_cap10_abi_version() noexcept', frag)
        self.assertIn('std::int32_t* reply) noexcept', frag)

    def test_exact_inverse_and_only_two_unique_replacements(self):
        mod = self.stage()
        text = mod.transform(ORIGINAL).decode()
        self.assertEqual(len(mod.REPLACEMENTS), 2)
        for before, after in reversed(mod.REPLACEMENTS):
            self.assertEqual(text.count(after), 1)
            text = text.replace(after, before)
        self.assertEqual(text.encode(), ORIGINAL)

    def test_hash_drift_empty_and_reapplication_refused(self):
        mod = self.stage()
        for raw in (b'', ORIGINAL + b'\n', mod.transform(ORIGINAL)):
            with self.subTest(bytes=len(raw)):
                with self.assertRaises(ValueError): mod.transform(raw)
        self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).digest(), hashlib.sha256(ORIGINAL).digest())

    def test_all_anchors_are_validated_before_transform(self):
        mod = self.stage()
        saved = mod.REPLACEMENTS
        try:
            mod.REPLACEMENTS = saved + [('absent anchor', 'replacement')]
            with self.assertRaisesRegex(ValueError, 'exact-once'): mod.transform(ORIGINAL)
        finally:
            mod.REPLACEMENTS = saved


if __name__ == '__main__': unittest.main(verbosity=2)

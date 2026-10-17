"""Source-only staging checks; these do not compile or execute target C++."""
import hashlib
import importlib.util
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SOURCE = ROOT / 'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp'
STAGER = HERE / 'stage_delivery_guard.py'
ORIGINAL = SOURCE.read_bytes()


def stager():
    if not STAGER.exists():
        return None
    spec = importlib.util.spec_from_file_location('delivery_stager', STAGER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def candidate():
    mod = stager()
    return ORIGINAL if mod is None else mod.transform(ORIGINAL)


def source_guard_allows(text, phase, state, prospero=True):
    """Evaluate only the tiny source predicate, not the target runtime body."""
    start = text.index('static auto lua_load_map_' + phase + '_callback(')
    preamble = text[start:text.index('        TRY([&]', start)]
    guard = re.search(r'if \(delivery_state != (\d+) && delivery_state != (\d+)\)\s*\{\s*return;', preamble)
    if not prospero or guard is None:
        return True
    return state in {int(guard[1]), int(guard[2])}


class DeliverySourceContract(unittest.TestCase):
    def test_pending_and_failed_registration_cannot_deliver_native_lua(self):
        text = candidate().decode()
        for phase in ('pre', 'post'):
            for state in (1, 3, 4):
                with self.subTest(phase=phase, state=state):
                    self.assertFalse(source_guard_allows(text, phase, state))

    def test_eager_and_fully_armed_delivery_remain_enabled(self):
        text = candidate().decode()
        for phase in ('pre', 'post'):
            for state in (0, 2):
                with self.subTest(phase=phase, state=state):
                    self.assertTrue(source_guard_allows(text, phase, state))

    def test_other_platform_body_is_unchanged_by_guard(self):
        text = candidate().decode()
        for phase in ('pre', 'post'):
            for state in (0, 1, 2, 3, 4):
                self.assertTrue(source_guard_allows(text, phase, state, prospero=False))

    def test_pending_publication_follows_state_and_route_initialization(self):
        text = candidate().decode()
        stores = re.findall(r'(m_loadmap_deferred_arm_pending|refused_log::eager_detour_block_skipped|refused_log::loadmap_state)\.store\((true|1), std::memory_order_release\);', text)
        # Explore an armer running immediately after each published pending flag.
        # Startup must have prepared both fields and must not overwrite an arm result.
        state, skipped = 0, False
        for position, (name, value) in enumerate(stores):
            if name == 'refused_log::loadmap_state':
                state = int(value)
            elif name == 'refused_log::eager_detour_block_skipped':
                skipped = True
            else:
                self.assertEqual(state, 1, 'armer can observe eager state before pending state is published')
                self.assertTrue(skipped, 'fallback route state must precede armer publication')
                for arm_result in (2, 3):
                    resumed = arm_result
                    for later_name, later_value in stores[position + 1:]:
                        if later_name == 'refused_log::loadmap_state':
                            resumed = int(later_value)
                    self.assertEqual(resumed, arm_result, 'startup can overwrite the final arm result')
        self.assertEqual(sum(name == 'm_loadmap_deferred_arm_pending' for name, _ in stores), 1)

    def test_stager_refuses_source_drift_empty_and_second_application(self):
        mod = stager()
        self.assertIsNotNone(mod, 'source-hash-gated staging implementation absent')
        for raw in (b'', ORIGINAL + b'\n', candidate()):
            with self.subTest(bytes=len(raw)):
                with self.assertRaises(ValueError):
                    mod.transform(raw)
        self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).digest(), hashlib.sha256(ORIGINAL).digest())

    def test_stage_is_only_guards_and_truthful_refusal_text(self):
        mod = stager()
        self.assertIsNotNone(mod, 'source-hash-gated staging implementation absent')
        derived = mod.transform(ORIGINAL).decode()
        self.assertEqual(derived.count('const int delivery_state = refused_log::loadmap_state.load'), 2)
        self.assertNotIn('Unreal::Hook::UnregisterCallback(', derived)
        self.assertIn('any partial native registration remains inert', derived)
        self.assertNotIn('the LoadMap detour stays off for this process lifetime', derived)
        self.assertIn('if (is_map && refused_log::loadmap_state.load(std::memory_order_acquire) == 2) { continue; }', derived)
        restored = derived
        for before, after in reversed(mod.REPLACEMENTS):
            self.assertEqual(restored.count(after), 1)
            restored = restored.replace(after, before)
        self.assertEqual(restored.encode(), ORIGINAL)


if __name__ == '__main__':
    unittest.main()

"""Tests for the SeedTable.h emitter (tools/profilegen/emit_seedtable.py) and the comparison tool (diff_seedtable.py).

Synthetic tests (always run) build small profile documents with arbitrary numbers and check the header's format,
slot order and kinds, the escaping of derivation strings (also by compiling and running them), the GMalloc choice,
the heap-span options, absent slots, refusals, the CLI flag, and that no verification marker is ever written.
The Townfall tests regenerate the profile from the retained executable (paths from MODBRIDGE_TOWNFALL_EBOOT /
MODBRIDGE_TOWNFALL_PARAM, the runtime's current header from MODBRIDGE_CURRENT_SEEDTABLE) and skip cleanly when a file is absent.
Compile checks (`c++ -std=c++20 -fsyntax-only`) skip when no C++ compiler is installed.
"""
import copy
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT))
sys.path.insert(0, str(PROJECT / "tests" / "fixtures" / "profilegen"))

from synth import Asm, Seg, build_elf, gnu_note  # noqa: E402,F401

from tools.profilegen import cli, detectors, diff_seedtable, emit_seedtable  # noqa: E402
from tools.profilegen.detectors import platform  # noqa: E402
from tools.profilegen.diff_seedtable import Parsed  # noqa: E402
from tools.profilegen.emit_seedtable import SeedOptions, SeedTableRefused  # noqa: E402
from tools.profilegen.index import Decoder  # noqa: E402
from tools.profilegen.model import HIGH, LOW, MEDIUM, Hex  # noqa: E402

TOWNFALL_EBOOT = os.environ.get(
    "MODBRIDGE_TOWNFALL_EBOOT",
    "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/"
    "townfall-static-executable-2026-10-02/decrypted-eboot.bin")
TOWNFALL_PARAM = os.environ.get(
    "MODBRIDGE_TOWNFALL_PARAM",
    "/WORKSPACE/ps5-claude/outputs/2026-10-04-zcode-team/worker-game-version/param.json")
CURRENT_SEEDTABLE = os.environ.get(
    "MODBRIDGE_CURRENT_SEEDTABLE",
    "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-combined-menu-package-v2/lineage/SeedTable.h")

CXX = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
needs_cxx = unittest.skipUnless(CXX, "no C++ compiler on this machine")
MARKER = re.compile(r"console[-_ ]?verified|verification", re.IGNORECASE)


# ---------------------------------------------------------------------------------------------
def env(value, confidence=HIGH, method="test.method", evidence=None):
    return {"value": value, "confidence": confidence, "method": method, "evidence": list(evidence or ["fact"])}


def sym(label, confidence=HIGH, method="test.method", evidence=None, **extra):
    node = {"label": env(label, confidence, method, evidence), "runtime": env(label, MEDIUM, "label + load.base")}
    node.update(extra)
    return node


def sample_profile(**over):
    """A profile document with arbitrary numbers (not any game's): four slots, a pool, a base and an executable hash."""
    doc = {
        "schema": "modbridge.runtime-profile/1",
        "profile_id": "PPSA00001/01.000.000/deadbeef",
        "status": "generated",
        "profile_digest": "ab" * 32,
        "verification": None,
        "binding": {"executable": {"sha256": env("5a" * 32), "bytes": env("0x1000")}},
        "load": {"base": env("0x200000", MEDIUM, "load.platform_convention", ["platform convention"])},
        "build": {"allocator_family": env("FMallocBinned2", MEDIUM, "gmalloc.family_literals")},
        "elf": {"segments": {
            "ph0": env({"type": "LOAD", "p_vaddr": "0x0", "p_filesz": "0x4000", "p_memsz": "0x4000"}),
            "ph1": env({"type": "LOAD", "p_vaddr": "0x5000", "p_filesz": "0x1000", "p_memsz": "0x3000"}),
            "ph2": env({"type": "GNU_RELRO", "p_vaddr": "0x5000", "p_filesz": "0x800", "p_memsz": "0x800"}),
        }},
        "symbols": {
            "object_array": sym("0x6100", HIGH, "uobject_array.member_function_cluster", ["4 of 7 functions take its address"]),
            "fname_ctor_wchar": sym("0x2340", HIGH, "fname_ctor.module_name_anchors", ["5 of 5 anchors agree"],
                                    prologue_32=env("554889e5" * 8, HIGH, "bytes")),
            "name_pool": sym("0x7200", HIGH, "fname_pool.init_callers"),
            "gmalloc": sym("0x6a00", HIGH, "gmalloc.dispatch_shape", ["Malloc and Free load it"]),
            "gmalloc_base_slot": sym("0x6a40", MEDIUM, "gmalloc.lazy_create_accessor", ["raw allocator pointer"]),
            "gmalloc_instance": sym("0x6b00", MEDIUM, "gmalloc.lazy_create_accessor"),
            "static_construct_object_internal": sym("0x3450", HIGH, "static_construct.new_object_pair", ["dominant pair"],
                                                    prologue_16=env("55" * 16, HIGH, "bytes")),
        },
    }
    for key, value in over.items():
        doc[key] = value
    return doc


def render(doc, gmalloc=None, heap=None):
    text, model = emit_seedtable.render_profile(doc, SeedOptions(gmalloc, heap))
    return text, model


def syntax_check(path):
    return subprocess.run([CXX, "-std=c++20", "-fsyntax-only", "-x", "c++", str(path)], capture_output=True, text=True)


# ---------------------------------------------------------------------------------------------
class FormatTests(unittest.TestCase):
    def test_golden_text_with_derivations_masked(self):
        text, model = render(sample_profile())
        masked = re.sub(r'(\{ (?:true|false), (?:true|false), 0x[0-9a-f]+, "[a-z_]+", )"(?:[^"\\]|\\.)*"( \};)', r'\1"<D>"\2', text)
        got = [l for l in masked.split("\n") if not l.lstrip().startswith("//")]
        self.assertEqual(got, [
            '#ifndef RC_SEED_TABLE_H_GENERATED_%x_' % (0x200000 ^ 0x7200),
            '#define RC_SEED_TABLE_H_GENERATED_%x_' % (0x200000 ^ 0x7200),
            '#include <cstdint>',
            '#include <string_view>',
            '',
            'namespace RC::Unreal::UnrealInitializer::SeedTable',
            '{',
            '    inline constexpr bool kGenerated = true;',
            '    inline constexpr uint64_t kBaseObserved = 0x200000;',
            '    inline constexpr uint64_t kFNamePoolLabel = 0x7200;',
            '    inline constexpr uint64_t kHeapLo = 0x0;',
            '    inline constexpr uint64_t kHeapHi = 0xffffffffffffffff;',
            '    inline constexpr uint64_t kGMallocInstanceLabel = 0x6b00; // FMallocBinned2 global instance (phdr1 BSS)',
            '    struct SlotSeed',
            '    {',
            '        bool present;',
            '        bool data_kind;',
            '        uint64_t label;',
            '        const char* name;',
            '        const char* derivation;',
            '    };',
            '    inline constexpr SlotSeed kGUObjectArray{ true, true, 0x6100, "g_uobject_array", "<D>" }; // slot 0 (data)',
            '    inline constexpr SlotSeed kFNameCtorWchar{ true, false, 0x2340, "fname_ctor_wchar", "<D>" }; // slot 2 (code)',
            '    inline constexpr SlotSeed kGMalloc{ true, true, 0x6a00, "gmalloc", "<D>" }; // slot 3 (data)',
            '    inline constexpr SlotSeed kStaticConstructObject{ true, false, 0x3450, "static_construct_object_internal", "<D>" }; // slot 4 (code)',
            '    inline constexpr bool kRequiredSetComplete = true;',
            '    inline constexpr std::string_view kImageSha256 = "%s";' % ("5a" * 32),
            '}',
            '#endif',
            '',
        ])
        self.assertTrue(text.endswith("#endif\n"))
        self.assertNotIn("\r", text)

    def test_header_comments_name_the_producer_and_the_contract(self):
        text, _m = render(sample_profile())
        lines = text.split("\n")
        self.assertTrue(lines[0].startswith("// SeedTable.h — GENERATED by tools/profilegen"))
        self.assertIn("SEED-TABLE-FORMAT.md v2", lines[1])
        self.assertIn("PPSA00001/01.000.000/deadbeef", lines[2])

    def test_order_slot_ids_and_kinds_follow_the_format(self):
        text, _m = render(sample_profile())
        p = Parsed(text)
        self.assertEqual(p.order, ["kGenerated", "kBaseObserved", "kFNamePoolLabel", "kHeapLo", "kHeapHi", "kGMallocInstanceLabel",
                                   "SlotSeed", "kGUObjectArray", "kFNameCtorWchar", "kGMalloc", "kStaticConstructObject",
                                   "kRequiredSetComplete", "kImageSha256"])
        self.assertEqual([(c, s["slot"], s["kind"], s["name"], s["data_kind"]) for c, s in p.slots.items()], [
            ("kGUObjectArray", 0, "data", "g_uobject_array", True),
            ("kFNameCtorWchar", 2, "code", "fname_ctor_wchar", False),
            ("kGMalloc", 3, "data", "gmalloc", True),
            ("kStaticConstructObject", 4, "code", "static_construct_object_internal", False)])
        self.assertEqual(p.struct_fields, ["bool present", "bool data_kind", "uint64_t label", "const char* name", "const char* derivation"])
        self.assertEqual(p.namespace, "RC::Unreal::UnrealInitializer::SeedTable")
        self.assertEqual(p.includes, ["<cstdint>", "<string_view>"])
        self.assertEqual(p.guard, p.define)

    def test_guard_is_base_xor_pool_label(self):
        doc = sample_profile()
        doc["load"]["base"] = env("0x300000", MEDIUM, "load.cli_override")
        text, _m = render(doc)
        self.assertIn("RC_SEED_TABLE_H_GENERATED_%x_" % (0x300000 ^ 0x7200), text)

    def test_derivation_is_compact_json_built_from_method_and_evidence(self):
        text, _m = render(sample_profile())
        p = Parsed(text)
        d = p.slots["kFNameCtorWchar"]["derivation"]
        self.assertEqual(list(d), ["anchor_bytes", "rip_arithmetic", "uniqueness", "method", "confidence", "source"])
        self.assertEqual(d["anchor_bytes"], "554889e5" * 8)                    # the profile's prologue bytes
        self.assertEqual(d["method"], "fname_ctor.module_name_anchors")
        self.assertEqual(d["confidence"], "high")
        self.assertEqual(d["uniqueness"], "5 of 5 anchors agree")
        self.assertIn("runtime 0x202340 = linked 0x2340 + base 0x200000", d["rip_arithmetic"])
        self.assertEqual(d["source"], "symbols.fname_ctor_wchar.label")
        self.assertNotIn(", ", p.slots["kGUObjectArray"]["derivation_text"].split('"anchor_bytes"')[0])    # compact separators
        data = p.slots["kGUObjectArray"]["derivation"]
        self.assertTrue(data["anchor_bytes"].startswith("data symbol"))       # no instruction bytes for a data slot

    def test_support_constant_comment_names_family_and_segment(self):
        doc = sample_profile()
        doc["symbols"]["gmalloc_vtable"] = sym("0x5400", MEDIUM)
        text, _m = render(doc)
        p = Parsed(text)
        self.assertEqual(p.consts["kGMallocInstanceLabel"]["comment"], "FMallocBinned2 global instance (phdr1 BSS)")
        self.assertEqual(p.consts["kGMallocVtableLabel"]["comment"], "FMallocBinned2 vtable (phdr1 RELRO)")
        # the two lines are aligned like the runtime's current header
        lines = [l for l in text.split("\n") if l.lstrip().startswith("inline constexpr uint64_t kGMalloc") and "//" in l]
        self.assertEqual(len(lines), 2)
        self.assertEqual(len({l.index("//") for l in lines}), 1)

    def test_missing_support_constants_are_left_out_with_a_comment(self):
        doc = sample_profile()
        del doc["symbols"]["gmalloc_instance"]
        text, model = render(doc)
        p = Parsed(text)
        self.assertNotIn("kGMallocInstanceLabel", p.consts)
        self.assertNotIn("kGMallocVtableLabel", p.consts)
        self.assertIn("kGMallocInstanceLabel is not emitted", text)
        self.assertIn("kGMallocVtableLabel is not emitted", text)
        self.assertTrue(any("kGMallocVtableLabel not emitted" in n for n in model.notes))


class EscapingTests(unittest.TestCase):
    NASTY = ['quote " here', "back\\slash and \\n literal", "tab\tnewline\nend", "café — dash", "100% ??/ %s {0}", "'single'"]

    def doc(self):
        doc = sample_profile()
        doc["symbols"]["object_array"]["label"]["evidence"] = list(self.NASTY)
        return doc

    def test_c_literal_body(self):
        self.assertEqual(emit_seedtable.c_literal_body('a"b\\c'), 'a\\"b\\\\c')

    def test_round_trip_through_the_parser(self):
        text, _m = render(self.doc())
        p = Parsed(text)
        d = p.slots["kGUObjectArray"]["derivation"]
        self.assertEqual(d["uniqueness"], "; ".join(self.NASTY))
        # the whole literal decodes back to the compact JSON of the same object
        self.assertEqual(p.slots["kGUObjectArray"]["derivation_text"], json.dumps(d, separators=(",", ":")))
        for line in text.split("\n"):
            self.assertNotIn("\t", line)
            self.assertTrue(all(ord(c) < 128 for c in line) or "—" in line)    # only the em dash comments are non-ASCII

    @needs_cxx
    def test_compiled_string_is_exactly_the_json_text(self):
        with tempfile.TemporaryDirectory() as tmp:
            header = Path(tmp, "SeedTable.h")
            text, _m = render(self.doc())
            header.write_text(text, encoding="utf-8")
            Path(tmp, "main.cpp").write_text(
                '#include "SeedTable.h"\n#include <cstdio>\n'
                'int main() { using namespace RC::Unreal::UnrealInitializer::SeedTable;\n'
                '  std::fputs(kGUObjectArray.derivation, stdout); std::fputc(0x0a, stdout);\n'
                '  std::fputs(kStaticConstructObject.derivation, stdout); return 0; }\n')
            exe = Path(tmp, "probe")
            build = subprocess.run([CXX, "-std=c++20", "-I", tmp, str(Path(tmp, "main.cpp")), "-o", str(exe)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            out = subprocess.run([str(exe)], capture_output=True, text=True, encoding="utf-8").stdout.split("\n")
            p = Parsed(text)
            self.assertEqual(out[0], p.slots["kGUObjectArray"]["derivation_text"])
            self.assertEqual(json.loads(out[0])["uniqueness"], "; ".join(self.NASTY))
            self.assertEqual(out[1], p.slots["kStaticConstructObject"]["derivation_text"])


@needs_cxx
class CompileTests(unittest.TestCase):
    def check(self, text, name):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp, name)
            path.write_text(text, encoding="utf-8")
            r = syntax_check(path)
            self.assertEqual(r.returncode, 0, "%s does not compile as C++20:\n%s" % (name, r.stderr))

    def test_complete_table(self):
        self.check(render(sample_profile())[0], "complete.h")

    def test_base_slot_variant_and_heap_variants(self):
        self.check(render(sample_profile(), "base_slot")[0], "base_slot.h")
        self.check(render(sample_profile(), None, "omit")[0], "omit.h")
        self.check(render(sample_profile(), None, "0x1000:0x2000")[0], "span.h")

    def test_table_with_absent_slots_and_no_support_constants(self):
        doc = sample_profile()
        doc["symbols"]["gmalloc"]["label"]["confidence"] = LOW
        del doc["symbols"]["gmalloc_instance"]
        del doc["symbols"]["object_array"]
        text, model = render(doc)
        self.assertFalse(model.complete)
        self.check(text, "partial.h")

    def test_header_constants_are_usable_in_constant_expressions(self):
        text, _m = render(sample_profile())
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp, "SeedTable.h")
            path.write_text(text, encoding="utf-8")
            src = Path(tmp, "use.cpp")
            src.write_text('#include "SeedTable.h"\nusing namespace RC::Unreal::UnrealInitializer::SeedTable;\n'
                           'static_assert(kGenerated && kRequiredSetComplete);\n'
                           'static_assert(kGMalloc.present && kGMalloc.data_kind && kGMalloc.label == 0x6a00);\n'
                           'static_assert(!kFNameCtorWchar.data_kind && kStaticConstructObject.label == 0x3450);\n'
                           'static_assert(kBaseObserved + kGUObjectArray.label == 0x206100);\n'
                           'static_assert(kImageSha256.size() == 64);\n')
            r = subprocess.run([CXX, "-std=c++20", "-fsyntax-only", "-I", tmp, str(src)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)


class GMallocChoiceTests(unittest.TestCase):
    def test_default_is_the_generators_value_and_the_header_says_so(self):
        text, model = render(sample_profile())
        p = Parsed(text)
        self.assertEqual(p.slots["kGMalloc"]["label"], 0x6a00)
        self.assertEqual(p.slots["kGMalloc"]["derivation"]["source"], "symbols.gmalloc.label")
        self.assertIn("--seed-gmalloc dispatch (the default: the generator's own value) -> 0x6a00", text)
        self.assertIn("Not seeded: symbols.gmalloc_base_slot = 0x6a40", text)
        self.assertIn("--seed-gmalloc base_slot", text)
        self.assertTrue(any(n.startswith("gmalloc:") and "NOT seeded" in n and "0x6a40" in n for n in model.notes))

    def test_explicit_base_slot(self):
        text, model = render(sample_profile(), gmalloc="base_slot")
        p = Parsed(text)
        self.assertEqual(p.slots["kGMalloc"]["label"], 0x6a40)
        self.assertEqual(p.slots["kGMalloc"]["derivation"]["source"], "symbols.gmalloc_base_slot.label")
        self.assertEqual(p.slots["kGMalloc"]["derivation"]["method"], "gmalloc.lazy_create_accessor")
        self.assertIn("--seed-gmalloc base_slot -> 0x6a40", text)
        self.assertIn("Not seeded: symbols.gmalloc = 0x6a00", text)
        self.assertNotIn("(the default", text)

    def test_explicit_dispatch_equals_the_default_apart_from_the_wording(self):
        a = Parsed(render(sample_profile())[0]).slots["kGMalloc"]
        b = Parsed(render(sample_profile(), gmalloc="dispatch")[0]).slots["kGMalloc"]
        self.assertEqual((a["label"], a["derivation"]), (b["label"], b["derivation"]))

    def test_the_two_choices_change_only_the_gmalloc_slot_and_its_comments(self):
        a, b = Parsed(render(sample_profile())[0]), Parsed(render(sample_profile(), gmalloc="base_slot")[0])
        for name in a.slots:
            if name != "kGMalloc":
                self.assertEqual(a.slots[name], b.slots[name])
        self.assertEqual(a.consts, b.consts)

    def test_both_variables_agreeing_is_stated(self):
        doc = sample_profile()
        doc["symbols"]["gmalloc_base_slot"] = sym("0x6a00", MEDIUM)
        text, model = render(doc)
        self.assertIn("name the same variable", text)
        self.assertNotIn("Not seeded", text)

    def test_no_silent_fallback_when_the_default_variable_is_missing(self):
        doc = sample_profile()
        del doc["symbols"]["gmalloc"]
        text, model = render(doc)
        p = Parsed(text)
        self.assertFalse(p.slots["kGMalloc"]["present"])
        self.assertEqual(p.slots["kGMalloc"]["label"], 0)
        self.assertEqual(p.slots["kGMalloc"]["derivation_text"], "")
        self.assertIn("is NOT used in its place", text)
        self.assertFalse(model.complete)
        self.assertIn("kRequiredSetComplete = false", text)
        self.assertNotIn("0x6a40", text.split("struct SlotSeed")[1].split("kStaticConstructObject")[0].replace("Not seeded", ""))

    def test_explicit_choice_that_the_profile_cannot_honour_is_refused(self):
        doc = sample_profile()
        del doc["symbols"]["gmalloc_base_slot"]
        with self.assertRaises(SeedTableRefused) as cm:
            render(doc, gmalloc="base_slot")
        self.assertIn("does not substitute", str(cm.exception))

    def test_bad_choice_is_refused(self):
        with self.assertRaises(SeedTableRefused):
            SeedOptions("effective")

    def test_v1_note_appears_only_for_the_dispatch_choice(self):
        self.assertIn("V1 may not hold", render(sample_profile())[0])
        self.assertNotIn("V1 may not hold", render(sample_profile(), gmalloc="base_slot")[0])


class HeapSpanTests(unittest.TestCase):
    def test_default_is_an_unbounded_range_with_a_comment(self):
        text, model = render(sample_profile())
        p = Parsed(text)
        self.assertEqual((p.consts["kHeapLo"]["number"], p.consts["kHeapHi"]["number"]), (0, 0xFFFFFFFFFFFFFFFF))
        self.assertIn("RETIRED", text)
        self.assertIn("UNBOUNDED", text)
        self.assertTrue(any("unbounded" in n for n in model.notes))

    def test_omit_drops_both_constants(self):
        text, _m = render(sample_profile(), heap="omit")
        p = Parsed(text)
        self.assertNotIn("kHeapLo", p.consts)
        self.assertNotIn("kHeapHi", p.consts)
        self.assertIn("not emitted (--seed-heap-span omit)", text)

    def test_explicit_span(self):
        text, _m = render(sample_profile(), heap="0x1000:0x9000")
        p = Parsed(text)
        self.assertEqual((p.consts["kHeapLo"]["number"], p.consts["kHeapHi"]["number"]), (0x1000, 0x9000))
        self.assertIn("supplied on the command line", text)

    def test_bad_spans_are_refused(self):
        for bad in ("lo:hi", "0x2000:0x1000", "0x1000", "1000:2000", ""):
            with self.assertRaises(SeedTableRefused, msg=bad):
                SeedOptions(None, bad)


class AbsentSlotAndRefusalTests(unittest.TestCase):
    def test_low_confidence_label_is_not_seeded(self):
        doc = sample_profile()
        doc["symbols"]["fname_ctor_wchar"]["label"]["confidence"] = LOW
        text, model = render(doc)
        p = Parsed(text)
        s = p.slots["kFNameCtorWchar"]
        self.assertEqual((s["present"], s["label"], s["derivation_text"]), (False, 0, ""))
        self.assertEqual((s["slot"], s["kind"], s["name"]), (2, "code", "fname_ctor_wchar"))
        self.assertIn("slot 2 (fname_ctor_wchar) is absent: symbols.fname_ctor_wchar.label is only low confidence", text)
        self.assertFalse(p.consts["kRequiredSetComplete"]["text"] == "true")
        self.assertFalse(model.complete)
        self.assertEqual([s.name for s in model.slots if not s.present], ["fname_ctor_wchar"])

    def test_unresolved_symbol_is_absent_with_a_reason(self):
        doc = sample_profile()
        doc["symbols"]["object_array"]["label"] = env(None, LOW)
        text, _m = render(doc)
        self.assertIn("has no value", text)
        self.assertFalse(Parsed(text).slots["kGUObjectArray"]["present"])

    def test_every_slot_absent_still_renders_a_valid_header(self):
        doc = sample_profile()
        doc["symbols"] = {"name_pool": doc["symbols"]["name_pool"]}
        text, model = render(doc)
        p = Parsed(text)
        self.assertTrue(all(not s["present"] for s in p.slots.values()))
        self.assertEqual(p.consts["kRequiredSetComplete"]["text"], "false")

    def test_refuses_without_the_base_the_pool_or_the_hash(self):
        for mutate, word in ((lambda d: d["load"].pop("base"), "load.base"),
                             (lambda d: d["symbols"].pop("name_pool"), "name_pool"),
                             (lambda d: d["binding"]["executable"].pop("sha256"), "sha256"),
                             (lambda d: d["symbols"]["name_pool"]["label"].update(confidence=LOW), "name_pool")):
            doc = sample_profile()
            mutate(doc)
            with self.assertRaises(SeedTableRefused) as cm:
                render(doc)
            self.assertIn(word, str(cm.exception))

    def test_a_non_sha256_hash_is_refused(self):
        doc = sample_profile()
        doc["binding"]["executable"]["sha256"] = env("not-a-hash")
        with self.assertRaises(SeedTableRefused):
            render(doc)

    def test_does_not_modify_the_profile(self):
        doc = sample_profile()
        before = copy.deepcopy(doc)
        render(doc)
        render(doc, gmalloc="base_slot")
        self.assertEqual(doc, before)


class HostileProfileTextTests(unittest.TestCase):
    """Text that comes from param.json or the executable ends up in comments and string literals: it must not become code."""

    def hostile(self):
        doc = sample_profile(profile_id="PPSA0\nint injected = 1; //\\")
        doc["build"]["allocator_family"] = env("FMalloc\nint also_injected;\\", MEDIUM, "m")
        doc["symbols"]["object_array"]["label"]["evidence"] = ["line\nbreak \\", "int third_injected;"]
        return doc

    def test_newlines_and_trailing_backslashes_cannot_end_a_comment_early(self):
        text, _m = render(self.hostile())
        p = Parsed(text)
        self.assertEqual(p.unrecognised, [])
        self.assertNotIn("\nint injected", text)
        self.assertNotIn("\nint also_injected", text)
        self.assertFalse(any(l.endswith("\\") for l in text.split("\n") if l.lstrip().startswith("//")))
        self.assertEqual(p.slots["kGUObjectArray"]["derivation"]["uniqueness"], "line\nbreak \\; int third_injected;")

    @needs_cxx
    def test_hostile_text_still_compiles_and_declares_nothing_extra(self):
        text, _m = render(self.hostile())
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp, "SeedTable.h")
            path.write_text(text, encoding="utf-8")
            src = Path(tmp, "use.cpp")
            src.write_text('#include "SeedTable.h"\nint main() { return 0; }\n')
            r = subprocess.run([CXX, "-std=c++20", "-fsyntax-only", "-I", tmp, str(src)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            probe = Path(tmp, "probe.cpp")
            probe.write_text('#include "SeedTable.h"\nint injected = 2;\nint also_injected = 3;\nint third_injected = 4;\n')
            r = subprocess.run([CXX, "-std=c++20", "-fsyntax-only", "-I", tmp, str(probe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, "a hostile string declared a name in the header:\n" + r.stderr)

    def test_values_that_are_not_numbers_are_refused_not_a_traceback(self):
        for bad in ("not hex", -5, 1 << 70, True, [1], "-0x10"):
            doc = sample_profile()
            doc["symbols"]["static_construct_object_internal"]["label"]["value"] = bad
            with self.assertRaises(SeedTableRefused, msg=repr(bad)):
                render(doc)


class NoVerificationMarkerTests(unittest.TestCase):
    def test_a_verified_profile_does_not_leak_into_the_header(self):
        doc = sample_profile(status="console-verified",
                             verification={"receipt_id": "r1", "result": "pass", "verified": ["symbols.name_pool"]})
        text, _m = render(doc)
        self.assertIsNone(MARKER.search(text), MARKER.search(text) and MARKER.search(text).group(0))
        self.assertNotIn("receipt", text)
        self.assertEqual(text, render(sample_profile())[0])        # same text as for the generated profile

    def test_the_emitter_never_writes_the_words_in_any_variant(self):
        for kw in ({}, {"gmalloc": "base_slot"}, {"heap": "omit"}, {"heap": "0x1000:0x2000"}):
            text, model = render(sample_profile(), **kw)
            self.assertIsNone(MARKER.search(text), kw)
            self.assertIsNone(MARKER.search("\n".join(model.notes)), kw)

    def test_text_from_the_profile_that_would_carry_a_marker_is_refused(self):
        for path in ("evidence", "method"):
            doc = sample_profile()
            if path == "evidence":
                doc["symbols"]["object_array"]["label"]["evidence"] = ["this value is console-verified on the PS5"]
            else:
                doc["symbols"]["object_array"]["label"]["method"] = "console_verified.receipt"
            with self.assertRaises(SeedTableRefused) as cm:
                render(doc)
            self.assertIn("verification marker", str(cm.exception))
        doc = sample_profile()
        doc["symbols"]["gmalloc"]["label"]["evidence"] = ["see the verification block"]
        with self.assertRaises(SeedTableRefused):
            render(doc)

    def test_offline_re_verification_wording_is_not_a_console_claim(self):
        doc = sample_profile()
        doc["symbols"]["object_array"]["label"]["evidence"] = ["9 call sites, 9 re-verified as E8 rel32"]
        text, _m = render(doc)
        self.assertIn("re-verified as E8 rel32", text)

    def test_the_emitter_source_never_assigns_a_status_or_verification(self):
        src = (PROJECT / "tools" / "profilegen" / "emit_seedtable.py").read_text(encoding="utf-8")
        self.assertNotRegex(src, r"\[\s*[\"']verification[\"']\s*\]\s*=")
        self.assertNotRegex(src, r"[\"']status[\"']\s*:\s*[\"']console")


# ---------------------------------------------------------------------------------------------
def tiny_image():
    a = Asm(0x1000)
    a.int3(1).align(16)
    a.label("f")
    a.prologue().epilogue()
    code = a.finish()
    return build_elf([Seg(0x1000, code, 5), Seg(0x6000, bytes(32), 4)], notes=[gnu_note(bytes(range(16)) + bytes(4))])


def stub_detectors(slots=True, pool=True, vtable=False):
    """Detector list for the CLI tests: the platform detector plus one stub that writes the facts a header needs."""
    def stub(ctx):
        if pool:
            ctx.put("symbols.name_pool.label", Hex(0x7200), HIGH, "stub.pool", ["pool"])
        if slots:
            ctx.put("symbols.object_array.label", Hex(0x6100), HIGH, "stub.uoa", ["uoa"])
            ctx.put("symbols.fname_ctor_wchar.label", Hex(0x2340), HIGH, "stub.ctor", ["ctor"])
            ctx.put("symbols.fname_ctor_wchar.prologue_32", "55" * 32, HIGH, "bytes", ["bytes"])
            ctx.put("symbols.gmalloc.label", Hex(0x6a00), HIGH, "stub.gm", ["dispatch"])
            ctx.put("symbols.gmalloc_base_slot.label", Hex(0x6a40), MEDIUM, "stub.gb", ["base"])
            ctx.put("symbols.static_construct_object_internal.label", Hex(0x3450), HIGH, "stub.sco", ["sco"])
        if vtable:
            ctx.put("symbols.gmalloc_vtable.label", Hex(0x5400), MEDIUM, "stub.vt", ["vt"])
    return [("platform", platform.detect), ("stub", stub)]


class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.eboot = os.path.join(self.tmp.name, "eboot.bin")
        Path(self.eboot).write_bytes(tiny_image())
        self.param = os.path.join(self.tmp.name, "param.json")
        Path(self.param).write_text(json.dumps({"titleId": "PPSA00002", "contentVersion": "01.001.000"}))
        self.saved = detectors.ORDER

    def tearDown(self):
        detectors.ORDER = self.saved

    def run_cli(self, *extra, order=None):
        detectors.ORDER = order or stub_detectors()
        out = os.path.join(self.tmp.name, "out%d" % len(os.listdir(self.tmp.name)))
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = cli.main(["--eboot", self.eboot, "--param", self.param, "--out", out, "--utc", "2000-01-01T00:00:00Z", *extra])
        return rc, out, buf.getvalue()

    def test_nothing_is_written_unless_asked_for(self):
        rc, out, _t = self.run_cli()
        self.assertEqual(rc, 0)
        self.assertFalse(Path(out, "runtime").exists())
        self.assertNotIn("runtime/SeedTable.h", [e["path"] for e in json.loads(Path(out, "runtime-profile.json").read_text())["emit"]])

    def test_emit_seedtable_writes_the_header_and_lists_it_when_inside_the_folder(self):
        target = os.path.join(self.tmp.name, "out%d" % len(os.listdir(self.tmp.name)), "runtime", "SeedTable.h")
        rc, out, text = self.run_cli("--emit-seedtable", target)
        self.assertEqual(rc, 0, text)
        self.assertEqual(out, os.path.dirname(os.path.dirname(target)))
        data = Path(target).read_bytes()
        doc = json.loads(Path(out, "runtime-profile.json").read_text())
        rec = [e for e in doc["emit"] if e["path"] == "runtime/SeedTable.h"]
        self.assertEqual(len(rec), 1)
        self.assertEqual(rec[0]["bytes"], len(data))
        p = Parsed(data.decode("utf-8"))
        self.assertEqual(p.consts["kRequiredSetComplete"]["text"], "true")
        self.assertEqual(p.slots["kGMalloc"]["label"], 0x6a00)
        self.assertEqual(p.consts["kBaseObserved"]["number"], 4 * 1024 * 1024)        # the platform base of the profile
        self.assertEqual(doc["status"], "generated")
        self.assertIsNone(doc["verification"])
        self.assertIn("SeedTable.h:", text)
        self.assertTrue(Path(out, "evidence", "seedtable.json").exists())

    def test_a_path_outside_the_folder_is_written_but_not_listed(self):
        target = os.path.join(self.tmp.name, "elsewhere", "SeedTable.h")
        rc, out, _t = self.run_cli("--emit-seedtable", target, "--seed-gmalloc", "base_slot")
        self.assertEqual(rc, 0)
        self.assertEqual(Parsed(Path(target).read_text(encoding="utf-8")).slots["kGMalloc"]["label"], 0x6a40)
        self.assertEqual(json.loads(Path(out, "runtime-profile.json").read_text())["emit"], [])

    def test_heap_span_option(self):
        target = os.path.join(self.tmp.name, "h.h")
        rc, _out, _t = self.run_cli("--emit-seedtable", target, "--seed-heap-span", "omit")
        self.assertEqual(rc, 0)
        self.assertNotIn("kHeapLo", Parsed(Path(target).read_text(encoding="utf-8")).consts)

    def test_incomplete_table_is_written_with_the_slots_absent_and_says_so(self):
        target = os.path.join(self.tmp.name, "p.h")
        rc, _out, text = self.run_cli("--emit-seedtable", target, order=stub_detectors(slots=False))
        self.assertEqual(rc, 0)
        p = Parsed(Path(target).read_text(encoding="utf-8"))
        self.assertTrue(all(not s["present"] for s in p.slots.values()))
        self.assertEqual(p.consts["kRequiredSetComplete"]["text"], "false")
        self.assertIn("kRequiredSetComplete=false", text)

    def test_refusal_has_its_own_exit_code_and_the_profile_is_still_written(self):
        target = os.path.join(self.tmp.name, "r.h")
        rc, out, text = self.run_cli("--emit-seedtable", target, order=stub_detectors(pool=False))
        self.assertEqual(rc, cli.EXIT_SEEDTABLE_REFUSED)
        self.assertFalse(os.path.exists(target))
        self.assertIn("SeedTable.h not written", text)
        self.assertTrue(Path(out, "runtime-profile.json").exists())

    def test_bad_option_values_exit_2_before_any_work(self):
        for args in (["--seed-heap-span", "nope"], ["--load-base", "zzz"], ["--load-base", "0x400001"]):
            rc, out, text = self.run_cli("--emit-seedtable", os.path.join(self.tmp.name, "x.h"), *args)
            self.assertEqual(rc, 2, args)
            self.assertFalse(os.path.exists(out), args)

    def test_header_written_by_the_cli_equals_the_one_rendered_from_the_json(self):
        target = os.path.join(self.tmp.name, "c.h")
        rc, out, _t = self.run_cli("--emit-seedtable", target)
        doc = json.loads(Path(out, "runtime-profile.json").read_text())
        self.assertEqual(Path(target).read_text(encoding="utf-8"), emit_seedtable.render_profile(doc)[0])

    def test_standalone_module_cli(self):
        profile = os.path.join(self.tmp.name, "runtime-profile.json")
        Path(profile).write_text(json.dumps(sample_profile()))
        target = os.path.join(self.tmp.name, "s.h")
        buf = io.StringIO()
        with redirect_stdout(buf), redirect_stderr(buf):
            rc = emit_seedtable.main(["--profile", profile, "--out", target, "--seed-gmalloc", "base_slot"])
        self.assertEqual(rc, 0)
        self.assertEqual(Parsed(Path(target).read_text(encoding="utf-8")).slots["kGMalloc"]["label"], 0x6a40)
        broken = sample_profile()
        del broken["symbols"]["name_pool"]
        Path(profile).write_text(json.dumps(broken))
        with redirect_stdout(buf), redirect_stderr(buf):
            self.assertEqual(emit_seedtable.main(["--profile", profile, "--out", os.path.join(self.tmp.name, "t.h")]), 4)
        self.assertFalse(os.path.exists(os.path.join(self.tmp.name, "t.h")))


# ---------------------------------------------------------------------------------------------
CURRENT_TEXT = """// SeedTable.h — GENERATED by gen_seed_table.py from SEED-TABLE.json.
// Format contract: SEED-TABLE-FORMAT.md v2 (worker-initializer-integration).
#ifndef GUARD_
#define GUARD_
#include <cstdint>
#include <string_view>

namespace RC::Unreal::UnrealInitializer::SeedTable
{
    inline constexpr bool kGenerated = true;
    inline constexpr uint64_t kBaseObserved = 0x200000;
    inline constexpr uint64_t kFNamePoolLabel = 0x7200;
    inline constexpr uint64_t kHeapLo = 0x1000;
    inline constexpr uint64_t kHeapHi = 0x9000;
    // old comment
    inline constexpr uint64_t kGMallocInstanceLabel = 0x6b00; // FMallocBinned2 global instance (phdr1 BSS)
    inline constexpr uint64_t kGMallocVtableLabel = 0x5400;   // FMallocBinned2 vtable (phdr1 RELRO)
    struct SlotSeed
    {
        bool present;
        bool data_kind;
        uint64_t label;
        const char* name;
        const char* derivation;
    };
    inline constexpr SlotSeed kGUObjectArray{ true, true, 0x6100, "g_uobject_array", "{\\"anchor_bytes\\":\\"x\\\\u2014y\\"}" }; // slot 0 (data)
    inline constexpr SlotSeed kFNameCtorWchar{ true, false, 0x2340, "fname_ctor_wchar", "{\\"anchor_bytes\\":\\"554889e5\\"}" }; // slot 2 (code)
    inline constexpr SlotSeed kGMalloc{ true, true, 0x6a40, "gmalloc", "{\\"anchor_bytes\\":\\"old\\"}" }; // slot 3 (data)
    inline constexpr SlotSeed kStaticConstructObject{ true, false, 0x3450, "static_construct_object_internal", "{\\"anchor_bytes\\":\\"55\\"}" }; // slot 4 (code)
    inline constexpr bool kRequiredSetComplete = true;
    inline constexpr std::string_view kImageSha256 = "%s";
}
#endif
""" % ("5a" * 32)


class DiffTests(unittest.TestCase):
    def test_parser_reads_every_field_of_a_header(self):
        p = Parsed(CURRENT_TEXT)
        self.assertEqual(p.consts["kHeapHi"]["number"], 0x9000)
        self.assertEqual(p.consts["kGMallocVtableLabel"]["comment"], "FMallocBinned2 vtable (phdr1 RELRO)")
        self.assertEqual(p.slots["kGUObjectArray"]["derivation"], {"anchor_bytes": "x—y"})      # — decoded like a C++ compiler
        self.assertEqual(p.consts["kImageSha256"]["text"], '"%s"' % ("5a" * 32))
        self.assertEqual(p.unrecognised, [])
        self.assertTrue(p.has_endif and p.final_newline)

    def test_c_unescape(self):
        self.assertEqual(diff_seedtable.c_unescape('a\\"b\\\\c\\n\\u2014\\x41\\101'), 'a"b\\c\n—AA')
        with self.assertRaises(ValueError):
            diff_seedtable.c_unescape("\\q")

    def test_identical_headers_have_no_differences(self):
        d = diff_seedtable.compare(CURRENT_TEXT, CURRENT_TEXT)
        self.assertEqual(d.differences, [])
        self.assertTrue(all(status == "same" for _f, _c, _e, status in d.rows))

    def test_groups_of_differences_against_the_emitter(self):
        doc = sample_profile()
        d, emitted = diff_seedtable.compare_profile(doc, CURRENT_TEXT)
        by = {(x.group, x.field): x for x in d.differences}
        # values: heap envelope (retired) and the GMalloc variable; the note names the profile symbol with the old value
        self.assertEqual(by[("value", "kHeapLo")].emitted, "0x0")
        self.assertEqual(by[("value", "kHeapHi")].emitted, "0xffffffffffffffff")
        g = by[("value", "kGMalloc.label")]
        self.assertEqual((g.current, g.emitted), ("0x6a40", "0x6a00"))
        self.assertIn("symbols.gmalloc_base_slot", g.note)
        # presence: the vtable constant is only in the current file
        v = by[("presence", "kGMallocVtableLabel")]
        self.assertEqual(v.emitted, "(not emitted)")
        self.assertIn("symbols.gmalloc_vtable", v.note)
        # derivations differ for every slot, summarised by key set
        self.assertEqual({f for g_, f in by if g_ == "derivation"},
                         {"kGUObjectArray.derivation", "kFNameCtorWchar.derivation", "kGMalloc.derivation", "kStaticConstructObject.derivation"})
        self.assertIn("keys only in the emitted file: rip_arithmetic, uniqueness, method, confidence, source", d.derivations["kGMalloc"])
        self.assertTrue(any("emitted bytes extend the current ones" in l for l in d.derivations["kFNameCtorWchar"]))
        # order/format: producer line, extra comments
        fields = {f for g_, f in by if g_ == "order/format"}
        self.assertIn("header comment, line 1", fields)
        self.assertIn("comment lines inside the namespace", fields)
        self.assertEqual(d.counts()["presence"], 1)
        # base_slot variant: the GMalloc label no longer differs
        d2, _t = diff_seedtable.compare_profile(doc, CURRENT_TEXT, "base_slot")
        self.assertNotIn(("value", "kGMalloc.label"), {(x.group, x.field) for x in d2.differences})

    def test_order_differences_are_detected(self):
        swapped = CURRENT_TEXT.replace("kHeapLo", "TMP").replace("kHeapHi", "kHeapLo").replace("TMP", "kHeapHi")
        d = diff_seedtable.compare(CURRENT_TEXT, swapped)
        self.assertTrue(any(x.field == "field order" for x in d.by_group("order/format")))

    def test_format_differences_are_detected(self):
        crlf = CURRENT_TEXT.replace("\n", "\r\n")
        d = diff_seedtable.compare(CURRENT_TEXT, crlf)
        self.assertIn("line ending", {x.field for x in d.differences})
        d = diff_seedtable.compare(CURRENT_TEXT, CURRENT_TEXT.replace("kGMallocInstanceLabel = 0x6b00; // FMallocBinned2", "kGMallocInstanceLabel = 0x6b00; // Other"))
        self.assertIn("kGMallocInstanceLabel trailing comment", {x.field for x in d.differences})

    def test_markdown_report_has_every_group_and_does_not_touch_the_current_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            current = Path(tmp, "SeedTable.h")
            current.write_text(CURRENT_TEXT, encoding="utf-8")
            before = (current.read_bytes(), current.stat().st_mtime_ns)
            profile = Path(tmp, "runtime-profile.json")
            profile.write_text(json.dumps(sample_profile()))
            report = Path(tmp, "DIFF.md")
            buf = io.StringIO()
            with redirect_stdout(buf), redirect_stderr(buf):
                rc = diff_seedtable.main(["--profile", str(profile), "--current", str(current), "--markdown", str(report),
                                          "--command", "profilegen test run"])
            self.assertEqual(rc, 0)
            self.assertEqual((current.read_bytes(), current.stat().st_mtime_ns), before)
            md = report.read_text(encoding="utf-8")
            for heading in ("## Inputs", "## Summary", "## Value differences", "## Presence differences",
                            "## Derivation-text differences", "## Ordering and format differences",
                            "## With `--seed-gmalloc base_slot`", "## Field-by-field"):
                self.assertIn(heading, md)
            self.assertIn("profilegen test run", md)
            self.assertIn("`kHeapLo`", md)
            self.assertIsNone(MARKER.search(md), "the report carries no verification wording")


# ---------------------------------------------------------------------------------------------
TOWNFALL_PRESENT = os.path.exists(TOWNFALL_EBOOT) and os.path.exists(TOWNFALL_PARAM)


@unittest.skipUnless(TOWNFALL_PRESENT, "Townfall executable / param.json not available")
@unittest.skipUnless(Decoder.available, "capstone is not installed")
class TownfallSeedTableTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = os.path.join(cls.tmp.name, "profile")
        cls.header = os.path.join(cls.tmp.name, "SeedTable.h")
        cls.result = cli.generate(TOWNFALL_EBOOT, TOWNFALL_PARAM, cls.out, utc="2000-01-01T00:00:00Z", seedtable=cls.header)
        cls.doc = cls.result.doc
        cls.text = Path(cls.header).read_text(encoding="utf-8")
        cls.parsed = Parsed(cls.text)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_complete_table_with_the_generators_gmalloc(self):
        p = self.parsed
        self.assertEqual(p.consts["kRequiredSetComplete"]["text"], "true")
        env_label = lambda name: int(self.doc["symbols"][name]["label"]["value"], 16)
        self.assertEqual(p.slots["kGMalloc"]["label"], env_label("gmalloc"))
        self.assertEqual(p.slots["kGUObjectArray"]["label"], env_label("object_array"))
        self.assertEqual(p.slots["kFNameCtorWchar"]["label"], env_label("fname_ctor_wchar"))
        self.assertEqual(p.slots["kStaticConstructObject"]["label"], env_label("static_construct_object_internal"))
        self.assertEqual(p.consts["kFNamePoolLabel"]["number"], env_label("name_pool"))
        self.assertEqual(p.consts["kBaseObserved"]["number"], int(self.doc["load"]["base"]["value"], 16))
        self.assertEqual(p.consts["kImageSha256"]["text"], '"%s"' % self.doc["binding"]["executable"]["sha256"]["value"])
        self.assertEqual(p.consts["kGMallocInstanceLabel"]["number"], env_label("gmalloc_instance"))
        self.assertNotEqual(env_label("gmalloc"), env_label("gmalloc_base_slot"))
        self.assertIn("Not seeded: symbols.gmalloc_base_slot", self.text)

    def test_base_slot_choice_seeds_the_other_variable(self):
        text, _m = emit_seedtable.render_profile(self.doc, SeedOptions("base_slot"))
        self.assertEqual(Parsed(text).slots["kGMalloc"]["label"], int(self.doc["symbols"]["gmalloc_base_slot"]["label"]["value"], 16))

    def test_no_verification_marker_in_the_real_header(self):
        self.assertIsNone(MARKER.search(self.text))
        for choice in emit_seedtable.GMALLOC_CHOICES:
            self.assertIsNone(MARKER.search(emit_seedtable.render_profile(self.doc, SeedOptions(choice))[0]))

    @needs_cxx
    def test_real_header_compiles_as_cpp20(self):
        r = syntax_check(self.header)
        self.assertEqual(r.returncode, 0, r.stderr)
        for choice in emit_seedtable.GMALLOC_CHOICES:
            for heap in (None, "omit"):
                p = Path(self.tmp.name, "v_%s_%s.h" % (choice, heap))
                p.write_text(emit_seedtable.render_profile(self.doc, SeedOptions(choice, heap))[0], encoding="utf-8")
                r = syntax_check(p)
                self.assertEqual(r.returncode, 0, r.stderr)

    @unittest.skipUnless(os.path.exists(CURRENT_SEEDTABLE), "the runtime's current SeedTable.h is not available")
    def test_against_the_runtimes_current_header(self):
        current = Path(CURRENT_SEEDTABLE).read_text(encoding="utf-8")
        cur = Parsed(current)
        self.assertEqual(cur.unrecognised, [])
        d, _t = diff_seedtable.compare_profile(self.doc, current)
        by = {(x.group, x.field) for x in d.differences}
        # everything the generator and the seed agree on
        for same in ("kBaseObserved", "kFNamePoolLabel", "kImageSha256", "kGMallocInstanceLabel", "kRequiredSetComplete", "kGenerated"):
            self.assertNotIn(("value", same), by, same)
        for slot in ("kGUObjectArray", "kFNameCtorWchar", "kStaticConstructObject"):
            self.assertNotIn(("value", slot + ".label"), by, slot)
        # the one slot where the generator measured something else, and the retired envelope
        self.assertIn(("value", "kGMalloc.label"), by)
        self.assertIn(("value", "kHeapLo"), by)
        # and with the base_slot choice that slot agrees with the seed
        d2, _t = diff_seedtable.compare_profile(self.doc, current, "base_slot")
        self.assertNotIn(("value", "kGMalloc.label"), {(x.group, x.field) for x in d2.differences})
        # order of the shared fields is the format's order
        self.assertEqual([n for n in cur.order if n in self.parsed.order], [n for n in self.parsed.order if n in cur.order])


if __name__ == "__main__":
    unittest.main()

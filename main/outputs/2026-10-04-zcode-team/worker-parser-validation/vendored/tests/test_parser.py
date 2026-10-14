"""Unit tests for modbridge_facts.

Synthetic edge cases cover: v1/v2 missing-comma variants, truncated line,
unknown build_id, duplicate seq, non-object top level, blank lines, and the
parse-mode exit/output contract (nothing silently dropped).

The dispatch also requires tests over the TWO REAL console logs
(outputs/2026-10-04-zcode-controller/console/). Those files were absent at
build time (verified; see ../CLAIM.md), so RealLogTests auto-discovers them
(via INGAME_FACTS_REAL_LOGS env var or the controller console directory) and
SKIPS with an explicit reason until they exist — a visible skip, never a
silent pass. Dropping the logs into place makes those tests run with zero
code changes.
"""
from __future__ import annotations

import glob
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
PKG_ROOT = os.path.dirname(HERE)
sys.path.insert(0, PKG_ROOT)

from modbridge_facts import parse_file, parse_lines, summary_table  # noqa: E402
from modbridge_facts.cli import main as cli_main  # noqa: E402
from modbridge_facts.parser import UnrepairableLineError  # noqa: E402

CONTROLLER_CONSOLE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(PKG_ROOT)))),
    "outputs", "2026-10-04-zcode-controller", "console")


def _wellformed(build="b1", seq=1):
    return json.dumps({"build_id": build, "seq": seq,
                       "experiments": {"move_smoke": {
                           "rc": 0, "errno": None,
                           "fields": {"ticks": 12, "hits": 1},
                           "needs_base": False,
                           "candidates": ["alpha", "beta"]}}},
                      separators=(",", ":"))


def _missing_comma_v2(build="b1", seq=2):
    # v2 writer malformation: comma omitted between members of the inner object
    return ('{"build_id":"%s","seq":%d,"experiments":{"jump_probe":'
            '{"rc":3 "errno":7,"fields":{"ticks":5},"needs_base":true,'
            '"candidates":["gamma"]}}}' % (build, seq))


class ParserTests(unittest.TestCase):
    def parse(self, text, **kw):
        return parse_lines(text.splitlines(keepends=True), **kw)

    # --- v1/v2 missing-comma malformation ---------------------------------
    def test_v2_missing_comma_inside_object(self):
        outcomes, rep = self.parse(_missing_comma_v2() + "\n")
        self.assertEqual(rep.unrepairable_lines, 0)
        self.assertEqual(rep.repaired_lines, 1)
        self.assertEqual(rep.experiments[0].name, "jump_probe")
        self.assertEqual(rep.experiments[0].errno, 7)
        self.assertTrue(rep.experiments[0].needs_base)
        self.assertEqual(len(outcomes[0].repairs), 1)

    def test_v1_missing_comma_between_string_values(self):
        line = '{"build_id":"b1" "seq":9}'
        outcomes, rep = self.parse(line + "\n")
        self.assertEqual(rep.repaired_lines, 1)
        self.assertEqual(outcomes[0].record["seq"], 9)

    def test_missing_comma_nested_array(self):
        line = '{"build_id":"b1","seq":3,"c":[1 2,3]}'
        _, rep = self.parse(line + "\n")
        self.assertEqual(rep.repaired_lines, 1)
        self.assertEqual(rep.experiments, [])

    def test_multiple_missing_commas_one_line(self):
        line = '{"build_id":"b1" "seq":4 "k":true "n":1.5}'
        _, rep = self.parse(line + "\n")
        self.assertEqual(rep.repaired_lines, 1)

    def test_repairs_are_logged_not_silent(self):
        _, rep = self.parse(_missing_comma_v2() + "\n")
        self.assertEqual(rep.repaired_lines, 1)
        # every repaired line is visible in the report counters and outcomes
        self.assertEqual(rep.ok_lines + rep.repaired_lines, 1)

    # --- required synthetic edge cases ------------------------------------
    def test_truncated_line_is_unrepairable(self):
        line = '{"build_id":"b1","seq":1'
        outcomes, rep = self.parse(line + "\n")
        self.assertEqual(rep.unrepairable_lines, 1)
        self.assertIsNone(outcomes[0].record)
        self.assertTrue(rep.fatal)
        self.assertEqual(rep.unrepairable[0]["raw"], line)  # preserved verbatim
        # at the low-level repair API a truncated line raises (parse_line
        # reports it as an outcome anomaly instead, by contract)
        from modbridge_facts.parser import _repair_missing_commas
        with self.assertRaises(UnrepairableLineError):
            _repair_missing_commas(line)

    def test_unknown_build_id_flagged_via_registry(self):
        _, rep = self.parse(_wellformed(build="mystery") + "\n",
                            expect_build_ids=["b1"])
        self.assertEqual(rep.unknown_build_lines, [1])
        self.assertFalse(rep.fatal)  # reported, but not an unrepairable line

    def test_unknown_build_id_not_flagged_without_registry(self):
        _, rep = self.parse(_wellformed(build="anything") + "\n")
        self.assertEqual(rep.unknown_build_lines, [])

    def test_duplicate_seq_flagged_kept(self):
        text = _wellformed(seq=5) + "\n" + _wellformed(seq=5) + "\n"
        outcomes, rep = self.parse(text)
        self.assertEqual(len(outcomes), 2)
        self.assertEqual(rep.duplicate_seqs,
                         [{"build_id": "b1", "seq": 5, "count": 2}])

    def test_non_object_top_level_is_invalid_envelope(self):
        _, rep = self.parse("[1,2]\n")
        self.assertEqual(rep.invalid_envelope_lines, 1)
        self.assertTrue(rep.fatal)

    def test_blank_lines_counted_not_fatal(self):
        _, rep = self.parse("\n   \n" + _wellformed() + "\n")
        self.assertEqual(rep.blank_lines, 2)
        self.assertFalse(rep.fatal)
        self.assertEqual(rep.total_lines, 3)

    def test_envelope_missing_fields(self):
        _, rep = self.parse('{"x":1}\n')
        self.assertEqual(rep.invalid_envelope_lines, 1)

    # --- extraction semantics ---------------------------------------------
    def test_entry_startup_and_summary(self):
        text = (json.dumps({"build_id": "b1", "seq": 1, "event": "entry"}) + "\n"
                + _wellformed() + "\n"
                + json.dumps({"build_id": "b1", "seq": 3, "startup": True}) + "\n")
        _, rep = self.parse(text)
        self.assertTrue(rep.entry_present)
        self.assertTrue(rep.startup_present)
        table = summary_table([rep])
        self.assertIn("entry present: yes", table)
        self.assertIn("startup present: yes", table)
        self.assertIn("move_smoke", table)

    def test_needs_base_and_candidates_in_summary(self):
        _, rep = self.parse(_wellformed() + "\n")
        e = rep.experiments[0]
        self.assertIs(e.needs_base, False)
        self.assertEqual(e.candidates, ["alpha", "beta"])
        self.assertEqual(e.fields, {"ticks": 12, "hits": 1})


class CliTests(unittest.TestCase):
    def _run_cli(self, text, extra=None):
        with tempfile.TemporaryDirectory() as td:
            log = os.path.join(td, "facts.jsonl")
            with open(log, "w") as f:
                f.write(text)
            out_dir = os.path.join(td, "out")
            argv = ["parse", log, "--out-dir", out_dir] + (extra or [])
            code = cli_main(argv)
            parsed = os.path.join(out_dir, "facts.parsed.jsonl")
            errors = os.path.join(out_dir, "facts.errors.jsonl")
            repairs = os.path.join(out_dir, "facts.repairs.json")
            recs = ([json.loads(l) for l in open(parsed).read().splitlines()]
                    if os.path.exists(parsed) else [])
            errs = ([json.loads(l) for l in open(errors).read().splitlines()]
                    if os.path.exists(errors) else [])
            reps = ([json.loads(l) for l in open(repairs).read().splitlines()]
                    if os.path.exists(repairs) else [])
            return code, recs, errs, reps

    def test_clean_file_exit_zero(self):
        code, recs, errs, reps = self._run_cli(_wellformed() + "\n")
        self.assertEqual(code, 0)
        self.assertEqual(len(recs), 1)
        self.assertEqual(errs, [])
        self.assertEqual(reps, [])

    def test_unrepairable_exit_nonzero_no_silent_drop(self):
        raw = '{"build_id":"b1","seq":1'
        code, recs, errs, reps = self._run_cli(
            _wellformed() + "\n" + raw + "\n")
        self.assertEqual(code, 1)
        self.assertEqual(len(recs), 1)          # good line still parsed
        self.assertEqual(len(errs), 1)          # bad line preserved
        self.assertEqual(errs[0]["raw"], raw)   # byte-for-byte
        self.assertIn("line_no", errs[0])

    def test_repaired_line_recorded_in_repairs_and_meta(self):
        code, recs, errs, reps = self._run_cli(_missing_comma_v2() + "\n")
        self.assertEqual(code, 0)
        self.assertEqual(len(reps), 1)
        self.assertEqual(recs[0]["_meta"]["repairs"], reps[0]["insertion_offsets"])
        self.assertEqual(len(reps[0]["insertion_offsets"]), 1)

    def test_unknown_build_id_via_cli_flag(self):
        code, recs, errs, _ = self._run_cli(
            _wellformed(build="ghost") + "\n",
            extra=["--expect-build-id", "b1"])
        self.assertEqual(code, 0)
        self.assertIn("unknown-build: 'ghost'", recs[0]["_meta"]["anomalies"])

    def test_duplicate_seq_via_cli(self):
        code, recs, errs, _ = self._run_cli(
            _wellformed(seq=7) + "\n" + _wellformed(seq=7) + "\n")
        self.assertEqual(code, 0)
        self.assertEqual(len(recs), 2)  # both kept, never dropped


class RealLogTests(unittest.TestCase):
    """Required tests over the two real console logs.

    Auto-discovery: INGAME_FACTS_REAL_LOGS (path-sep separated) wins;
    otherwise outputs/2026-10-04-zcode-controller/console/*.jsonl.
    Absent logs SKIP with the exact expected location — visible, never silent.
    """

    def _real_logs(self):
        env = os.environ.get("INGAME_FACTS_REAL_LOGS")
        if env:
            paths = [p for p in env.split(os.pathsep) if p.strip()]
            reason = None
        else:
            paths = sorted(glob.glob(os.path.join(CONTROLLER_CONSOLE, "*.jsonl")))
            reason = ("real logs not delivered: %s does not exist "
                      "(verified 2026-10-04; see CLAIM.md)" % CONTROLLER_CONSOLE)
        return paths, reason

    def test_both_real_logs_parse_fully(self):
        paths, skip_reason = self._real_logs()
        if len(paths) < 2:
            self.skipTest(skip_reason or
                          "INGAME_FACTS_REAL_LOGS needs >= 2 log paths")
        for p in paths[:2]:
            with open(p, encoding="utf-8", errors="surrogateescape") as f:
                _, rep = parse_lines(f, path=p)
            # The real logs were written by a working writer: after repair,
            # every line must parse. Zero unrepairable lines is the acceptance
            # signal for the repair rules; a failure here means the rules must
            # be refined FROM THAT LOG's actual bytes (see parser.py provenance).
            self.assertEqual(
                rep.unrepairable_lines, 0,
                "%s: %d unrepairable line(s), first: %r"
                % (p, rep.unrepairable_lines,
                   rep.unrepairable[0] if rep.unrepairable else None))
            self.assertGreater(rep.total_lines, 0)
            self.assertTrue(rep.build_ids, "no build_id envelope found in %s" % p)
            print("\n[real log] %s: %s" % (os.path.basename(p),
                                           summary_table([rep]).splitlines()[2]))


if __name__ == "__main__":
    unittest.main(verbosity=2)

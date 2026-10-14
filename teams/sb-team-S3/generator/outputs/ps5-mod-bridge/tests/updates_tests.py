"""Pure update-chain resolver tests: no network, no clock, no files."""
import copy
from pathlib import Path
import random
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "companion"))
from modbridge_updates import MAX_STEPS, resolve_file_update


def listed(*ids, **extra):
    return [dict({"file_id": i, "name": f"File {i}", "version": f"v{i}", "category_name": "MAIN",
                  "uploaded_timestamp": 1000 + i, "description": "private", "size_kb": 1}, **extra) for i in ids]


def edge(old, new):
    return {"old_file_id": old, "new_file_id": new, "old_file_name": f"File {old}", "new_file_name": f"File {new}",
            "uploaded_timestamp": 2000 + new, "uploaded_time": ""}


def chain(*ids):
    return [edge(a, b) for a, b in zip(ids, ids[1:])]


def latest(i):
    return {"file_id": i, "name": f"File {i}", "version": f"v{i}", "category_name": "MAIN", "uploaded_timestamp": 1000 + i}


class ResolverTests(unittest.TestCase):
    def test_no_edge_and_still_listed_is_current_with_the_file_itself_as_latest(self):
        self.assertEqual(resolve_file_update(5, listed(5, 6), []), {"status": "current", "latest": latest(5), "steps": 0})
        # Edges that do not start at this file do not matter.
        self.assertEqual(resolve_file_update(6, listed(5, 6, 9), chain(5, 9)), {"status": "current", "latest": latest(6), "steps": 0})

    def test_current_file_absent_from_the_list_and_no_edge_is_unknown(self):
        self.assertEqual(resolve_file_update(5, listed(6, 7), []), {"status": "unknown", "latest": None, "steps": 0})
        self.assertEqual(resolve_file_update(5, [], []), {"status": "unknown", "latest": None, "steps": 0})
        self.assertEqual(resolve_file_update(5, listed(6), chain(6, 7)), {"status": "unknown", "latest": None, "steps": 0})

    def test_a_chain_to_a_listed_file_is_newer_and_counts_steps(self):
        self.assertEqual(resolve_file_update(1, listed(1, 2), chain(1, 2)), {"status": "newer", "latest": latest(2), "steps": 1})
        self.assertEqual(resolve_file_update(1, listed(2, 4), chain(1, 3, 4)), {"status": "newer", "latest": latest(4), "steps": 2})
        # The current file itself may already be gone from the list (archived); the chain still resolves.
        self.assertEqual(resolve_file_update(1, listed(2, 3), chain(1, 2, 3))["steps"], 2)
        # A middle file of the chain does not need to be listed, only the end.
        self.assertEqual(resolve_file_update(1, listed(1, 4), chain(1, 2, 3, 4))["status"], "newer")

    def test_a_chain_that_ends_at_an_unlisted_file_is_unknown(self):
        self.assertEqual(resolve_file_update(1, listed(1, 2), chain(1, 2, 3)), {"status": "unknown", "latest": None, "steps": 2})
        self.assertEqual(resolve_file_update(1, listed(1), chain(1, 2))["status"], "unknown")

    def test_cycles_are_unknown(self):
        for edges in (chain(1, 1), chain(1, 2, 1), chain(1, 2, 3, 1), chain(1, 2, 3, 2), [edge(1, 2), edge(2, 2)]):
            with self.subTest(edges=edges):
                result = resolve_file_update(1, listed(1, 2, 3), edges)
                self.assertEqual((result["status"], result["latest"]), ("unknown", None))

    def test_a_branch_on_the_path_is_unknown_but_a_branch_elsewhere_is_ignored(self):
        self.assertEqual(resolve_file_update(1, listed(1, 2, 3), [edge(1, 2), edge(1, 3)])["status"], "unknown")
        self.assertEqual(resolve_file_update(1, listed(1, 2, 3, 4), [edge(1, 2), edge(2, 3), edge(2, 4)])["status"], "unknown")
        # Two different olds merging into one new file is not a branch.
        self.assertEqual(resolve_file_update(1, listed(1, 2, 3), [edge(1, 3), edge(2, 3)])["status"], "newer")
        # A branch away from the followed chain does not affect it.
        self.assertEqual(resolve_file_update(1, listed(1, 2, 8, 9), [edge(1, 2), edge(8, 9), edge(8, 1)])["status"], "newer")
        self.assertEqual(resolve_file_update(1, listed(1, 2, 8, 9), [edge(1, 2), edge(7, 8), edge(7, 9)])["latest"], latest(2))
        # An identical repeated edge is one edge, not a branch.
        self.assertEqual(resolve_file_update(1, listed(1, 2), [edge(1, 2), edge(1, 2)])["status"], "newer")

    def test_the_step_cap_is_sixty_four_edges(self):
        ids = list(range(1, MAX_STEPS + 3))
        self.assertEqual(MAX_STEPS, 64)
        exactly = resolve_file_update(1, listed(*ids), chain(*ids[:MAX_STEPS + 1]))
        self.assertEqual((exactly["status"], exactly["steps"], exactly["latest"]["file_id"]), ("newer", 64, 65))
        over = resolve_file_update(1, listed(*ids), chain(*ids[:MAX_STEPS + 2]))
        self.assertEqual((over["status"], over["latest"]), ("unknown", None))
        # A cap hit is never turned into "the last file we reached".
        self.assertEqual(resolve_file_update(1, listed(1, 65), chain(*ids[:MAX_STEPS + 2]))["status"], "unknown")

    def test_a_long_acyclic_chain_does_not_recurse(self):
        ids = list(range(1, 10001))
        self.assertEqual(resolve_file_update(1, listed(*ids), chain(*ids))["status"], "unknown")
        self.assertEqual(resolve_file_update(9990, listed(*ids), chain(*ids))["status"], "newer")

    def test_result_is_deterministic_and_independent_of_input_order(self):
        files, edges = listed(*range(1, 12)), chain(1, 4, 7, 11) + [edge(2, 3), edge(5, 6), edge(9, 10)]
        expected = resolve_file_update(1, files, edges)
        self.assertEqual(expected, {"status": "newer", "latest": latest(11), "steps": 3})
        generator = random.Random(7)
        for _ in range(50):
            files_copy, edges_copy = list(files), list(edges)
            generator.shuffle(files_copy)
            generator.shuffle(edges_copy)
            self.assertEqual(resolve_file_update(1, files_copy, edges_copy), expected)
        self.assertEqual(resolve_file_update(1, tuple(files), tuple(edges)), expected)

    def test_inputs_are_not_modified_and_the_result_shares_nothing_with_them(self):
        files, edges = listed(1, 2), chain(1, 2)
        before = copy.deepcopy((files, edges))
        result = resolve_file_update(1, files, edges)
        self.assertEqual((files, edges), before)
        result["latest"]["name"] = "changed"
        self.assertEqual(files, before[0])

    def test_latest_exposes_only_the_summary_with_typed_optional_fields(self):
        result = resolve_file_update(1, listed(1, 2), chain(1, 2))
        self.assertEqual(set(result), {"status", "latest", "steps"})
        self.assertEqual(set(result["latest"]), {"file_id", "name", "version", "category_name", "uploaded_timestamp"})
        sparse = [{"file_id": 1, "name": "One", "version": "1"}, {"file_id": 2, "name": 5, "version": None,
                  "category_name": 7, "uploaded_timestamp": True}]
        result = resolve_file_update(1, sparse, chain(1, 2))
        self.assertEqual(result["latest"], {"file_id": 2, "name": None, "version": None, "category_name": None, "uploaded_timestamp": None})
        result = resolve_file_update(1, sparse[:1], [])
        self.assertEqual(result["latest"], {"file_id": 1, "name": "One", "version": "1", "category_name": None, "uploaded_timestamp": None})

    def test_malformed_input_is_unknown_and_never_raises(self):
        good_files, good_edges = listed(1, 2), chain(1, 2)
        cases = [
            (0, good_files, good_edges), (-1, good_files, good_edges), (True, good_files, good_edges), ("1", good_files, good_edges),
            (None, good_files, good_edges), (1.0, good_files, good_edges), (2 ** 63, good_files, good_edges),
            (1, None, good_edges), (1, good_files, None), (1, "files", good_edges), (1, good_files, {"old_file_id": 1}),
            (1, good_files + ["x"], good_edges), (1, good_files + [{"file_id": 0}], good_edges),
            (1, good_files + [{"file_id": True}], good_edges), (1, good_files + [{}], good_edges),
            (1, good_files, good_edges + [{"old_file_id": 1}]), (1, good_files, good_edges + [7]),
            (1, good_files, [{"old_file_id": 1, "new_file_id": "2"}]), (1, good_files, [{"old_file_id": 1, "new_file_id": 0}]),
            (1, good_files, [{"old_file_id": True, "new_file_id": 2}]),
            # Duplicate listed ids cannot be trusted either.
            (1, good_files + listed(2), good_edges),
        ]
        for args in cases:
            with self.subTest(args=args):
                self.assertEqual(resolve_file_update(*args), {"status": "unknown", "latest": None, "steps": 0})

    def test_extra_keys_in_inputs_are_ignored(self):
        files = [dict(f, key="must-not-leak") for f in listed(1, 2)]
        edges = [dict(e, key="must-not-leak") for e in chain(1, 2)]
        result = resolve_file_update(1, files, edges)
        self.assertEqual(result, {"status": "newer", "latest": latest(2), "steps": 1})
        self.assertNotIn("must-not-leak", repr(result))


if __name__ == "__main__":
    unittest.main()

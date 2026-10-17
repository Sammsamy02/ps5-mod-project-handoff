import unittest
try:
    from classification_adapter import coverage
except ImportError:
    def coverage(*args): return {}

class CoverageTests(unittest.TestCase):
    def test_cross_container_scope_never_becomes_added(self):
        c={'packages':[{'base_replaced':True}]*66+[{'base_replaced':False}]*61}
        s={'catalogue_completeness_verified':False,'targets':[{'matched_package_count':66},{'matched_package_count':2}],'mods':[{'export_package_count':127,'unmatched_package_count':59}]}
        r=coverage(c,s)
        self.assertEqual(r['store_membership_unknown'],61)
        self.assertEqual(r['captured_path_overlaps'],68)
        self.assertEqual(r['unmatched_in_captured_indexes'],59)
        self.assertEqual(r['cross_container_overlap_gap'],2)
        self.assertNotIn('added',r)

if __name__ == '__main__': unittest.main()

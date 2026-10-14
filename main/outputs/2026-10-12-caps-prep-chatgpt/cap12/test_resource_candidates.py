import struct
import unittest
try:
    from resource_candidates import census
except ImportError:
    def census(*a,**k): return {'candidates':[]}

class CandidateTests(unittest.TestCase):
    def test_bounded_values_are_hypotheses_only(self):
        image=struct.pack('<4I',70000,2,70000,7)
        r=census([(image,[0,8],0)],10,8)
        self.assertEqual(r['candidates'][0]['relative_offset'],4)
        self.assertEqual(r['candidates'][0]['max_value'],7)
        self.assertFalse(r['decoded_resource_index'])
    def test_zero_objects_refuses(self):
        with self.assertRaises(ValueError): census([(b'abc',[],0)],10,8)
    def test_out_of_bounds_start_refuses(self):
        with self.assertRaises(ValueError): census([(b'1234',[99],0)],10,8)
    def test_does_not_scan_across_next_object(self):
        r=census([(struct.pack('<3I',1,2,3),[0,4,8],0)],10,8)
        self.assertEqual([c['relative_offset'] for c in r['candidates']],[0])

if __name__=='__main__': unittest.main()

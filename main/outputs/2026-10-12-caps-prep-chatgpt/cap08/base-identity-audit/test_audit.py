import unittest
try:
    from audit import import_matches
except ImportError:
    def import_matches(*args):return []

def metadata(duplicate=False,missing=False):
    target={'package_id':'0000000000000002','decoded':{'exports':[{'name':'Target','public_export_hash':'123456789abcdef0'}],'import_map':[]}}
    source={'package_id':'0000000000000001','decoded':{'exports':[],'import_map':[{'kind':'PackageImport','referenced_package_id':'0000000000000002','referenced_public_export_hash':'123456789abcdef0'}]}}
    if duplicate:target['decoded']['exports']*=2
    return {'packages':[source]+([] if missing else [target])}

class AuditTests(unittest.TestCase):
    def test_exact_package_and_public_hash_closes_local_edge_only(self):
        r=import_matches(metadata());self.assertEqual(r[0]['target_export_name'],'Target');self.assertEqual(r[0]['resolution'],'mod_export_hash_match');self.assertFalse(r[0]['runtime_compatible'])
    def test_missing_external_export_stays_unknown(self):
        r=import_matches(metadata(missing=True));self.assertEqual(r[0]['resolution'],'base_export_payload_required');self.assertNotIn('target_export_name',r[0])
    def test_ambiguous_public_hash_refuses(self):
        with self.assertRaises(ValueError):import_matches(metadata(duplicate=True))

if __name__=='__main__':unittest.main()

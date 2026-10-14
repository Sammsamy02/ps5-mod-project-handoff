#!/usr/bin/env python3
"""Focused pure/read-only checks; never prepares or runs an l23 package."""
from pathlib import Path
import copy,json,sys,hashlib,unittest
sys.dont_write_bytecode=True
import prepare_package as p
import finalize_review as f

class Tools(unittest.TestCase):
    def test_paths(self):
        self.assertEqual(p.ROOT,Path(__file__).resolve().parents[3])
        self.assertEqual(p.BUILD,p.ROOT/'outputs/2026-10-08-l23-build-r2')
        self.assertEqual(p.OUT,p.ROOT/'outputs/2026-10-08-l23-package')
        self.assertTrue((p.ROOT/'AGENTS.md').is_file())
    def test_controller_one_overlay_and_twin_selection(self):
        text=p.adapt_controller((p.BASE/'package_combined.py').read_text())
        self.assertEqual(text.count("for row in inputs['lineage_overlays']:"),1)
        self.assertNotIn('l22_lineage_overlays',text)
        self.assertIn("runtime_row=inputs['runtime_objects_by_pass'][args.pass_id]",text)
        self.assertIn('b.gap(str(error));status=1',text)
        self.assertEqual(text.count("'scePthreadGetthreadid']='sceKernelOpen'"),1)
        compile(text,'controller','exec')
    def test_controller_drift_refuses(self):
        with self.assertRaises(ValueError):p.adapt_controller('no selected baseline')
    def test_current_retained93_and_all4_changes(self):
        current=p.read(p.RUNTIME/'profile/profiles/townfall-build-facts.json')
        baseline=p.read(p.ROOT/'outputs/2026-10-08-l22-runtime/profile/profiles/townfall-build-facts.json')
        changes=p.read(p.RUNTIME/'profile/CHANGES.json')
        self.assertEqual(p.verify_profile(changes,current,baseline,p.RUNTIME/'profile/units'),changes['profile_digest'])
        bad=copy.deepcopy(current);bad['required_members'][0]['offset']+=1
        with self.assertRaises(ValueError):p.verify_profile(changes,bad,baseline,p.RUNTIME/'profile/units')
    def test_overlay_merge_preserves_unknown_and_replaces_headers(self):
        old=p.read(p.BASE/'BUILD-INPUTS.json')['l22_lineage_overlays']
        old=old+[{'destination':'RetainedExtra.hpp','path':'historical','sha256':'historical'}]
        new=p.merge_overlays(old,p.read(p.RUNTIME/'profile/CHANGES.json'))
        self.assertEqual(len(new),4)
        got={r['destination']:r for r in new}
        self.assertEqual(got['RetainedExtra.hpp']['path'],'historical')
        for name in p.HEADERS:self.assertEqual(Path(got[name]['path']),p.RUNTIME/'profile/units'/name)
        with self.assertRaises(ValueError):p.merge_overlays(old+[old[0]],p.read(p.RUNTIME/'profile/CHANGES.json'))
    def test_no_new_offset_promotion(self):
        changes=p.read(p.RUNTIME/'profile/CHANGES.json');changes['new_promoted_facts']=3
        with self.assertRaises(ValueError):p.verify_profile(changes,p.read(p.RUNTIME/'profile/profiles/townfall-build-facts.json'),p.read(p.ROOT/'outputs/2026-10-08-l22-runtime/profile/profiles/townfall-build-facts.json'),p.RUNTIME/'profile/units')
    def test_offline_nuget(self):
        p.offline_config((p.BASE/'inputs/NuGet.Config').read_bytes())
        for raw in (b'<configuration><packageSources><add key="remote" value="https://nuget.org" /></packageSources></configuration>',b'<configuration/>'):
            with self.assertRaises(ValueError):p.offline_config(raw)
        text=(p.BASE/'package_prx_l19.py').read_text()
        self.assertEqual(text.count('"-p:RestoreConfigFile=" + str(Path(__file__).resolve().parent / "inputs/NuGet.Config")'),2)
    def test_import_identity_structural_exact207(self):
        original=p.read(p.BASE/'pass-1/EXACT-IMPORT-IDENTITIES.json')
        moved=copy.deepcopy(original);moved['imports'].reverse()
        for row in moved['imports']:
            for k in ('library','module'):row[k]['value_offset']+=256
        self.assertEqual(f.normalized_identities(original),f.normalized_identities(moved))
        moved['imports'][0]['module']['name']='FOREIGN_PROVIDER'
        self.assertNotEqual(f.normalized_identities(original),f.normalized_identities(moved))
        moved['count']=206
        with self.assertRaises(ValueError):f.normalized_identities(moved)
    def test_retained_completed_package_gate_contract(self):
        result=f.verify_packages(p.BASE,p.BASE)
        self.assertEqual(result['static_checks_each_pass'],20)
        self.assertEqual(result['exact_import_count'],207)
        self.assertEqual(result['prx']['sha256'],result['twin']['sha256'])
    def test_epic_excluded(self):
        for folder in ('official-source','official-headers','2026-10-08-l23-layout-generator'):
            with self.assertRaises(ValueError):f.safe_packet_source(p.ROOT/'outputs'/folder/'UnrealType.h')
        raw=p.RUNTIME/'profile/profiles/townfall-build-facts.json';before=p.sha(raw)
        data,omitted=f.packet_data(raw)
        self.assertEqual(len(omitted),17)
        self.assertEqual(p.sha(raw),before)
        parsed=json.loads(data)
        for row in parsed['evidence']['l22_allocator']['entries']:
            self.assertNotIn('text',row['source_declaration'])
            self.assertIn('text_sha256',row['source_declaration'])

if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Tools))
    out=p.HERE/'tests/RESULTS.json'
    out.write_text(json.dumps({'status':'PASS' if result.wasSuccessful() else 'FAIL','tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),'qualification':'Pure functions and read-only retained l22 artifacts only. l23 preparation, packaging, finalizer and console not run.','scripts':{n:hashlib.sha256((p.HERE/n).read_bytes()).hexdigest() for n in ('prepare_package.py','finalize_review.py','test_tools.py')}},indent=2)+'\n')
    raise SystemExit(not result.wasSuccessful())

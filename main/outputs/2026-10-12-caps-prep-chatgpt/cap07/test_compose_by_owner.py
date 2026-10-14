import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from test_project_packages import api, fixture, HERE
try:
    import compose_by_owner as coordinator
except ModuleNotFoundError:
    coordinator = None
import project_packages


class CompositionTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(coordinator, 'multi-container coordinator missing')
        (HERE/'tmp').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=HERE/'tmp')
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.scratch = self.folder/'scratch'; self.scratch.mkdir()
        self.a, self.b, self.c = 0x1001, 0x1002, 0x1003
        self.bases = [self.base('A', 100, {self.a: ((), ())}),
                      self.base('B', 200, {self.b: ((), ())})]
        self.mods = [self.mod('mod', 500, {
            self.a: ((self.b, self.c), ()), self.b: ((), ()), self.c: ((), ())})]
        self.placements = {('mod', self.c): 'B'}

    def base(self, name, cid, packages, inner_id=None, payload_size=200000):
        raw = struct.pack('<Q', cid)
        header = fixture.make_container_header_bytes(cid if inner_id is None else inner_id, packages)
        chunks = [(raw+bytes.fromhex('00000006'), header)]
        files = []
        for pid in packages:
            chunk = fixture.chunk_id_for(pid)
            chunks.append((chunk, bytes([pid%251])*payload_size))
            files.append(('../../../Game/Content/%x.uasset'%pid, chunk))
        archive = fixture.SyntheticArchive(raw, chunks, files)
        return {'base_id':name,'utoc':archive.utoc,'ucas_size':len(archive.ucas),'header':header}

    def mod(self, name, cid, packages):
        raw = struct.pack('<Q',cid)
        chunks = [(raw+bytes.fromhex('00000006'), fixture.make_container_header_bytes(cid,packages))]
        files = []
        for pid in packages:
            for kind, suffix, data in ((1,'uasset',bytes([pid%251])*70001),
                                       (2,'ubulk',b'bulk'+struct.pack('<Q',pid))):
                chunk=fixture.chunk_id_for(pid,chunk_type=kind)
                chunks.append((chunk,data))
                files.append(('../../../Game/Content/%x.%s'%(pid,suffix),chunk))
        archive=fixture.SyntheticArchive(raw,chunks,files)
        utoc=self.folder/(name+'.utoc'); ucas=utoc.with_suffix('.ucas')
        utoc.write_bytes(archive.utoc);ucas.write_bytes(archive.ucas)
        return api.ModSpec(name,str(utoc),str(ucas))

    def compose(self, bases=None, mods=None, placements=None):
        return coordinator.compose(api, project_packages,
            self.bases if bases is None else bases,
            self.mods if mods is None else mods,
            self.placements if placements is None else placements,
            self.scratch)

    def test_all_owners_payloads_bulk_imports_and_sources_survive(self):
        before=[(Path(m.utoc_path).read_bytes(),Path(m.ucas_path).read_bytes()) for m in self.mods]
        outputs, receipt=self.compose()
        self.assertEqual(set(outputs),{'A','B'})
        for owner,pids in (('A',{self.a}),('B',{self.b,self.c})):
            output=outputs[owner]; toc=api.parse_toc(output['utoc'])
            view=fixture._view(output['sidecar'],self.bases[0 if owner=='A' else 1]['ucas_size'])
            for pid in pids:
                for kind,expected in ((1,bytes([pid%251])*70001),(2,b'bulk'+struct.pack('<Q',pid))):
                    cid=fixture.chunk_id_for(pid,chunk_type=kind)
                    self.assertEqual(api.extract_chunk_bytes(toc,toc.ids.index(cid),view),expected)
            header_id=toc.container_id+bytes.fromhex('00000006')
            merged=api.parse_container_header(api.extract_chunk_bytes(toc,toc.ids.index(header_id),view))
            self.assertEqual(set(merged.packages),pids)
            if owner=='A':self.assertEqual(merged.packages[self.a].imported_packages,(self.b,self.c))
        self.assertEqual(before,[(Path(m.utoc_path).read_bytes(),Path(m.ucas_path).read_bytes()) for m in self.mods])
        self.assertFalse(receipt['runtime_mount_ready'])
        self.assertEqual(list(self.scratch.iterdir()),[])

    def test_two_nonconflicting_mods_compose_in_one_generation(self):
        second=self.mod('extra',501,{0x1004:((),())})
        placements=dict(self.placements);placements[('extra',0x1004)]='A'
        outputs,receipt=self.compose(mods=self.mods+[second],placements=placements)
        self.assertEqual(len(outputs),2)
        self.assertEqual(len(receipt['sources']['mods']),2)
        self.assertEqual(list(self.scratch.iterdir()),[])

    def test_unmatched_package_is_never_silently_assigned(self):
        with self.assertRaisesRegex(ValueError,'unmatched_package_placement_required'):
            self.compose(placements={})
        self.assertEqual(list(self.scratch.iterdir()),[])

    def test_missing_store_refuses_entire_generation(self):
        bases=[dict(b) for b in self.bases];bases[1]['header']=None
        with self.assertRaisesRegex(ValueError,'base_header_unavailable'):
            self.compose(bases=bases)
        self.assertEqual(list(self.scratch.iterdir()),[])

    def test_ambiguous_ownership_and_header_identity_refuse(self):
        with self.subTest('ambiguous'):
            bases=[self.bases[0],self.base('B',200,{self.a:((),()),self.b:((),())})]
            with self.assertRaisesRegex(ValueError,'ambiguous_base_package_owner'):self.compose(bases=bases)
        with self.subTest('identity'):
            bases=[self.base('A',100,{self.a:((),())},inner_id=999),self.bases[1]]
            with self.assertRaisesRegex(ValueError,'base_container_header_identity_mismatch'):self.compose(bases=bases)

    def test_unresolved_import_refuses(self):
        mod=self.mod('unresolved',502,{self.a:((0xdead,),())})
        with self.assertRaisesRegex(ValueError,'external_import_unresolved'):
            self.compose(mods=[mod],placements={})

    def test_conflicting_mods_refuse_by_default(self):
        other=self.mod('conflict',503,{self.a:((),())})
        with self.assertRaisesRegex(ValueError,'mod_conflict_rejected'):
            self.compose(mods=self.mods+[other])

    def test_source_drift_refuses(self):
        source=Path(self.mods[0].ucas_path)
        source.write_bytes(source.read_bytes()+b'x')
        with self.assertRaisesRegex(ValueError,'source_identity_changed'):
            self.compose()

    def test_same_container_identity_cannot_be_split_into_two_owners(self):
        bases=[self.bases[0],self.base('B',100,{self.b:((),())})]
        with self.assertRaisesRegex(ValueError,'duplicate_base_container_identity'):
            self.compose(bases=bases)

    def test_late_second_owner_failure_cleans_all_temporary_projections(self):
        bases=[self.bases[0],self.base('B',200,{self.b:((),())},payload_size=32)]
        with self.assertRaisesRegex(ValueError,'sidecar_would_require_more_than_one_new_partition'):
            self.compose(bases=bases)
        self.assertEqual(list(self.scratch.iterdir()),[])


if __name__=='__main__':unittest.main()

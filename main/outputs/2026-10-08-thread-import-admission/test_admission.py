"""Focused exact-contract and owned-relocation admission controls (host only)."""
import copy, importlib.util, json, pathlib, subprocess, tempfile, unittest

HERE=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('admission_impl',HERE/'import_admission.py')
impl=importlib.util.module_from_spec(spec)

class AdmissionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec.loader.exec_module(impl)
        cls.evidence=json.loads(impl.EVIDENCE.read_text())
        cls.names=['baseline_'+str(i) for i in range(15)]+['scePthreadGetthreadid']
    def test_exact_evidence(self):
        self.assertEqual(impl.validate_evidence(self.evidence)['Name'],'scePthreadGetthreadid')
    def test_wrong_donor(self):
        value=copy.deepcopy(self.evidence);value['identity']['ModuleVersion']=256
        with self.assertRaises(ValueError):impl.validate_evidence(value)
    def test_wrong_nid(self):
        value=copy.deepcopy(self.evidence);value['nid']='wrong'
        with self.assertRaises(ValueError):impl.validate_evidence(value)
    def test_missing_typed_row(self):
        with self.assertRaises(ValueError):impl.generate(self.names[:-1])
    def test_duplicate_typed_row(self):
        with self.assertRaises(ValueError):impl.generate(self.names+['scePthreadGetthreadid'])
    def test_own_slot_controls(self):
        source=impl.generate(self.names)['source']
        with tempfile.TemporaryDirectory(dir=HERE) as folder:
            path=pathlib.Path(folder)
            (path/'admission.c').write_text(source)
            (path/'control.c').write_text('''#include <stdint.h>
#include <stdlib.h>
uintptr_t modbridge_supplier_import_table[16];
uint32_t modbridge_supplier_import_count;
void diagnostic_event(const char *name,uint32_t value){(void)name;(void)value;}
int bridge_native_thread_import_ready(void);
int main(void){
 modbridge_supplier_import_count=16;
 if(bridge_native_thread_import_ready()!=0)return 1;
 modbridge_supplier_import_table[15]=0x12345678;
 if(bridge_native_thread_import_ready()!=1)return 2;
 modbridge_supplier_import_count=15;
 if(bridge_native_thread_import_ready()!=0)return 3;
 modbridge_supplier_import_count=17;
 if(bridge_native_thread_import_ready()!=0)return 4;
 return 0;
}
''')
            for flags in ([],['-fsanitize=address,undefined','-fno-sanitize-recover=all']):
                binary=path/('san' if flags else 'plain')
                built=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror',*flags,
                    str(path/'admission.c'),str(path/'control.c'),'-o',str(binary)],capture_output=True,text=True)
                self.assertEqual(built.returncode,0,built.stderr)
                result=subprocess.run([str(binary)],capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)

if __name__=='__main__':unittest.main()

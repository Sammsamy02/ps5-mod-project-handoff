#!/usr/bin/env python3
"""Inspect paired module bytes, reject two bad contracts, and wrap them locally."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
from build import ROOT,SPIKE,HERE,read,save,sha

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('receipt',type=Path)
    p.add_argument('--baseline-tool',type=Path,default=SPIKE/'native-cppmod-contract-1/3a3c14679b1b4e3593636f3e1975bb65/tool/ContractHost.dll')
    args=p.parse_args()
    receipt=read(args.receipt);out=args.receipt.resolve().parent;verification=out/'verification'
    verification.mkdir(exist_ok=False)
    host=Path(receipt['host']['path']);plugin=Path(receipt['plugin']['path'])
    assert sha(host)==receipt['host']['sha256'] and sha(plugin)==receipt['plugin']['sha256']
    reader=load('declared_import_reader',SPIKE/'ue4ss-native-declared-imports-1/read_declared_imports.py')
    metadata=reader.parse(plugin.read_bytes());contract=read(out/'plugin-contract.json')['contract']['imports']
    assert len(metadata['imports'])==len(contract)==19
    assert not metadata['record_conflicts'] and not metadata['invalid_import_names']
    comparisons=reader.compare(metadata,[x['ImportSymbol']['Name'] for x in contract])
    for expected,actual in zip(contract,comparisons):
        assert actual['status']=='unique' and len(actual['declared_imports'])==1
        actual=actual['declared_imports'][0];identity=expected['ImportSymbol']
        assert actual['binding']==1
        assert actual['symbol_type']=={'FUNC':2,'OBJECT':1}[expected['symbol_type']]
        assert (actual['module']['name'],actual['module']['version'],actual['module']['needed_adjacent'],
                actual['library']['name'],actual['library']['version']) == (
            identity['ModuleName'],identity['ModuleVersion'],identity['Soname'],
            identity['LibraryName'],identity['LibraryVersion'])
    object_names={x['ImportSymbol']['Name'] for x in contract if x['symbol_type']=='OBJECT'}
    assert object_names=={'_ZTIN2RC14CppUserModBaseE','_ZTVN10__cxxabiv120__si_class_type_infoE'}
    owned=[x for x in contract if x['ImportSymbol']['ModuleName']=='modbridge_ue4ss_host_contract']
    system=[x for x in contract if x not in owned]
    assert len(owned)==17 and {x['ImportSymbol']['Name'] for x in system}=={'__cxa_atexit','__cxa_finalize'}
    assert {'__unw_add_dynamic_eh_frame_section','__unw_remove_dynamic_eh_frame_section'} <= {x['ImportSymbol']['Name'] for x in owned}
    save(verification/'actual-plugin-imports.json',dict(metadata=metadata,comparisons=comparisons,
        reader_sha256=sha(SPIKE/'ue4ss-native-declared-imports-1/read_declared_imports.py')))
    dotnet=SPIKE/'sharpprospero-host-tool-1/dotnet/dotnet';tool=[dotnet,out/'tool/ContractHost.dll']
    env=dict(os.environ,DOTNET_ROOT=str(dotnet.parent),DOTNET_CLI_HOME=str(verification/'dotnet-home'),
             DOTNET_CLI_TELEMETRY_OPTOUT='1',DOTNET_NOLOGO='1',DOTNET_MULTILEVEL_LOOKUP='0')
    records=[]
    def call(label,argv,expected=0):
        argv=list(map(str,argv));p=subprocess.run(argv,cwd=ROOT,env=env,capture_output=True,text=True)
        (verification/(label+'.stdout')).write_text(p.stdout);(verification/(label+'.stderr')).write_text(p.stderr)
        records.append(dict(label=label,argv=argv,exit=p.returncode));save(verification/'commands.json',records)
        assert p.returncode==expected,(label,p.stderr)
    # Actual writer gate refuses nonexistent exports and an RTTI-as-function edit.
    badnames=verification/'missing-export.txt';badnames.write_text('nonexistent_owned_contract_export\n')
    obj=read(receipt['source_compile_audit'])['plugin_object']
    badout=verification/'missing-export.prx'
    call('refuse-missing-export',[*tool,'write',obj,out/'plugin-contract.json',badout,badnames],2)
    assert not badout.exists()
    for entry in ['module_entry_does_not_exist','__dso_handle']:
        badout=verification/(entry+'.prx')
        call('refuse-child-entry-'+entry,[*tool,'write',obj,out/'plugin-contract.json',badout,
             out/'plugin-exports.txt','--child-lifecycle','module_start',entry],2)
        assert not badout.exists()
    # The same host object/imports/exports still emit identical default bytes.
    baseline=args.baseline_tool.resolve()
    baseline_dir=verification/'default-baseline';baseline_dir.mkdir()
    baseline_host=baseline_dir/host.name
    call('default-writer-byte-parity',[dotnet,baseline,'write',receipt['runtime_object']['path'],
         out/'host-contract.json',baseline_host,out/'host-exports.txt'])
    assert baseline_host.read_bytes()==host.read_bytes(), 'default library emission changed'
    bad=read(out/'plugin-contract.json')
    next(x for x in bad['contract']['imports'] if x['ImportSymbol']['Name']=='_ZTIN2RC14CppUserModBaseE')['symbol_type']='FUNC'
    bp=verification/'wrong-rtti-type.json';save(bp,bad);badout=verification/'wrong-rtti-type.prx'
    call('refuse-rtti-function',[*tool,'write',obj,bp,badout,out/'plugin-exports.txt'],2)
    assert not badout.exists()
    # Existing SDK wrapper plus retained SharpProspero extraction: byte-only checks.
    wrapped=verification/'wrapped';wrapped.mkdir()
    probe=load('module_probe_recipe',ROOT/'outputs/ps5-mod-bridge/native-module-probe/build.py')
    wrapper=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/install_app/make_fself.py'
    modules=[]
    for original in [host,plugin]:
        target=wrapped/original.name;extracted=verification/(original.stem+'.extracted.elf')
        call(original.stem+'-wrap',[sys.executable,wrapper,original,target])
        assert target.read_bytes()[:4]==bytes.fromhex('4f153d1d')
        call(original.stem+'-extract',[*tool,'extract',target,extracted])
        assert probe.load_segments(original.read_bytes())==probe.load_segments(extracted.read_bytes())
        modules.append(dict(path=str(target),sha256=sha(target),bytes=target.stat().st_size))
    save(verification/'receipt.json',dict(status='PAIRED_IMPORT_EXPORT_AND_SDK_WRAPPER_CHECKS_PASSED',
        input_receipt_sha256=sha(args.receipt),actual_plugin_imports=len(contract),owned_host_imports=len(owned),
        system_imports=len(system),rtti_data_imports=sorted(object_names),negative_controls=4,wrapped=modules,
        default_writer_same_input_bytes_identical=True,
        wrapper_sha256=sha(wrapper),target_execution=False,console_contact=False,
        qualification='Local metadata/link/wrapper verification only; no loader, constructors or C++ lifecycle execution.'))
    print(json.dumps(dict(receipt=str(verification/'receipt.json'),imports=len(contract),negative_controls=4,wrapped=2)))

if __name__=='__main__':main()

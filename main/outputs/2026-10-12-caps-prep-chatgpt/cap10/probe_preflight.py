"""Bounded source contract check; does not parse IL, compile or execute C#."""
import argparse,re,json

def validate(cs, header):
    expected={'modbridge_cap10_abi_version':('int','AbiVersion','',r'int32_t\s+modbridge_cap10_abi_version\s*\(void\)\s*MODBRIDGE_CAP10_NOEXCEPT\s*;'),
              'modbridge_cap10_runtime_probe':('int','RuntimeProbe','int cookie, out int reply',r'int32_t\s+modbridge_cap10_runtime_probe\s*\(int32_t\s+cookie,\s*int32_t\s*\*\s*reply\)\s*MODBRIDGE_CAP10_NOEXCEPT\s*;')}
    errors=[]
    imports=re.findall(r'\[DllImport\("([^"]+)",\s*EntryPoint\s*=\s*"([^"]+)",\s*CallingConvention\s*=\s*CallingConvention\.(\w+),\s*ExactSpelling\s*=\s*(true|false)\)\]\s*private static extern (\w+) (\w+)\(([^)]*)\);',cs)
    if len(re.findall(r'\[DllImport\s*\(',cs))!=2 or len(imports)!=2:errors.append('exactly two explicitly described imports required')
    if "#define MODBRIDGE_CAP10_NOEXCEPT noexcept" not in header:errors.append("missing native noexcept contract")
    seen=set()
    for library,entry,convention,exact,ret,name,args in imports:
        seen.add(entry)
        if entry not in expected:errors.append('unknown import '+entry);continue
        want=expected[entry]
        if (library,convention,exact,ret,name,' '.join(args.split()))!=('__Internal','Cdecl','true',want[0],want[1],want[2]):errors.append('signature mismatch '+entry)
        if not re.search(want[3],header):errors.append('missing/mismatched native declaration '+entry)
    if seen!=set(expected):errors.append('import set mismatch')
    if 'public static int Run(int cookie)' not in cs:errors.append('entry method missing')
    if 'ThrowForHostBoundaryTest' not in cs:errors.append('managed exception boundary probe missing')
    return errors

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('probe');p.add_argument('header');a=p.parse_args()
    errors=validate(open(a.probe).read(),open(a.header).read());print(json.dumps({'passed':not errors,'errors':errors,'label':'Mac-only source text consistency; no compilation/ABI/host execution'},indent=2));raise SystemExit(bool(errors))

# New source-audit behavior starts test-first.
def validate_host_plan(plan, sources):
    apis=plan.get('required_apis',[]);errors=[]
    if not apis:errors.append('zero required host APIs')
    for api in apis:
        source=sources.get(api['file'])
        if source is None:errors.append('missing source '+api['file'])
        elif not re.search(r'\b'+re.escape(api['symbol'])+r'\s*\(',source):errors.append('API absent '+api['symbol'])
    return errors

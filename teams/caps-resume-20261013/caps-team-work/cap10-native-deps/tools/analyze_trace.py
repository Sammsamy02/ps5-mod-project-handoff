#!/usr/bin/env python3
"""Summarize saved Mono/dyld output without executing the owner again."""
from pathlib import Path
import hashlib,json,re
BASE=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 outpath=BASE/'logs/OWNER-DEPENDENCY-TRACE.stdout.log';errpath=BASE/'logs/OWNER-DEPENDENCY-TRACE.stderr.log'
 out=outpath.read_text();err=errpath.read_text()
 images=re.findall(r'^dyld\[\d+\]: <[^>]+> (.+)$',err,re.M)
 opens=re.findall(r'^dyld\[\d+\]: dlopen\((.+)\)$',err,re.M)
 syms=re.findall(r'^dyld\[\d+\]: dlsym\([^,]+, "([^"]+)"\)',err,re.M)
 libraries=re.findall("DllImport attempting to load: '([^']+)'",out)
 symbols=re.findall("Searching for '([^']+)'",out)
 terms=['System.Native','libmono-native','SystemNative_','MonoPosixHelper','libmono-btls']
 result={'loaded_image_count_including_macos_transitive_frameworks':len(images),'loaded_local_experiment_images':[x for x in images if '/caps-team-work/' in x],'dlopen_calls':opens,'dlsym_unique_symbols':sorted(set(syms)),'mono_pinvoke_load_requests':libraries,'mono_pinvoke_symbols':symbols,'optional_native_component_mentions':[t for t in terms if t in out+err],'trace_hashes':{'stdout_sha256':sha(outpath),'stderr_sha256':sha(errpath)},'scope':'Only the exact executed owner/Probe flow on this Mac; not general C# API or target portability proof.'}
 (BASE/'evidence/DEPENDENCY-TRACE-SUMMARY.json').write_text(json.dumps(result,indent=2)+'\n')
 assert len(libraries)==1 and libraries[0].startswith('modbridge-owned:')
 assert set(symbols)=={'modbridge_cap10_abi_version','modbridge_cap10_runtime_probe'}
 assert not result['optional_native_component_mentions']
 print('SAVED_TRACE_ANALYSIS_PASS: only the two owned Probe P/Invokes; optional native components absent')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Offline source-handoff integrity and exclusion checks. Prints paths, never secrets."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
BAD_SUFFIX=set('.pak .utoc .ucas .uasset .umap .ubulk .uexp .bank .bnk .wem .elf .self .prx .sprx .dll .exe .dylib .so .a .o .obj .bin .log .jsonl .stdout .stderr .nm .zip .rar .7z .gz .tar .pem .key .p12 .pfx .crt .der'.split())
PATTERNS={
    'epic-source':re.compile(rb'Copyright[^\n\r]{0,100}Epic\s+Games',re.I),
    'private-key':re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----'),
    'github-token':re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,})\b'),
    'aws-key':re.compile(rb'\bAKIA[A-Z0-9]{16}\b'),
    'personal-home':re.compile(rb'/Users/[a-zA-Z][a-zA-Z0-9_-]+/'),
    'private-network-literal':re.compile(rb'(?<![0-9.])(?:192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})(?![0-9.])'),
}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--report',type=Path);args=ap.parse_args()
    failures=[];files=0;total=0;screens=0;seen=set()
    identifier_file=ROOT/'.local/privacy-machine-identifiers.json'
    private_identifiers=set(json.loads(identifier_file.read_text()).values()) if identifier_file.exists() else set()
    manifest=json.loads((ROOT/'handoff/SOURCE-MANIFEST.json').read_text())
    for row in manifest['files']:
        rel=row['path'];p=ROOT/rel
        if rel in seen: failures.append({'path':rel,'issue':'duplicate-manifest-entry'})
        seen.add(rel)
        if not p.is_file() or p.is_symlink(): failures.append({'path':rel,'issue':'missing-or-symlink'});continue
        data=p.read_bytes()
        if len(data)!=row['bytes'] or hashlib.sha256(data).hexdigest()!=row['sha256']:failures.append({'path':rel,'issue':'source-hash-mismatch'})
    for p in ROOT.rglob('*'):
        rel=p.relative_to(ROOT)
        if any(x in {'.git','.local','__pycache__'} for x in rel.parts):continue
        if p.is_symlink():failures.append({'path':str(rel),'issue':'symlink'});continue
        if not p.is_file():continue
        files+=1;data=p.read_bytes();total+=len(data)
        if p.suffix.lower() in BAD_SUFFIX:failures.append({'path':str(rel),'issue':'forbidden-extension'})
        if len(data)>95*1024*1024:failures.append({'path':str(rel),'issue':'github-file-size'})
        if p.suffix.lower() in {'.png','.jpg','.jpeg','.webp'}:
            if not str(rel).startswith('ui-tv/outputs/ps5-mod-bridge/docs/ui/'):failures.append({'path':str(rel),'issue':'unreviewed-image'})
            else:screens+=1
            continue
        if b'\x00' in data:failures.append({'path':str(rel),'issue':'binary-content'})
        for name,pattern in PATTERNS.items():
            if pattern.search(data):failures.append({'path':str(rel),'issue':name})
        if any(value.encode().lower() in data.lower() for value in private_identifiers if value):
            failures.append({'path':str(rel),'issue':'actual-computer-identifier'})
        if p.suffix=='.lua' and (b'TownfallItemSpawner' in data or b'Clean Nexus release build' in data):failures.append({'path':str(rel),'issue':'downloaded-mod-code'})
        if b'GIT binary patch' in data and p.suffix in {'.patch','.diff'}:failures.append({'path':str(rel),'issue':'binary-patch'})
    result={'passed':not failures,'files':files,'bytes':total,'manifest_sources':len(manifest['files']),'owned_ui_images':screens,'failures':failures,'console_contact':False,'scope':'Source hash/exclusion audit only; not a full build or target test.'}
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2));return 0 if not failures else 1

if __name__=='__main__':sys.exit(main())

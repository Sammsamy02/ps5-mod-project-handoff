#!/usr/bin/env python3
"""Render selected source copies using private local settings; no network operations."""
import argparse
import json
from pathlib import Path
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
COMPONENTS=('main','ui-tv','installer-compat','teams','legacy-port-source','sessions')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--component',choices=COMPONENTS,action='append',required=True)
    ap.add_argument('--config',type=Path,default=ROOT/'.local/config.json');args=ap.parse_args()
    config=json.loads(args.config.read_text()) if args.config.exists() else {}
    host=config.get('console_host','')
    if not isinstance(host,str) or not re.fullmatch(r'[A-Za-z0-9_.:\[\]-]*',host):raise SystemExit('Use a plain host name or address in local configuration.')
    workspace=ROOT/'.local/workspace'
    replacements={
        '/WORKSPACE/.codex/worktrees/translation-core/what':str(workspace/'main'),
        '/WORKSPACE/.codex/worktrees/translation-core/modbridge-tv-ui-20261013':str(workspace/'ui-tv'),
        '/WORKSPACE/Projects/ps5-installer-compat':str(workspace/'installer-compat'),
        '/WORKSPACE/Documents/Codex/2026-10-12/files-pasted-by-the-user-yes/outputs':str(workspace/'sessions/gpt-handoffs'),
    }
    for p in (ROOT/'teams').iterdir():
        if p.is_dir():replacements['/WORKSPACE/.codex/worktrees/translation-core/'+p.name]=str(workspace/'teams'/p.name)
    if host:replacements['ps5-host-not-configured.invalid']=host
    for component in args.component:
        target=workspace/component
        if target.exists():raise SystemExit('Local component already exists; preserve/review it before rendering again: '+component)
        shutil.copytree(ROOT/component,target)
        for p in target.rglob('*'):
            if not p.is_file() or p.suffix.lower() in {'.png','.jpg','.jpeg','.webp'}:continue
            try:s=p.read_text()
            except UnicodeDecodeError:continue
            changed=s
            for old,new in replacements.items():changed=changed.replace(old,new)
            if changed!=s:p.write_text(changed)
    print('Rendered '+', '.join(args.component)+' into gitignored .local/workspace/. No console contacted.')

if __name__=='__main__':main()

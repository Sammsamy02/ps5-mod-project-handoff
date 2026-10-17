"""Isolated destination and exact new source admission for combined local builds."""
import hashlib,json,os
from pathlib import Path
OWN=Path(__file__).resolve().parent
ROOT=OWN.parents[1]

def require_owned(path):
    p=Path(path).resolve()
    if not p.is_relative_to(OWN):raise ValueError('outside new owned packaging directory: '+str(p))
    return p

def pin(record):
    path=Path(record['path'])
    path=(path if path.is_absolute() else ROOT/path).resolve()
    if hashlib.sha256(path.read_bytes()).hexdigest()!=record['sha256']:
        raise ValueError('source/artifact pin drift: '+str(path))
    return path

def require_ready():
    manifest=json.loads((OWN/'BUILD-INPUTS.json').read_text())
    if manifest.get('scope')!='LOCAL_COMBINED_PACKAGE_ONLY':raise ValueError('missing exact local scope')
    for row in manifest['pins']:pin(row)
    selection=json.loads(pin(manifest['runtime_source_selection']).read_text())
    for role in ('source_pins','receipt_pins','artifact_pins'):
        for row in selection[role]:pin(row)
    for key in ('runtime_object','runtime_independent_twin_receipt','outer_entry','outer_bridge','import_evidence','import_check','package_script','controller'):
        pin(manifest[key])
    for row in manifest['lineage_source_pins']:pin(row)
    for row in manifest['outer_headers']:pin(row)
    for row in manifest['SDK_captures'].values():pin(row)
    for row in manifest['historical_receipts'].values():pin(row)
    if manifest.get('console_actions_authorized') is not False:raise ValueError('build manifest must not grant console actions')
    return manifest

def local_environment():
    env=dict(os.environ)
    for key,name in {'TMPDIR':'tmp','TMP':'tmp','TEMP':'tmp','DOTNET_CLI_HOME':'tool-env/dotnet',
                     'NUGET_PACKAGES':'tool-env/nuget','XDG_CACHE_HOME':'tool-env/cache'}.items():
        path=require_owned(OWN/name);path.mkdir(parents=True,exist_ok=True);env[key]=str(path)
    env.update(PYTHONDONTWRITEBYTECODE='1',DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',DOTNET_CLI_TELEMETRY_OPTOUT='1',
               DOTNET_NOLOGO='1',DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE='true',CCACHE_DISABLE='1')
    return env

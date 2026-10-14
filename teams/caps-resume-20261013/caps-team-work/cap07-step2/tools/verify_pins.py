"""Replay bounded saved windows/prefixes after streaming original-image SHA.
Does not independently rediscover symbol names, arbitrary references or a new game.
Explicit saved-image path required. Never used by the ordinary host suite.
"""
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import argparse,json,hashlib
H=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser();p.add_argument('--image',type=Path,required=True);a=p.parse_args()
profile=json.loads((H/'evidence/current-profile.json').read_text());base=json.loads((H/'evidence/saved-window-pass.json').read_text());extra=json.loads((H/'evidence/snapshot-windows.json').read_text())
assert a.image.stat().st_size==194241424
h=hashlib.sha256();total=0
with a.image.open('rb') as f:
 for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 assert h.hexdigest()==profile['image_sha256']
 def get(va,n):
  assert 0<n<=4096
  s=next(s for s in base['loads'] if s['vaddr']<=va and n<=s['filesz']-(va-s['vaddr']))
  f.seek(s['offset']+va-s['vaddr']);b=f.read(n);assert len(b)==n;return b
 for row in profile['proofs']:
  b=get(int(row['linked'],16),row['size']);assert b.hex()==row['bytes'];total+=len(b)
 windows=base['windows']+extra['windows']
 for w in windows:
  b=get(int(w['va'],16),w['length']);assert hashlib.sha256(b).hexdigest()==w['sha256'];total+=len(b)
assert total and total<16384
print('PASS:6 prefixes,',len(windows),'saved windows;',total,'bounded selected bytes; whole-image SHA matches; no new ABI/target proof')

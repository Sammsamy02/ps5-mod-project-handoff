#!/usr/bin/env python3
"""Pinned public Mono loader dependency source patch; no build/apply."""
import sys
sys.dont_write_bytecode=True
import hashlib,json,difflib
from pathlib import Path
BASE=Path(__file__).resolve().parents[1]
SOURCE=BASE/'dependencies/mono-source/mono/utils/mono-dl.c'
SOURCE_SHA='b7ccf30b3828f0fb64cf07afda22f574f00b6f665b17faeb9f9b7fb3df8f9e8e'
CHANGES=[(b'\tlib = mono_dl_open_file (name, lflags);',b'\t/* Reserved owned names never consult the ambient OS loader. */\n\tlib = (name && strstr (name, "modbridge-owned:")) ? NULL : mono_dl_open_file (name, lflags);'),(b'\tif (!lib && !dl_fallback) {\n\t\tchar *lname;',b'\t/* Includes path/prefix/suffix variants constructed by the native library search. */\n\tif (!lib && !dl_fallback && name && strstr (name, "modbridge-owned:")) {\n\t\tif (error_msg) *error_msg = g_strdup ("Owned library has no registered provider");\n\t\tg_free (module);\n\t\treturn NULL;\n\t}\n\tif (!lib && !dl_fallback) {\n\t\tchar *lname;'),(b'\tg_slist_remove (fallback_handlers, handler);',b'\tfallback_handlers = g_slist_remove (fallback_handlers, handler);')]
def transform(raw):
 if hashlib.sha256(raw).hexdigest()!=SOURCE_SHA:raise ValueError('Mono source hash drift/reapplication')
 for before,after in CHANGES:
  if raw.count(before)!=1 or after in raw:raise ValueError('Mono anchor not exact-once')
 for before,after in CHANGES:raw=raw.replace(before,after)
 return raw
def inverse(raw):
 for before,after in reversed(CHANGES):
  if raw.count(after)!=1:raise ValueError('inverse Mono anchor not exact-once')
  raw=raw.replace(after,before)
 if hashlib.sha256(raw).hexdigest()!=SOURCE_SHA:raise ValueError('inverse Mono hash mismatch')
 return raw
def main():
 raw=SOURCE.read_bytes();new=transform(raw)
 if inverse(new)!=raw or SOURCE.read_bytes()!=raw:raise ValueError('source instability')
 (BASE/'native/mono-owned-loader.patch').write_text(''.join(difflib.unified_diff(raw.decode().splitlines(True),new.decode().splitlines(True),fromfile='original/mono/utils/mono-dl.c',tofile='staged/mono/utils/mono-dl.c')))
 print(json.dumps({'source_sha256':SOURCE_SHA,'derived_sha256':hashlib.sha256(new).hexdigest(),'anchors':3,'exact_inverse':True,'applied':False},indent=2))
if __name__=='__main__':main()

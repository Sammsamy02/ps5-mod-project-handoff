"""Read-only C++ lexical API inventory, not a C++ parser or ABI/field verifier."""
import argparse, hashlib, json, re
from pathlib import Path

def blank_literals(text):
    # Preserve every newline for actionable references; raw C++ literals are not parsed.
    pattern=r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern,lambda m:re.sub(r'[^\n]',' ',m.group()),text,flags=re.S)

def scan_text(text, path):
    clean=blank_literals(text); found=[]
    patterns=[('imgui',r'\bImGui::(\w+)\s*\('),
              ('hook',r'\b((?:Register\w*Callback|UnregisterCallback))\b'),
              ('accessor_candidate',r'(?:->|\.)(Get\w+|Has\w+|Is\w+)\s*\('),
              ('data_table_write_candidate',r'\b(\w+)\s*\[[^\]\n]+\]\s*=(?!=)')]
    for family,pattern in patterns:
        for m in re.finditer(pattern,clean):
            found.append({'family':family,'symbol':m.group(1),'path':path,'line':clean.count('\n',0,m.start())+1,'_pos':m.start()})
    found.sort(key=lambda x:x['_pos'])
    for row in found:del row['_pos']
    return found

def inventory(root):
    files=sorted(p for p in Path(root).rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.h','.inl'))
    if not files:raise ValueError('zero source files checked')
    result={'label':'Mac-only lexical inventory; accessor types/fields unresolved; no ABI proof','files_checked':len(files),'files':{},'calls':[]}
    for p in files:
        rel=str(p.relative_to(root));data=p.read_bytes();result['files'][rel]=hashlib.sha256(data).hexdigest();result['calls']+=scan_text(data.decode('utf8'),rel)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source');a=p.parse_args()
    try:print(json.dumps(inventory(Path(a.source)),indent=2))
    except ValueError as exc:p.exit(2,str(exc)+'\n')

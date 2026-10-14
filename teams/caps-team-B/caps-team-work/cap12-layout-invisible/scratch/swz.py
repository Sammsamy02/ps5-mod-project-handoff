"""Parse public Mesa addrlib GFX10 swizzle tables (text) into address-bit equations."""
import re
def load(path):
    s=open(path).read()
    tabs={}
    for m in re.finditer(r'const UINT_64 (GFX10_SW_PATTERN_NIBBLE\w+)\[\]\[\d\]\s*=\s*\{(.*?)\n\};', s, re.S):
        rows=[]
        for line in m.group(2).splitlines():
            mm=re.match(r'\s*\{(.*)\}\s*,\s*//\s*(\d+)', line)
            if mm:
                ent=[x.strip() for x in mm.group(1).split(',') if x.strip()!='']
                assert int(mm.group(2))==len(rows)
                rows.append(ent)
        tabs[m.group(1)]=rows
    pats={}
    for m in re.finditer(r'const ADDR_SW_PATINFO (GFX10_SW_\w+_PATINFO)\[\]\s*=\s*\{(.*?)\n\};', s, re.S):
        rows=[]
        for line in m.group(2).splitlines():
            mm=re.match(r'\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*\}\s*,\s*//\s*(.*)$', line)
            if mm: rows.append((tuple(int(mm.group(i)) for i in range(1,6)), mm.group(6).strip()))
        pats[m.group(1)]=rows
    return tabs,pats
def equation(tabs, info):
    _,n01,n2,n3,n4=info
    return (tabs['GFX10_SW_PATTERN_NIBBLE01'][n01]+tabs['GFX10_SW_PATTERN_NIBBLE2'][n2]
            +tabs['GFX10_SW_PATTERN_NIBBLE3'][n3]+tabs['GFX10_SW_PATTERN_NIBBLE4'][n4])
def find(pats, table, comment_re):
    out=[(i,c) for i,c in pats[table] if re.search(comment_re, c)]
    return out

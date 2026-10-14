import sys
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent))
import catch_inventory as ci
ov=sys.argv[1]
files=ci.file_set(); texts=ci.load_texts(files,ov)
funcs,raising,sites,h2=ci.analyze(texts)
for s in sorted(sites,key=lambda s:(s['file'],s['line'])):
    if s['class'] in('all','void*') and s['lua_in_try'] and not s['rethrows'] and not s['void_pass_first'] and 'candidate' in s['file']:
        print(s['file'].split('candidate/')[-1], s['line'], s['enclosing'], s['handler_sha8'], 'pcall' if s.get('try_is_pcall') else '', '|', ' ; '.join(map(str,s['lua_in_try'][:4]))[:300])

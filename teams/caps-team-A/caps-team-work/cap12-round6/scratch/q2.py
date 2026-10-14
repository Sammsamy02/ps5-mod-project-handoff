import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graph import *
for t in sys.argv[1:]:
    hits=[a['package'] for a in reg.assets if a['package'].endswith('/'+t)]
    for p in hits:
        ib=sorted((x.split('/')[-1],cls(x)) for x in impby.get(p,()) if cls(x)!='LevelSequence')
        print('==',p,cls(p),'imported by (non-sequence):',ib, ' + sequences:', sum(1 for x in impby.get(p,()) if cls(x)=='LevelSequence'))

# Builds the self-test table (items 1..N) from a decoded UE4SS log. Read-only.
import re,sys
L=open(sys.argv[1],encoding='utf-8',errors='replace').read().split('\n')
print('| # | test | result | detail | time | log line |')
print('|---|---|---|---|---|---|')
for i,l in enumerate(L,1):
    m=re.match(r'\[2026-10-12 (\S+)\] \[Lua\] \[ST2\] (\d+)/168 (\S+) (PASS|FAIL|SKIP)\s*(.*)$',l)
    if not m: continue
    t,n,name,res,det=m.groups()
    sm=re.search(r'SLOW-ms=(\d+)',det); det2=re.sub(r'\s*SLOW-ms=\d+','',det).replace('|','/')
    tm=('**%s ms (SLOW)**'%sm.group(1)) if sm else '<=16 ms'
    r=res if not sm else res+' + SLOW'
    print('| %s | %s | %s | %s | %s | %d |'%(n,name,r,det2[:60],tm,i))

import sys, os, json, struct
sys.dont_write_bytecode=True
T='/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-textures/PATCH/tools'
sys.path.insert(0,T)
import utoc, zen
M='/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P'
toc=utoc.load(M+'.utoc')
uc=open(M+'.ucas','rb')
def rd(o,n): uc.seek(o); return uc.read(n)
bypath={}
for row in toc.rows_of_type(1):
    p=toc.files.get(row); bypath[p.split('/Content/')[1].rsplit('.',1)[0]]=row
def fix(z):
    d=z.data
    (iph, imp, exp, bundle, dep_h, dep_e, ipn, cimp, cexp) = struct.unpack_from('<9i', d, 24)
    for i,e in enumerate(z.exports):
        soff,ssize=struct.unpack_from('<QQ',d,exp+72*i)
        e['data_offset']=z.header_size+soff
    return z
def pkg(name):
    return fix(zen.ZenPackage(toc.read_chunk(bypath[name], rd)))
if __name__=='__main__':
    for n in sys.argv[1:]:
        z=pkg(n)
        print('==',n,'names',len(z.names)); print(z.names[:400])
        print('imported',z.imported_packages)
        print('imports',z.imports)
        for e in z.exports: print(e)

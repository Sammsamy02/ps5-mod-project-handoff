import sys
sys.dont_write_bytecode = True
exec(open(sys.argv[1]).read().split("ANY_VF =")[0])
for n, z in sorted(pk.items()):
    if len(z.exports) > 1:
        print(len(z.exports), cls_of(n), n.rsplit('/', 1)[-1])

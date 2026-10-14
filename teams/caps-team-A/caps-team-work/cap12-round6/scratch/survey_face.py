import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'survey.py')).read().split('for n in sorted(bypath)')[0])
for n in sorted(bypath):
    if 'FaceMesh' in n: run(n)

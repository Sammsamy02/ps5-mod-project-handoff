"""Fail-closed declared API coverage gate. Not runtime compatibility evidence."""
import argparse,json

def assess(required, coverage):
    names=sorted(set(required));missing=[x for x in names if coverage.get(x)!='implemented']
    return {'passed':bool(names) and not missing,'required_count':len(names),'missing':missing,'label':'Mac-only declared coverage; implementation claims require independent evidence'}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('inventory');p.add_argument('coverage');a=p.parse_args()
    inv=json.load(open(a.inventory));coverage=json.load(open(a.coverage))
    names=[row['symbol'] for row in inv['calls'] if row['family']=='imgui']
    result=assess(names,coverage);print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else (1 if names else 2))

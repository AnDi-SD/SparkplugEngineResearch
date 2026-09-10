#!/usr/bin/env python3
"""Cache an explicitly selected family once, using only catalogs/native ledger."""
import argparse,json,re,shutil,sqlite3
from pathlib import Path
from capture_native_ranges import ROOT,capture


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('family');parser.add_argument('--roots',nargs='*',default=[]);parser.add_argument('--classes',nargs='*',default=[])
    args=parser.parse_args()
    if not re.fullmatch('[a-z0-9]+(?:-[a-z0-9]+)*',args.family) or not(args.roots or args.classes):raise ValueError('Explicit safe family and class selection required')
    cycle=ROOT/'local-data/results/native-cycle-20260910-1900';p=cycle/args.family
    if p.exists():raise ValueError('Existing family catalog/evidence must not be overwritten')
    cats={k:{r['class_name']:r for r in json.loads((cycle/f'catalog/{k}-architecture.json').read_text())['registered_types']} for k in ('pc','ps2')}
    selected=set(args.classes)|set(args.roots);known=set(cats['pc'])|set(cats['ps2'])
    if not selected<=known:raise ValueError('Unknown exact class names: '+str(selected-known))
    descendants=set(args.roots)
    while True:
        nxt=descendants|{r['class_name'] for cat in cats.values() for r in cat.values() if r['base_class_name'] in descendants}
        if nxt==descendants:break
        descendants=nxt
    selected|=descendants
    if len(selected)>64:raise ValueError('Maximum64 explicit family class names per cache')
    db=sqlite3.connect((ROOT/'local-data/results/smo-corpus-v2.sqlite').as_uri()+'?mode=ro',uri=True);rows=[];ranges=[]
    for n in sorted(selected):
        a=cats['pc'].get(n);b=cats['ps2'].get(n);c=a or b
        if a and b:assert a['class_hash']==b['class_hash'] and a['base_class_name']==b['base_class_name'],n
        prior=[dict(platform=k,score=s) for k,s in db.execute('select a.platform_key,a.score from native_platform_assessments a join native_types t on t.id=a.native_type_id where t.class_name=?',(n,))]
        row=dict(className=n,classHash=c['class_hash'],base=c['base_class_name'],pcFactory=a['constructor_arg5_va'] if a else None,ps2Factory=b['constructor_t1_va'] if b else None,pcRecord=a['registration_object_va'] if a else None,ps2Record=b['registration_object_va'] if b else None,priorAssessments=prior);rows.append(row)
        for k in cats:
            if row[k+'Factory']:ranges.append(dict(name=k+'-'+n,platform=k,address=row[k+'Factory'],size=0x100))
    p.mkdir();(p/'catalog-family.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    (p/'selection.json').write_text(json.dumps(vars(args),indent=2)+'\n',encoding='utf-8')
    if ranges:capture(dict(ranges=ranges),p/'factories')
    snap=p/'source-snapshot';snap.mkdir()
    for n in ('prepare_native_class_family.py','probe_pc_ai_action_lifecycle.py','probe_pc_task_timer.py','probe_pc_animation_lifecycle.py','pc_instruction_emulator.py','pc_block_emulator.py','capture_native_ranges.py','inspect_executable_architecture.py','ps2_scalar_prefix.py','pc_crt_string_fixtures.py'):shutil.copy2(ROOT/'research'/n,snap/n)
    print(json.dumps(dict(classCount=len(rows),rows=rows),indent=2))


if __name__=='__main__':main()

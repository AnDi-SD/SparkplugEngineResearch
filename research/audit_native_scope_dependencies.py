#!/usr/bin/env python3
"""Read-only queue-to-workflow audit; no inferred binary edges or automatic credit.

Every registered class explicitly placed in a workflow work item must be in the
versioned denominator on that platform. This catches bookkeeping omissions,
not undiscovered binary dependencies. No EXE/resource scan or database writes.
"""
import argparse
import hashlib
import json
import sqlite3
import time
from native_workbench import DATABASE,WORK_ITEMS,validate_config

def audit_dependencies(config,scope,catalog):
    members={name for group in scope['groups'] for name in group['classes']}
    edges={}
    for item in config['items']:
        for name in item['classes']:edges.setdefault((item['platform'],name),set()).add(item['id'])
    missing=[];invalid=[]
    for (platform,name),items in sorted(edges.items()):
        row=dict(platform=platform,className=name,workItems=sorted(items))
        if name not in catalog:invalid.append(dict(row,reason='not_registered'))
        elif platform not in catalog[name]:invalid.append(dict(row,reason='absent_on_platform'))
        elif name not in members:missing.append(row)
    return dict(scopeId=scope['scopeId'],checkedClassPlatformPairs=len(edges),
        missingScopeMembers=missing,invalidCatalogReferences=invalid,
        passed=not missing and not invalid,
        limitation='Audits explicitly recorded work items only; no proof of complete reachable binary inventory or class behavior.')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--scope');parser.add_argument('--json',action='store_true');args=parser.parse_args()
    config=validate_config(json.loads(WORK_ITEMS.read_text(encoding='utf-8')))
    with sqlite3.connect(DATABASE.resolve().as_uri()+'?mode=ro',uri=True) as db:
        limit=time.monotonic()+5;db.set_progress_handler(lambda:int(time.monotonic()>limit),10000)
        row=db.execute('SELECT manifest_json FROM native_goal_scopes WHERE scope_id=?',(args.scope,)).fetchone() if args.scope else db.execute('SELECT manifest_json FROM native_goal_scopes ORDER BY created_utc DESC LIMIT 1').fetchone()
        if not row:raise ValueError('Requested workflow scope is not imported')
        scope=json.loads(row[0]);catalog={name:({'pc'} if pc else set())|({'ps2'} if ps2 else set()) for name,pc,ps2 in db.execute('SELECT class_name,on_pc,on_ps2 FROM native_types')}
    result=audit_dependencies(config,scope,catalog)
    for key,value in (('configurationSha256',config),('scopeSha256',scope)):
        result[key]=hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    if args.json:print(json.dumps(result,ensure_ascii=False,indent=2))
    else:
        print(f"{'PASS' if result['passed'] else 'GAP'} {result['scopeId']}: {result['checkedClassPlatformPairs']} queue class/platform pairs; missingScope={len(result['missingScopeMembers'])}, invalidCatalog={len(result['invalidCatalogReferences'])}")
        for row in result['missingScopeMembers']+result['invalidCatalogReferences']:print(json.dumps(row,ensure_ascii=False))
        print(result['limitation'])
    return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

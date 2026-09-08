#!/usr/bin/env python3
"""Read-only class dossiers/dependency queue plus explicit bounded regression profiles.

No binary/corpus rescan for dossiers. No inferred names, evidence credit or source
generation. Profiles use fresh bounded Python children, never launch the game.
Parallel execution is opt-in and requires declared independent profile entries.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
DATABASE = ROOT / 'local-data/results/smo-corpus-v2.sqlite'
WORK_ITEMS = ROOT / 'research/native-work-items.json'
CHILD_TIMEOUT = 30
MAX_WORKERS = 4
ESTIMATED_CHILD_MIB = 192
ESTIMATED_CONTROLLER_MIB = 64
MEMORY_BUDGET_MIB = 1024


def validate_config(config):
    if config['schemaVersion'] != 1:
        raise ValueError('Unknown work-item schema')
    items = {item['id']: item for item in config['items']}
    if len(items) != len(config['items']):
        raise ValueError('Duplicate work item')
    for item in items.values():
        if item['platform'] not in ('pc', 'ps2'):
            raise ValueError('Explicit platform required')
        if item.get('planningStatus', 'active') not in ('active', 'deferred'):
            raise ValueError('Unknown planning status')
        for unknown in item['unknowns']:
            if unknown['kind'] not in ('behavior', 'name', 'path', 'abi'):
                raise ValueError('Unknown gap kind')
        for dep in item['dependsOn']:
            if dep not in items or items[dep]['platform'] != item['platform']:
                raise ValueError('Missing or cross-platform dependency')
            if (item.get('planningStatus', 'active') == 'active'
                    and items[dep].get('planningStatus', 'active') == 'deferred'):
                raise ValueError('Active work item depends on deferred work')
        for profile in item['testProfiles']:
            if profile not in config['testProfiles']:
                raise ValueError('Missing test profile')
            if any(test['platform'] != item['platform'] for test in config['testProfiles'][profile]):
                raise ValueError('Cross-platform test profile')
        for value in item['evidencePaths']:
            path = (ROOT / value).resolve()
            if not path.is_relative_to(ROOT) or not path.is_file():
                raise ValueError('Invalid work-item evidence path')
    def visit(identity, chain):
        if identity in chain:
            raise ValueError('Dependency cycle')
        for dep in items[identity]['dependsOn']:
            visit(dep, chain | {identity})
    for identity in items:
        visit(identity, set())
    identities = set()
    for tests in config['testProfiles'].values():
        for test in tests:
            path = (ROOT / test['script']).resolve()
            if not path.is_relative_to(ROOT / 'research') or path.suffix != '.py' or not path.is_file():
                raise ValueError('Only repository research Python profiles allowed')
            if test['platform'] not in ('pc', 'ps2') or not all(isinstance(arg, str) for arg in test['args']):
                raise ValueError('Invalid test platform/arguments')
            if 'parallelSafe' in test and not isinstance(test['parallelSafe'],bool):
                raise ValueError('Explicit boolean parallelSafe declaration required')
            if test['id'] in identities:
                raise ValueError('Duplicate test id')
            identities.add(test['id'])
    return config


def dossier(db, config, name, platform):
    if platform not in ('pc', 'ps2'):
        raise ValueError('Explicit platform required')
    row = db.execute('SELECT * FROM native_types WHERE class_name=?', (name,)).fetchone()
    if row is None or not row['on_' + platform]:
        raise ValueError(f'{name} is absent from {platform} catalog')
    result = dict(className=name, platform=platform, classHash=f"0x{row['type_hash']:08X}",
                  owner=row['owner_scope'], registration=row[platform + '_registration_locator'])
    parent = db.execute('SELECT class_name FROM native_types WHERE type_hash=? AND on_' + platform + '=1',
                        (row[platform + '_base_hash'],)).fetchall()
    result['baseCandidates'] = [entry[0] for entry in parent]
    result['scopes'] = [dict(entry) for entry in db.execute(
        'SELECT scope_key,object_count,resource_count,evidence_status,provenance FROM native_type_scopes WHERE native_type_id=?', (row['id'],))]
    result['scopeCountMode'] = 'shared_project_inventory_not_platform_filtered'
    tables = {entry[0] for entry in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
    assessment = None
    if 'native_platform_assessments' in tables:
        assessment = db.execute('SELECT * FROM native_platform_assessments WHERE native_type_id=? AND platform_key=?',
                                (row['id'], platform)).fetchone()
    result['platformAssessment'] = dict(assessment) if assessment else None
    result['platformEvidenceRefs'] = json.loads(assessment['evidence_json']) if assessment else []
    result['legacyMixedAssessment'] = next((dict(entry) for entry in db.execute(
        'SELECT * FROM native_research_progress WHERE native_type_id=?', (row['id'],))), None)
    result['evidence'] = [dict(entry) for entry in db.execute(
        "SELECT platform_key,evidence_kind,source_path,locator,observation,confidence FROM native_research_evidence WHERE native_type_id=? AND platform_key IN (?, 'common') ORDER BY id",
        (row['id'], platform))]
    result['workItems'] = [item for item in config['items'] if item['platform'] == platform and name in item['classes']]
    result['warnings'] = ['Common evidence is listed for context, never automatically converted to platform credit.',
                          'Scope object/resource counts come from the shared project inventory, not a PC-only or PS2-only file count.',
                          'No work item means backlog not yet structured, not a fully known class.',
                          'Registration locators are copied as labelled from the catalog; not assumed to be registration object addresses.']
    return result


def queue(db, config, platform, *, include_deferred=False):
    if platform not in ('pc', 'ps2'):
        raise ValueError('Explicit platform required')
    direct = {r[0] for r in db.execute("SELECT t.class_name FROM native_types t JOIN native_type_scopes s ON s.native_type_id=t.id WHERE s.scope_key='smo_san' AND t.on_" + platform + '=1')}
    items = [dict(item, directConsumers=sorted(direct.intersection(item['classes'])))
             for item in config['items'] if item['platform'] == platform
             and (include_deferred or item.get('planningStatus', 'active') == 'active')]
    # Investigation order is dependency-aware, without requiring a partially
    # studied dependency to be100% before independently testing its consumer.
    # Priority alone could put a new mesh item before its loader prerequisite.
    pending={item['id']:item for item in items};ordered=[]
    while pending:
        ready=[item for item in pending.values() if not any(dep in pending for dep in item['dependsOn'])]
        if not ready:raise ValueError('Dependency cycle in platform queue')
        selected=min(ready,key=lambda item:(item['priority'],-len(item['directConsumers']),item['id']))
        ordered.append(selected);del pending[selected['id']]
    return ordered


def run_profile(config, profile, deadline=None, report_path=None, *, workers=1):
    if type(workers) is not int or not 1<=workers<=MAX_WORKERS:
        raise ValueError('Bounded worker count must be1..4')
    tests=config['testProfiles'][profile]
    if workers>1 and any(test.get('parallelSafe') is not True for test in tests):
        raise ValueError('Every parallel child needs an audited parallelSafe declaration')
    if workers*ESTIMATED_CHILD_MIB+ESTIMATED_CONTROLLER_MIB>MEMORY_BUDGET_MIB:
        raise ValueError('Worker admission estimate exceeds aggregate research budget')
    results = []
    started=datetime.now(timezone.utc).isoformat()
    configuration_hash=hashlib.sha256(json.dumps(config,sort_keys=True,separators=(',',':')).encode('utf-8')).hexdigest()
    def save(status, code=None):
        if report_path is None:return
        document=dict(schemaVersion=2,kind='bounded-native-profile-run',profile=profile,
                      startedUtc=started,updatedUtc=datetime.now(timezone.utc).isoformat(),
                      deadlineUtc=deadline.isoformat() if deadline else None,
                      status=status,exitCode=code,expectedChildren=len(config['testProfiles'][profile]),
                      completedChildren=len(results),childTimeoutSeconds=CHILD_TIMEOUT,results=results,
                      configurationSha256=configuration_hash,
                      workers=workers,memoryBudgetMiB=MEMORY_BUDGET_MIB,
                      memoryAdmissionEstimateMiB=workers*ESTIMATED_CHILD_MIB+ESTIMATED_CONTROLLER_MIB,
                      memoryAdmissionIsOSLimit=False,
                      configurationHashMethod='sha256-of-startup-json-sort-keys-compact-ensure-ascii',
                      notes='Exit results and script hashes only; NOT class coverage, native startup, binary completeness or live display proof.')
        # Generated diagnostic artifact; callers establish exclusive ownership.
        report_path.write_text(json.dumps(document,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    save('running')
    def execute(test):
        start=time.monotonic()
        print(f"RUN {test['platform']} {test['id']} (fresh child; {CHILD_TIMEOUT}s cap)",flush=True)
        try:
            child=subprocess.run([sys.executable,'-B',str(ROOT/test['script']),*test['args']],
                                 cwd=ROOT,timeout=CHILD_TIMEOUT)
            code=child.returncode
        except subprocess.TimeoutExpired:
            code=124 # subprocess.run kills and waits for this direct child.
        return dict(id=test['id'],platform=test['platform'],exitCode=code,
                    script=test['script'],args=test['args'],
                    scriptSha256=hashlib.sha256((ROOT/test['script']).read_bytes()).hexdigest(),
                    elapsedSeconds=round(time.monotonic()-start,3))
    # Fixed waves preserve deterministic report order and stop admission after
    # any failed wave. Already running siblings finish under their original cap.
    for offset in range(0,len(tests),workers):
        if deadline and (deadline - datetime.now(timezone.utc)).total_seconds() < CHILD_TIMEOUT + 1:
            print('STOP: insufficient time before report deadline', flush=True)
            save('deadline',3)
            return 3
        wave=tests[offset:offset+workers]
        if workers==1:completed=[execute(wave[0])]
        else:
            with ThreadPoolExecutor(max_workers=workers) as pool:completed=list(pool.map(execute,wave))
        results.extend(completed)
        for row in completed:print(json.dumps(row),flush=True)
        code=next((row['exitCode'] for row in completed if row['exitCode']),0)
        if code:
            save('failed',code)
            return code
        save('running')
    print(f'PASS profile {profile}: {len(results)}/{len(results)} bounded children')
    save('passed',0)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('dossier', 'queue', 'run'))
    parser.add_argument('target', nargs='?')
    parser.add_argument('--platform', choices=('pc', 'ps2'), default='pc')
    parser.add_argument('--database', type=Path, default=DATABASE)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--include-deferred', action='store_true',
                        help='Include long-term engine backlog in queue output')
    parser.add_argument('--deadline-utc', help='UTC ISO timestamp, e.g.2026-09-06T09:00:00Z')
    parser.add_argument('--workers',type=int,choices=range(1,MAX_WORKERS+1),default=1,
                        help='Independent fresh children per wave; audited profiles only, default1')
    args = parser.parse_args()
    config = validate_config(json.loads(WORK_ITEMS.read_text(encoding='utf-8')))
    if args.command == 'run':
        if args.target not in config['testProfiles']:
            parser.error('A known test profile is required')
        if any(test['platform'] != args.platform for test in config['testProfiles'][args.target]):
            parser.error('Profile belongs to a different platform')
        deadline = datetime.fromisoformat(args.deadline_utc.replace('Z', '+00:00')) if args.deadline_utc else None
        if deadline and deadline.tzinfo is None:
            parser.error('Deadline must include UTC offset')
        report_dir=ROOT/'local-data/results/bounded-native-runs'
        report_dir.mkdir(parents=True,exist_ok=True)
        stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        report_path=report_dir/f'{stamp}-{args.target}.json'
        with report_path.open('x',encoding='utf-8') as report:report.write('{}\n')
        print(f'RUN REPORT {report_path.relative_to(ROOT)}',flush=True)
        return run_profile(config, args.target, deadline,report_path,workers=args.workers)
    db = sqlite3.connect(args.database.resolve().as_uri() + '?mode=ro', uri=True)
    db.row_factory = sqlite3.Row
    try:
        if args.command == 'dossier':
            if not args.target:
                parser.error('A class name is required')
            result = dossier(db, config, args.target, args.platform)
        else:
            result = queue(db, config, args.platform, include_deferred=args.include_deferred)
    finally:
        db.close()
    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    elif args.command == 'queue':
        for item in result:
            print(f"P{item['priority']} {item['id']} | direct consumers {len(item['directConsumers'])} | {item['status']} | {item.get('planningStatus', 'active')}")
            print('  ' + item['summary'])
        if not result:
            print('No independently structured work items for this platform; not a claim of no remaining work.')
    else:
        print(f"# {result['className']} — {args.platform.upper()} — {result['classHash']}\n")
        print(f"Catalog registration: {result['registration']}; base candidates: {result['baseCandidates']}\n")
        print('Platform assessment: ' + (str(result['platformAssessment']['score']) + '%' if result['platformAssessment'] else 'UNRATED'))
        for item in result['workItems']:
            print(f"\n## {item['id']}\n\n{item['summary']}\n")
            for gap in item['unknowns']:
                print(f"- [{gap['kind']}] {gap['text']}")
        print('\n## Evidence\n')
        for evidence in result['platformEvidenceRefs']:
            print(f"- [platform assessment/{evidence['platformKey']}] {evidence['sourcePath']} | {evidence['locator']} | {evidence['observation']}")
        for evidence in result['evidence']:
            print(f"- [{evidence['platform_key']}/{evidence['confidence']}] {evidence['source_path']} | {evidence['locator']} | {evidence['observation']}")
        print('\n' + '\n'.join(result['warnings']))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

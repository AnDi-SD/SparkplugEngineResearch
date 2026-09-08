"""Read-only, evidence-backed readiness of explicitly scoped tool operations.

No executable/corpus scan and no class-score conversion. Only all three passed
stages with unchanged referenced files and no blockers count as a ready operation.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SCOPE = ROOT / 'research/tool-readiness-scope-v1.json'
STAGES = ('contract', 'integration', 'validation')
STATUSES = {'unknown', 'partial', 'passed'}
LOCAL_ONLY = {'local-data', '.codex-tmp', '.git', 'artifacts'}
TEXT_SUFFIXES = {'.md', '.json', '.py', '.cs', '.cpp', '.h', '.xaml', '.axaml', '.csproj', '.txt'}
MAX_EVIDENCE_BYTES = 8 * 1024 * 1024


def sha256(data):
    return hashlib.sha256(data).hexdigest().upper()


def load_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def validate_scope(scope):
    if scope.get('kind') != 'tool-readiness-scope' or scope.get('schemaVersion') != 1:
        raise ValueError('Unknown readiness scope schema')
    if not scope.get('scopeId') or not scope.get('features'):
        raise ValueError('Scope and nonempty feature list required')
    seen = set()
    for feature in scope['features']:
        identity = feature.get('id')
        if not identity or identity in seen:
            raise ValueError('Duplicate or missing feature ID')
        seen.add(identity)
        if not all(feature.get(key) for key in ('title', 'group', 'tools', 'acceptance')):
            raise ValueError('Feature needs title, group, tools and acceptance')
        if not isinstance(feature['tools'], list) or not all(isinstance(t, str) for t in feature['tools']):
            raise ValueError('Feature tools must be a nonempty string list')
        if 'weight' in feature:
            raise ValueError('Operation weights are fixed at one')
    return scope


def latest_assessment(scope_path, root=ROOT):
    scope_bytes = Path(scope_path).read_bytes()
    scope = validate_scope(json.loads(scope_bytes))
    candidates = []
    for path in (root / 'research').glob('tool-readiness-assessment-*.json'):
        value = load_json(path)
        if value.get('scopeId') == scope['scopeId'] and value.get('scopeSha256') == sha256(scope_bytes):
            candidates.append((value.get('assessedUtc', ''), path))
    if not candidates:
        raise ValueError('No assessment matching this exact scope')
    return max(candidates)[1]


def build_report(scope, assessment, scope_hash, *, root=ROOT):
    validate_scope(scope)
    if assessment.get('kind') != 'tool-readiness-assessment' or assessment.get('schemaVersion') != 1:
        raise ValueError('Unknown readiness assessment schema')
    if assessment.get('scopeId') != scope['scopeId'] or assessment.get('scopeSha256') != scope_hash:
        raise ValueError('Assessment belongs to a different scope/version')
    instant = datetime.fromisoformat(assessment['assessedUtc'].replace('Z', '+00:00'))
    if instant.tzinfo is None:
        raise ValueError('Assessment time must have a UTC offset')
    allowed = {f['id'] for f in scope['features']}
    entries = {}
    hashes = {}
    for entry in assessment.get('assessments', []):
        identity = entry.get('featureId')
        if identity not in allowed or identity in entries:
            raise ValueError('Unknown or duplicate assessment feature ID')
        if set(entry.get('stages', {})) != set(STAGES):
            raise ValueError('Every assessment needs all three stages')
        if not isinstance(entry.get('blockers', []), list):
            raise ValueError('Blockers must be a list')
        entries[identity] = entry

    def check_reference(reference):
        name = reference.get('path', '')
        relative = Path(name)
        path = (root / relative).resolve()
        if (not name or relative.is_absolute() or not path.is_relative_to(root.resolve())
                or path.relative_to(root.resolve()).parts[0].lower() in LOCAL_ONLY
                or path.suffix.lower() not in TEXT_SUFFIXES):
            raise ValueError('Evidence must be a repository text artifact')
        expected = reference.get('sha256', '')
        if len(expected) != 64 or any(c not in '0123456789ABCDEFabcdef' for c in expected):
            raise ValueError('Exact evidence SHA256 required')
        if not reference.get('observation'):
            raise ValueError('Evidence observation required')
        if path not in hashes:
            if not path.is_file():
                hashes[path] = None
            elif path.stat().st_size > MAX_EVIDENCE_BYTES:
                raise ValueError('Evidence file exceeds bounded text-artifact size')
            else:
                with path.open('rb') as source:
                    data = source.read(MAX_EVIDENCE_BYTES + 1)
                if len(data) > MAX_EVIDENCE_BYTES:
                    raise ValueError('Evidence file grew beyond bounded size')
                hashes[path] = sha256(data)
        return hashes[path] == expected.upper()

    features = []
    for feature in scope['features']:
        entry = entries.get(feature['id'])
        stages = {}
        stale = []
        for stage in STAGES:
            value = entry['stages'][stage] if entry else {'status': 'unknown', 'evidence': []}
            status = value.get('status')
            if status not in STATUSES:
                raise ValueError('Unknown stage status')
            evidence = value.get('evidence', [])
            if status == 'passed' and not evidence:
                raise ValueError('Passed stage needs explicit evidence')
            changed = [ref['path'] for ref in evidence if not check_reference(ref)]
            stale.extend(changed)
            stages[stage] = dict(status='needs_review' if changed else status,
                                 recordedStatus=status, evidenceCount=len(evidence))
        blockers = entry.get('blockers', []) if entry else []
        ready = all(stages[s]['status'] == 'passed' for s in STAGES) and not blockers
        state = ('ready' if ready else 'needs_review' if stale else 'blocked' if blockers
                 else 'in_progress' if entry else 'unassessed')
        features.append(dict(id=feature['id'], title=feature['title'], group=feature['group'],
                             tools=feature['tools'], acceptance=feature['acceptance'],
                             limits=feature.get('limits', []), stages=stages, state=state,
                             blockers=blockers, staleEvidence=sorted(set(stale)),
                             assessmentOrigin=entry.get('origin') if entry else None))
    ready = sum(row['state'] == 'ready' for row in features)
    return dict(kind='tool-readiness-report', schemaVersion=1, scopeId=scope['scopeId'],
                scopeSha256=scope_hash, assessedUtc=assessment['assessedUtc'],
                assessmentId=assessment['assessmentId'],
                readyCount=ready, totalCount=len(features), readyPercent=100 * ready / len(features),
                stagePassedCounts={s: sum(row['stages'][s]['status'] == 'passed' for row in features) for s in STAGES},
                stateCounts=dict(Counter(row['state'] for row in features)), features=features,
                evidenceFilesRead=len(hashes),
                note='Equal-weight readiness of named operations and stated variants only. Not class/instruction coverage, remaining-time estimate or proof of all application behavior. Historical evidence carried into the baseline is not new cycle progress; changed evidence requires review.')


def compare_baseline(report, baseline):
    if (baseline.get('kind') != 'tool-readiness-report'
            or baseline.get('scopeSha256') != report['scopeSha256']
            or {r['id'] for r in baseline['features']} != {r['id'] for r in report['features']}):
        raise ValueError('Cannot compare different readiness scopes')
    before = {r['id'] for r in baseline['features'] if r['state'] == 'ready'}
    after = {r['id'] for r in report['features'] if r['state'] == 'ready'}
    return dict(readyCountDelta=len(after) - len(before), newlyReady=sorted(after - before),
                noLongerReady=sorted(before - after),
                percentagePointDelta=100 * (len(after) - len(before)) / report['totalCount'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('report', 'record'), nargs='?', default='report')
    parser.add_argument('--scope', type=Path, default=DEFAULT_SCOPE)
    parser.add_argument('--assessment', type=Path)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--details', action='store_true')
    args = parser.parse_args()
    scope_bytes = args.scope.read_bytes()
    path = args.assessment or latest_assessment(args.scope)
    assessment_bytes = path.read_bytes()
    report = build_report(json.loads(scope_bytes), json.loads(assessment_bytes), sha256(scope_bytes))
    report['assessmentManifestSha256'] = sha256(assessment_bytes)
    report['generatedUtc'] = datetime.now(timezone.utc).isoformat()
    if args.baseline:
        report['cycleDelta'] = compare_baseline(report, load_json(args.baseline))
    if args.command == 'record':
        directory = ROOT / 'local-data/results/tool-readiness-snapshots'
        directory.mkdir(parents=True, exist_ok=True)
        target = directory / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '.json')
        with target.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        report['snapshotPath'] = target.relative_to(ROOT).as_posix()
    if args.json:
        print(json.dumps(report, ensure_ascii=True, indent=2))
    else:
        print(f"{report['scopeId']}: {report['readyCount']}/{report['totalCount']} ready ({report['readyPercent']:.2f}%)")
        print('Stages:', report['stagePassedCounts'], '| states:', report['stateCounts'])
        print('Evidence as of:', report['assessedUtc'])
        if args.details:
            for row in report['features']:
                print(row['state'], row['id'], '|', '; '.join(row['blockers'] + row['staleEvidence']))
        if 'cycleDelta' in report:
            print('Cycle:', report['cycleDelta'])
        if 'snapshotPath' in report:
            print('Snapshot:', report['snapshotPath'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

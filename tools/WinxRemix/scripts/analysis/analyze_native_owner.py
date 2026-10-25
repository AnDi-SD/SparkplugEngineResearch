"""Summarize a closed operation-scoped native ownership audit."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from analyze_native_draw import records
from analyze_native_mesh_source import exited

REASONS = ('no_scope', 'callsite', 'scene', 'registry', 'support', 'model', 'mesh', 'world', 'retired')
COUNTERS = ('supportCalls', 'queueSupportCalls', 'modelCalls', 'modelWithoutMesh', 'modelFailures',
            'captures', 'qualified', 'used', 'sceneRetirements', 'nodeRetirements', 'renderNodeRetirements')


def analyze(run):
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    exited(launch['pid'])
    path = run / 'native-owner-source.jsonl'
    before = path.stat()
    events, totals, rejections, histogram, contexts, owners, retire_kinds = (Counter() for _ in range(7))
    scenes, roots, mutations, scopes = set(), set(), set(), set()
    samples, multiple_models, multiple_registrations, materials_differ, repeated_submissions = 0, 0, 0, 0, 0
    seen_calls = set()
    first = last = init = None
    retirements = []
    for row in records(path):
        kind = row['event']; events[kind] += 1
        if kind == 'init':
            if init is not None:
                raise ValueError('Duplicate owner initialization')
            init = row
        elif kind == 'frame':
            frame = row['frame']
            if last is not None and frame <= last:
                raise ValueError('Non-monotonic owner frames')
            if first is None:
                first = frame
            last = frame
            if len(row['rejected']) != len(REASONS) or row['used'] > row['qualified']:
                raise ValueError('Inconsistent owner counters/schema')
            totals.update({k: row.get(k, 0) for k in COUNTERS})
            rejections.update(dict(zip(REASONS, row['rejected'])))
            histogram[row['used']] += 1
        elif kind == 'submit':
            samples += 1
            for name, output in (('scene', scenes), ('root', roots), ('mutation', mutations), ('sceneScope', scopes)):
                output.add(row[name])
            if not row['registrations'] or not row['modelOccurrences'] or len(row['worldBits']) != 16:
                raise ValueError('Incomplete owner relationship')
            key = (row['frame'], row['submission'])
            repeated_submissions += key in seen_calls
            seen_calls.add(key)
            contexts['support_scope' if row['supportCall'] else 'outside_support_scope'] += 1
            owners[row.get('ownerKind', 'legacy_static_partition')] += 1
            multiple_models += row['modelOccurrences'] > 1
            multiple_registrations += row['registrations'] > 1
            materials_differ += row['modelMaterial'] != row['selectedMaterial']
        elif kind == 'retire':
            retirements.append(row)
            retire_kinds[row['kind']] += 1
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('Owner log changed during analysis')
    if not init or not init['enabled']:
        raise ValueError('Native owner hooks were not enabled')
    return dict(schema=1, run=str(run), pidExited=True, proxySha256=launch['proxySha256'],
                sourceLogSha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=before.st_size,
                logLimitReached=before.st_size >= init['maxLogBytes'], frameRange=[first, last],
                events=dict(events), totals=dict(totals), rejected=dict(rejections),
                ownerFrameHistogram=dict(sorted(histogram.items())), sampledSubmits=samples,
                sampledContexts=dict(contexts), sampledSceneAddresses=sorted(scenes),
                sampledOwnerKinds=dict(owners), retirementEventCounts=dict(retire_kinds),
                sampledRootAddresses=sorted(roots), sampledMutationSerials=sorted(mutations),
                sampledSceneScopes=len(scopes), sampledRepeatedModelMembership=multiple_models,
                sampledRepeatedOwnerRegistrations=multiple_registrations,
                sampledModelSelectedMaterialDifferences=materials_differ, retirements=retirements,
                sampledRepeatedNativeSubmissions=repeated_submissions,
                scope='Exact static/partition and inherited RenderNode ownership at original Model own mesh callsite, joined to successful existing native API submits. Operation serials and borrowed addresses are not durable instance identities. Snapshot mutation serial observes known Scene/PartitionNode/RenderNode retirement only; arbitrary graph changes and persistent offscreen submission remain open. Models without mesh counts calls without a qualified native mesh entry, not a decoded skip reason. Retirement events after last Present are counted separately from frame counters.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.run)
    with args.output.open('x', encoding='utf-8') as output:
        json.dump(result, output, indent=2)
    print(json.dumps(result))

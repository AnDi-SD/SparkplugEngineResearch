"""Capture the reviewed local bridge patch and closed CPU/build evidence only."""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import difflib
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = Path(__file__).resolve().parent
WORK = ROOT / 'local-data/rtx-remix/direct-camera-bridge-work'
REFERENCE = ROOT / 'local-data/rtx-remix/upstream/dxvk-remix'
EVIDENCE = ROOT / 'local-data/rtx-remix/skinning-bridge'
OUTPUT_DIR = ROOT / '.private/evidence/tool-builds/WinxRemix/skinning-bridge'
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUTPUT = OUTPUT_DIR / ('capture-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '.json')

BASE = 'b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def git(*args):
    return subprocess.check_output(['git', '-C', str(WORK), *args])


def patch_file(relative, before=''):
    after = (WORK / relative).read_text(encoding='utf-8')
    return ''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                    fromfile='a/' + relative if before else '/dev/null',
                    tofile='b/' + relative)).encode()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tests', required=True)
    ap.add_argument('--build', required=True)
    ap.add_argument('--camera-tests', default='skinwire-camera-v1')
    args = ap.parse_args()
    if git('rev-parse', 'HEAD').decode().strip() != BASE:
        raise SystemExit('Unexpected source revision')
    test_dir = EVIDENCE / args.tests
    build_dir = EVIDENCE / args.build
    camera_dir = ROOT / 'local-data/rtx-remix/direct-camera-bridge-tests' / args.camera_tests
    tests = json.loads((test_dir / 'result.json').read_text(encoding='utf-8-sig'))
    build = json.loads((build_dir / 'result.json').read_text(encoding='utf-8-sig'))
    camera = json.loads((camera_dir / 'result.json').read_text(encoding='utf-8-sig'))
    if tests['status'] != 'PASS' or build['status'] != 'BUILT_NOT_INSTALLED_OR_RUN':
        raise SystemExit('Completed passing tests and build required')
    if not camera['identicalWire'] or any(r['exitCode'] for r in camera['results']):
        raise SystemExit('Camera serializer regression must pass')
    for item in tests['sourceHashes']:
        if sha(WORK / item['path']) != item['sha256']:
            raise SystemExit('Source changed since CPU tests: ' + item['path'])
    full = git('diff', 'HEAD', '--binary')
    for relative in ('bridge/src/server/instance_audit.h',
                     'bridge/test/rtx/unit/test_remix_api_skinning.cpp'):
        full += patch_file(relative)
    full_path = PACKAGE / 'skinning-wire-v1-full.patch'
    full_path.write_bytes(full)
    incremental = b''
    own_files = ('bridge/meson.build', 'bridge/src/util/util_remixapi.cpp',
                 'bridge/src/util/util_remixapi.h', 'bridge/src/client/remix_api.cpp',
                 'bridge/src/server/main.cpp', 'bridge/test/rtx/unit/meson.build')
    for relative in own_files:
        before = (EVIDENCE / 'before-v1' / relative).read_text(encoding='utf-8')
        incremental += patch_file(relative, before)
    incremental += patch_file('bridge/test/rtx/unit/test_remix_api_skinning.cpp')
    incremental_path = PACKAGE / 'skinning-wire-v1-incremental.patch'
    incremental_path.write_bytes(incremental)
    subprocess.run(['git', '-C', str(REFERENCE), 'apply', '--check', str(full_path)], check=True)
    clean = subprocess.check_output(['git', '-C', str(REFERENCE), 'status', '--porcelain']).decode()
    if clean.strip():
        raise SystemExit('Read-only reference is not clean')
    artifacts = []
    for directory in (*sorted(p for p in EVIDENCE.iterdir() if p.is_dir()), camera_dir):
        for path in sorted(directory.rglob('*')):
            if path.is_file():
                artifacts.append({'path': path.relative_to(ROOT).as_posix(),
                                  'bytes': path.stat().st_size, 'sha256': sha(path)})
    package_files = [{'path': path.relative_to(ROOT).as_posix(), 'sha256': sha(path)}
                     for path in sorted(PACKAGE.iterdir())
                     if path.is_file() and path.name != 'manifest.json']
    report = {
        'status': 'CPU_SERIALIZER_PASS_PAIR_BUILT_NOT_INSTALLED_OR_GPU_TESTED',
        'provenance': 'Own universal bridge correction; no recovered game logic or stock renderer changed',
        'baseRevision': BASE,
        'stockRendererRevision': '68edea01; not present in local shallow reference',
        'wireVersion': 'remix-main-skinwire-v1',
        'fullPatch': {'path': full_path.relative_to(ROOT).as_posix(), 'sha256': sha(full_path),
                      'appliesToReadOnlyReference': True},
        'incrementalPatch': {'path': incremental_path.relative_to(ROOT).as_posix(),
                             'sha256': sha(incremental_path),
                             'base': 'before-v1 own camera + server-audit source snapshot'},
        'referenceWorktreeClean': True,
        'compatibility': 'Matching client/server mandatory: existing startup version equality rejects old/new pair',
        'bounds': {
            'wireBytes': 'uint32 envelope; checked sums and products before serialization',
            'mesh': 'Mesh pNext null; required arrays; total element counts equal vertices*bonesPerVertex; bonesPerVertex>0 when skinning present',
            'bonePalette': '0..256 transforms, pointer required for nonempty array, exact payload length',
            'instanceExtensions': 'At most64 traversed nodes, at most1 BoneTransforms; other existing supported routing unchanged',
            'callerMemory': 'Pointers must address readable stable arrays for the duration of the API call',
        },
        'malformed': 'Client preflight before command; server validates mesh/palette payload before allocation; malformed mesh drains UID, malformed/duplicate palette drains Instance extensions and skips renderer call',
        'ownership': 'Typed new[]/delete[] for surfaces, vertices, indices, weights, bone indices and palette; zero-initialized partial decode ownership; generic client serializer temporary now delete[]',
        'readOnlyReview': {
            'reviewer': '/root/native_source_review/owner_checkpoint_review',
            'verdict': 'No concrete blockers in the changed client/server wrappers and partial ownership',
            'scope': 'Preflight before ClientMessage, malformed mesh UID drain, invalid/duplicate bone drain and skip, typed-array initialization/deletion; no reviewer builds or GPU',
        },
        'limitations': [
            'DrawInstance/CreateMesh client SUCCESS remains enqueue success; no added ACK',
            'Renderer API success is EmitCs acceptance, not GPU completion or deformation proof',
            'Local renderer source weight stride=4 and implicit final weight require actual stock B2 fixture; this patch does not alter renderer',
            'No finite/normalization/index-value or native bone matrix derivation policy added',
            'No general malformed-command or allocation-failure recovery refactor; caller/stable-buffer contract remains',
        ],
        'tests': tests,
        'cameraRegression': camera,
        'build': build,
        'historicalAttempts': [p.relative_to(ROOT).as_posix() for p in sorted(EVIDENCE.iterdir())
                               if p.is_dir() and p not in (test_dir, build_dir)],
        'packageFiles': package_files,
        'artifacts': artifacts,
    }
    (OUTPUT).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'manifestSha256': sha(OUTPUT),
                      'artifacts': len(artifacts), 'fullPatchSha256': sha(full_path)}))


if __name__ == '__main__':
    main()

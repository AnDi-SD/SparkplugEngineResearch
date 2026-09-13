"""Capture an existing, built camera bridge. Does not build or install it."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
WORK = ROOT / 'local-data/rtx-remix/direct-camera-bridge-work'
REFERENCE = ROOT / 'local-data/rtx-remix/upstream/dxvk-remix'
OUTPUT_DIR = ROOT / '.private/evidence/tool-builds/WinxRemix/direct-camera-bridge'
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
OUTPUT = OUTPUT_DIR / ('capture-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '.json')

BASE = 'b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4'
def sha(data):
    return hashlib.sha256(data).hexdigest().upper()
def git(path, *args):
    return subprocess.run(['git', '-C', str(path), *args], check=True, capture_output=True).stdout
def file_entry(path):
    data = path.read_bytes()
    return {'path': path.relative_to(ROOT).as_posix(), 'bytes': len(data), 'sha256': sha(data)}

assert git(WORK, 'rev-parse', 'HEAD').decode().strip() == BASE
assert git(REFERENCE, 'rev-parse', 'HEAD').decode().strip() == BASE
assert not git(REFERENCE, 'status', '--porcelain')
patch = HERE / 'camera-v1.patch'
git(REFERENCE, 'apply', '--check', str(patch))
git(WORK, 'apply', '--reverse', '--check', str(patch))
names = git(WORK, 'diff', '--name-only', '--', 'bridge').decode().splitlines()
def command_ids(text):
    block = text.split('enum D3D9Command : uint16_t {', 1)[1].split('\n  };', 1)[0]
    result = {}; value = -1
    for name, expression in re.findall(r'^\s*(\w+)\s*(?:=\s*([^,]+))?,', block, re.MULTILINE):
        value = (65535 if 'max()' in expression else int(expression, 0)) if expression else value + 1
        result[name] = value
    return result
old = command_ids((REFERENCE / 'bridge/src/util/util_commands.h').read_text())
new = command_ids((WORK / 'bridge/src/util/util_commands.h').read_text())
assert all(new[name] == value for name, value in old.items())
result = {
    'status': 'Built; cross-architecture serializer, isolated live IPC/moving-camera Present, and terminal transport-fault cleanup validated; NOT installed in game',
    'provenance': 'Own universal bridge transport extension, not reconstructed game logic; MIT upstream patch',
    'baseRevision': BASE,
    'stockBinaryRevision': '68edea01 (not present in local shallow reference)',
    'bridgeVersion': 'remix-main-camera-v1+b81a7b56',
    'patchSha256': sha(patch.read_bytes()),
    'patchAppliesToReadOnlyReference': True,
    'referenceWorktreeClean': True,
    'existingCommandIdsPreserved': len(old),
    'cameraCommandId': new['RemixApi_SetupCamera'],
    'wire': {'bytes': 140, 'fields': 'uint32 byteSize, uint32 sType, uint32 cameraType, view32bits[16], projection32bits[16]', 'pNext': 'No extensions; client rejects non-null and decoder reconstructs null', 'validation': 'Exact outer/inner size, camera sType, known type, finite float32; output untouched on rejection'},
    'result': 'Client waits for mandatory UID-matched server response using existing ack timeout. Normal API errors are consumed and returned. A transport wait failure after send logs UID/result then unconditionally terminates the host process with 0xE052CA01; no fallback or recovery. SUCCESS is the real renderer API return, not proof GPU consumed the exact queued camera.',
    'build': {'msvc': '19.51.36257', 'sdk': '10.0.26100.0', 'meson': '1.9.2', 'ninja': '1.13.0', 'platforms': ['x86', 'x64'], 'workers': 1, 'configuration': 'release', 'detoursRevision': 'ea6c4ae7f3f1b1772b8a7cda4199230b932f5a50'},
    'patchedFiles': [],
    'artifacts': [],
}
for name in names:
    data = (WORK / name).read_bytes()
    result['patchedFiles'].append({'path': name, 'sha256': sha(data), 'lfSha256': sha(data.replace(b'\r\n', b'\n'))})
for path in [
    ROOT / 'local-data/rtx-remix/direct-camera-bridge-build/x86/src/client/d3d9.dll',
    ROOT / 'local-data/rtx-remix/direct-camera-bridge-build/x64/src/server/NvRemixBridge.exe',
    ROOT / 'local-data/rtx-remix/direct-camera-bridge-build/x86/test/rtx/unit/test_remix_api_camera_x86.exe',
    ROOT / 'local-data/rtx-remix/direct-camera-bridge-build/x64/test/rtx/unit/test_remix_api_camera_x64.exe',
    *sorted((ROOT / 'local-data/rtx-remix/direct-camera-bridge-tests/cross-arch-v3').iterdir()),
]:
    if path.is_file(): result['artifacts'].append(file_entry(path))
test_path = ROOT / 'local-data/rtx-remix/direct-camera-bridge-tests/cross-arch-v3/result.json'
tests = json.loads(test_path.read_text(encoding='utf-8-sig'))
assert len(tests['results']) == 4 and all(entry['exitCode'] == 0 for entry in tests['results'])
assert tests['identicalWire']
result['tests'] = tests
result['tests']['scope'] += '; 214+219+214+219=866 checks in the final run'
live_base = ROOT / 'local-data/rtx-remix/direct-camera-live'
result['live'] = {}
for name in ['positive-v2', 'fault-v1', 'fault-v2', 'fault-v3', 'fault-v4']:
    directory = live_base / name
    report = json.loads((directory / 'result.json').read_text())
    assert not report['jobAfterCleanup']
    assert report['elapsedSeconds'] < 30
    result['live'][name] = report
    for path in [directory / 'result.json', directory / 'launch.json', directory / 'helper.jsonl', directory / 'prepared.json', *sorted((directory / 'rtx-remix/logs').glob('*.log'))]:
        result['artifacts'].append(file_entry(path))
closed_path=live_base/'closed-evidence-v1.json'
closed=json.loads(closed_path.read_text())
assert len(closed['cases'])==3 and all(c['nativeVerified'] for c in closed['cases'])
result['liveNativeValidation']=closed
result['artifacts'].append(file_entry(closed_path))
result['testHarness'] = [file_entry(HERE / name) for name in ['live_camera.cpp','Prepare-Live.ps1','Run-Live.py','Test-LiveEvidence.py']]
result['historicalEvidence'] = ['history/initial-camera-v1.patch', 'history/initial-build-manifest.json', 'positive-v1: 1 GiB commit cap failure; later measured peak 2.76 GB', 'fault-v1 and fault-v2: correct native terminal exit; original wrapper failed reading trace before Windows released its handle, saved native evidence independently verified after cleanup', 'fault-v3: cold CreateDevice startup exceeded client timeout; fault-v4: cold bridge startup exceeded handshake timeout; neither reached fault injection; original failed reports preserved']
result['notVerified'] = ['Exact renderer-accepted matrix values or GPU pixel motion readback; helper orders API camera before first draw but does not instrument renderer update', 'Game native camera selection, main/UI/offscreen interaction, cuts/history and level/device lifecycle', 'Full compatibility of reference-based rebuilt bridge with stock68edea01 renderer beyond the isolated tested calls; matching stock source commit is unavailable']
(OUTPUT).write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'manifest': str(OUTPUT), 'patchFiles': len(names), 'preservedCommands': len(old), 'cameraCommandId': new['RemixApi_SetupCamera'], 'artifacts': len(result['artifacts'])}, indent=2))

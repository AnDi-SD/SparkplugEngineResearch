"""Apply the owned external asset identity patch to its exact pinned baseline."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def lf_hash(path):
    return hashlib.sha256(path.read_bytes().replace(b'\r\n', b'\n')).hexdigest()


def run(source, *args):
    result = subprocess.run(['git', '-C', str(source), '-c', 'core.autocrlf=false',
                             *args], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or 'Git command failed')
    return result.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true', help='validate without changing the source')
    mode.add_argument('--apply', action='store_true', help='apply after validating all baseline inputs')
    args = parser.parse_args()
    package = Path(__file__).resolve().parents[3] / 'patches/renderer-resource-identity'
    source = args.source.resolve(strict=True)
    manifest = json.loads((package / 'manifest.json').read_text(encoding='utf-8'))
    patch = package / manifest['patch']
    if hashlib.sha256(patch.read_bytes()).hexdigest() != manifest['patchSha256']:
        raise ValueError('Patch digest differs from the manifest')
    if run(source, 'rev-parse', 'HEAD') != manifest['pin']:
        raise ValueError('Renderer HEAD differs from the pinned revision')
    for entry in manifest.get('prerequisiteFiles', []):
        target = source / entry['path']
        if not target.is_file() or lf_hash(target) != entry['lfSha256']:
            raise ValueError('Required USD stack differs: ' + entry['path'])
    for entry in manifest['sourceFiles']:
        relative = Path(entry['path'])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Manifest path is not relative to the renderer source')
        target = source / relative
        before = entry['beforeLfSha256']
        if before is None:
            if target.exists():
                raise ValueError('New patch file already exists: ' + entry['path'])
        elif not target.is_file() or lf_hash(target) != before:
            raise ValueError('Baseline differs (apply the documented USD stack first): ' + entry['path'])
    run(source, 'apply', '--check', str(patch))
    if args.apply:
        run(source, 'apply', str(patch))
        for entry in manifest['sourceFiles']:
            if lf_hash(source / entry['path']) != entry['afterLfSha256']:
                raise ValueError('Applied output differs: ' + entry['path'])
    print(json.dumps({'status': 'PASS', 'operation': 'apply' if args.apply else 'check',
                      'pin': manifest['pin'], 'patchSha256': manifest['patchSha256'],
                      'files': len(manifest['sourceFiles']), 'rendererBuilt': False}))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Package the checked WinxCoop Ready builds without rebuilding native binaries."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / 'tools/WinxCoop'
MACHINES = {'i386': 0x014C, 'amd64': 0x8664}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pe_machine(data: bytes) -> int:
    if data[:2] != b'MZ' or len(data) < 64:
        raise ValueError('Expected a PE image')
    offset, = struct.unpack_from('<I', data, 0x3C)
    if data[offset:offset + 4] != b'PE\0\0':
        raise ValueError('Missing PE header')
    return struct.unpack_from('<H', data, offset + 4)[0]


def documents(manifest: dict) -> dict[str, bytes]:
    docs = {f'docs/{name}': (ROOT / name).read_bytes() for name in manifest['legalDocuments']}
    tag = manifest['tag']
    version = manifest['version']
    source = f'https://github.com/{manifest["repository"]}/tree/{tag}'
    for suffix, source_label, notes_label in (
        ('.md', 'Исходный код', 'Заметки к выпуску'),
        ('.en.md', 'Source code', 'Release notes'),
    ):
        notes_name = f'RELEASE_NOTES_{version}{suffix}'
        guide = f'passive-companion{suffix}'
        readme = (TOOL / f'PACKAGE_README{suffix}').read_text(encoding='utf-8')
        readme = readme.replace(f'](docs/{guide})', f']({guide})')
        readme += f'\n[{notes_label}]({notes_name}) · [{source_label}]({source}) · [MPL-2.0](LICENSE.md)\n'
        docs[f'docs/README{suffix}'] = readme.encode('utf-8')
        docs[f'docs/{guide}'] = (TOOL / 'docs' / guide).read_bytes()
        notes = (TOOL / notes_name).read_text(encoding='utf-8')
        docs[f'docs/{notes_name}'] = notes.replace(f'](docs/{guide})', f']({guide})').encode('utf-8')
    for path, data in docs.items():
        for link in re.findall(r'\]\(([^)]+)\)', data.decode('utf-8-sig')):
            target = link.split('#', 1)[0]
            if target and '://' not in target and str(Path(path).parent / target).replace('\\', '/') not in docs:
                raise ValueError(f'Unresolved package link: {path} → {target}')
    return docs


def package(ready: Path, output: Path, manifest: dict, verified: dict[str, dict]) -> list[tuple[str, str]]:
    docs = documents(manifest)
    checksums = []
    for platform, machine in manifest['platforms'].items():
        name = f'WinxCoop-{manifest["version"]}-{platform}'
        entries = dict(docs)
        for binary in manifest['binaries']:
            relative = f'{platform}/{binary}'
            data = (ready / relative).read_bytes()
            expected = verified[relative]
            if digest(data) != expected['sha256'].lower() or len(data) != expected['bytes']:
                raise ValueError(f'Checked binary changed: {relative}')
            expected_machine = MACHINES[machine if binary == 'WinxCoopLauncher.exe' else 'i386']
            if pe_machine(data) != expected_machine:
                raise ValueError(f'Wrong architecture: {relative}')
            entries[binary] = data

        archive_path = output / f'{name}.zip'
        with ZipFile(archive_path, 'x', compression=ZIP_DEFLATED) as archive:
            for path, data in sorted(entries.items()):
                member = ZipInfo(f'{name}/{path}')
                archive.writestr(member, data, compress_type=ZIP_DEFLATED, compresslevel=9)
        with ZipFile(archive_path) as archive:
            expected_paths = {f'{name}/{path}' for path in entries}
            if set(archive.namelist()) != expected_paths or len(archive.infolist()) != len(expected_paths):
                raise ValueError(f'Unexpected ZIP members: {archive_path}')
            for path, data in entries.items():
                if archive.read(f'{name}/{path}') != data:
                    raise ValueError(f'ZIP readback differs: {archive_path}: {path}')
        checksums.append((digest(archive_path.read_bytes()), archive_path.name))
        print(f'{archive_path.name}: {len(entries)} files, {archive_path.stat().st_size} bytes')
    return checksums


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ready', type=Path, required=True)
    parser.add_argument('--verified-manifest', type=Path, required=True, help='SHA-256 inventory of the session-checked Ready files')
    parser.add_argument('--output', type=Path, required=True, help='new output directory')
    args = parser.parse_args()
    manifest = json.loads(Path(__file__).with_name('winxcoop-manifest.json').read_text(encoding='utf-8'))
    verified = json.loads(args.verified_manifest.read_text(encoding='utf-8-sig'))
    args.output.mkdir(parents=True, exist_ok=False)
    checksums = package(args.ready, args.output, manifest, {item['path']: item for item in verified['files']})
    (args.output / 'SHA256SUMS.txt').write_text(''.join(f'{sha}  {name}\n' for sha, name in checksums), encoding='ascii')


if __name__ == '__main__':
    main()

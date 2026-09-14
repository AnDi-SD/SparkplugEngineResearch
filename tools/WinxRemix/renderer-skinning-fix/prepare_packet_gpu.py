"""Freeze bounded SKP1 inputs and the unchanged pinned Remix compute fixture."""
from pathlib import Path
import argparse, json, re, shutil, struct
from prepare_cpu import ROOT, PACKAGE, PIN, digest
from prepare_gpu import prepare_gpu


def prepare_packets(destination, runs):
    names = runs.split(',')
    if not 1 <= len(names) <= 8 or len(set(names)) != len(names):
        raise ValueError('Choose one to eight distinct capture runs')
    entries, journals, total = [], [], 0
    for name in names:
        if not re.fullmatch(r'[a-zA-Z0-9_-]+', name):
            raise ValueError('Run names must be local directory names')
        folder = (ROOT / 'local-data/rtx-remix/runs' / name / 'skin-packets').resolve()
        folder.relative_to(ROOT.resolve())
        journal = folder / 'packets.jsonl'
        if journal.stat().st_size > 1024 * 1024:
            raise ValueError('Packet journal exceeds 1 MiB')
        content = journal.read_bytes()
        journals.append((name, content))
        seen = set()
        for line in content.decode('utf-8-sig').splitlines():
            event = json.loads(line)
            if event.get('event') != 'packet':
                continue
            file = event['file']
            if not re.fullmatch(r'packet-[0-9]{4}\.skp', file) or file in seen:
                raise ValueError('Invalid or duplicate packet filename')
            seen.add(file)
            original = (folder / file).resolve()
            original.relative_to(folder)
            size = original.stat().st_size
            total += size
            if not 32 <= size <= 8 * 1024 * 1024 or total > 64 * 1024 * 1024 or len(entries) >= 128:
                raise ValueError('Packet count/byte budget exceeded')
            data = original.read_bytes()
            magic, version, wire, vertices, indices, influences, bones, attributes = struct.unpack_from('<8I', data)
            if (magic, version, wire) != (0x31504B53, 1, len(data)) or size != len(data):
                raise ValueError('Packet header mismatch')
            if not (0 < vertices <= 65536 and 0 < indices <= 3 * 32768 and indices % 3 == 0
                    and 1 <= influences <= 4 and 1 <= bones <= 16 and attributes <= 3):
                raise ValueError('Packet field bound')
            if wire != 32 + bones * 48 + vertices * 80 + indices * 4:
                raise ValueError('Packet extent mismatch')
            if any(event[key] != expected for key, expected in [('bytes', wire), ('vertices', vertices),
                    ('triangles', indices // 3), ('influences', influences), ('bones', bones)]):
                raise ValueError('Packet journal/header mismatch')
            entries.append({'ordinal': len(entries) + 1, 'run': name, 'file': file,
                'source': original.relative_to(ROOT.resolve()).as_posix(), 'sha256': digest(data),
                'bytes': wire, 'vertices': vertices, 'indices': indices, 'influences': influences,
                'bones': bones, 'captureEvent': event})
        if not seen:
            raise ValueError('No captured packets in ' + name)
    prepare_gpu(destination)
    for relative in ['tools/WinxRemix/winx_skin_packet.h', 'tools/WinxRemix/winx_skin_packet_remix.h',
                     'tools/WinxRemix/winx_skin_packet_gpu.h',
                     'Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h']:
        target = destination / 'source' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / relative, target)
    for name in ['test_gpu_skin_packets.cpp', 'prepare_packet_gpu.py', 'Test-GPUPackets.ps1']:
        shutil.copy2(PACKAGE / name, destination / name)
    (destination / 'packets').mkdir()
    (destination / 'journals').mkdir()
    for name, content in journals:
        (destination / 'journals' / (name + '.jsonl')).write_bytes(content)
    for entry in entries:
        data = (ROOT / entry['source']).read_bytes()
        if digest(data) != entry['sha256']:
            raise ValueError('Input changed during snapshot')
        entry['snapshot'] = 'packets/packet-%04d.skp' % entry['ordinal']
        (destination / entry['snapshot']).write_bytes(data)
    metadata = {'schema': 1, 'pin': PIN, 'packetCount': len(entries), 'packetBytes': total,
        'capturedVertices': sum(p['vertices'] for p in entries),
        'expectedDispatches': 56 + sum(4 * ((p['vertices'] + 4095) // 4096) for p in entries),
        'syntheticCases': 13,
        'variants': ['stock-strides', 'corrected-strides', 'baked-identity-kernel-control', 'signed-palette-zero-remainder'],
        'scope': 'All packet vertices: actual pinned CPU header and unchanged GPU SPIR-V versus shared Fixed bake. Baked API layout uses an identity kernel control; no unskinned API draw, material, bridge or full renderer validation.',
        'packets': entries,
        'files': {p.relative_to(destination).as_posix(): digest(p.read_bytes())
                  for p in sorted(destination.rglob('*')) if p.is_file()}}
    (destination / 'packet-gpu-source.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--runs', required=True, help='Comma-separated local capture directory names')
    args = parser.parse_args()
    prepare_packets(args.destination.resolve(), args.runs)

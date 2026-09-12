"""Own read-only shader coverage analysis; outputs stay in the local run directory.

DXSO equality proves transport to the stock renderer. SPIR-V existence proves
translation, not correct ray-traced capture or visual equivalence. Snapshots
cover all game draws through the last snapshot, not an entire playthrough.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pc_shader_sdk import D3dx


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def analyze(run: Path, game: Path):
    sdk = D3dx()
    launch = json.loads((run / 'launch.json').read_text(encoding='utf-8-sig'))
    backend = launch['environment']['WINX_REMIX_BACKEND']
    output = run / 'shader-analysis'
    output.mkdir(exist_ok=True)
    inventory = []
    assembly_sources = {}
    for path in sorted((game / 'Shaders').iterdir()):
        if not path.is_file():
            continue
        source = path.read_bytes()
        record = dict(file=path.name, bytes=len(source), sha256=digest(source))
        if path.suffix.lower() in ('.vsh', '.psh'):
            hr, code, errors = sdk.assemble(source)
            record.update(assemblyHr=hr, assemblyMessages=errors)
            if code:
                disassembly, normalized = sdk.disassemble(code)
                (output / (path.name + '.assembled.bin')).write_bytes(code)
                (output / (path.name + '.asm')).write_text(disassembly, encoding='utf-8')
                record['assembledSha256'] = digest(code)
                assembly_sources.setdefault(normalized, []).append(path.name)
        elif path.suffix.lower() == '.rfx':
            record['records'] = [
                {k: e.attrib.get(k) for k in ('NAME', 'TYPE', 'PIXEL_SHADER', 'TARGET', 'ENTRY_POINT')}
                for e in ET.fromstring(source).iter() if 'Shader' in e.tag and 'CODE' in e.attrib
            ]
        inventory.append(record)

    events = [json.loads(line) for line in (run / 'shaders-client/audit.jsonl').read_text().splitlines() if line.strip()]
    snapshots = [e for e in events if e['event'] == 'snapshot']
    if not snapshots:
        raise RuntimeError('No completed frame snapshots; coverage is not established')
    latest = snapshots[-1]
    servers = {}
    for path in sorted((run / 'shaders-server').glob('*.dxso')):
        servers.setdefault(digest(path.read_bytes()), []).append(path)
    shaders = []
    for event in (e for e in events if e['event'] == 'shader'):
        path = run / f'shaders-client/shader-{event["id"]:04}.bin'
        code = path.read_bytes()
        disassembly, normalized = sdk.disassemble(code)
        (output / (path.stem + '.asm')).write_text(disassembly, encoding='utf-8')
        matches = servers.get(digest(code), [])
        stage = 0 if event['stage'] == 'vs' else 1
        draws = sum(pair[2] for pair in latest['totalDraws'] if pair[stage] == event['id'])
        shaders.append(dict(
            **event, sha256=digest(code), sourceMatches=assembly_sources.get(normalized, []),
            constantNames=sorted(set(re.findall(r'^//\s+(?:float\w*|int\w*|bool\w*)\s+(\w+)', disassembly, re.M))),
            serverMatches=[str(p.relative_to(run)) for p in matches],
            spirv=[str(p.with_suffix('.spv').relative_to(run)) for p in matches if p.with_suffix('.spv').is_file()],
            drawsThroughSnapshot=draws,
        ))
    used = [s for s in shaders if s['drawsThroughSnapshot']]
    report = dict(
        schema=1, run=str(run), backend=backend, inventory=inventory, shaders=shaders, lastSnapshot=latest,
        counts=dict(unique=len(shaders), used=len(used), received=sum(bool(s['serverMatches']) for s in shaders),
                    translated=sum(bool(s['spirv']) for s in shaders), usedReceived=sum(bool(s['serverMatches']) for s in used),
                    standaloneMatched=len({n for s in shaders for n in s['sourceMatches']})),
        limits=['Ordinary CreateDevice/Present only; no Ex-only device creation.',
                'All four game DrawPrimitive variants are counted; injected adapter markers are excluded.',
                'State blocks are observed through draw-time GetShader queries.',
                'Counters end at the last periodic snapshot; unvisited scenes are not covered.',
                'Assembly-source identification compares SDK disassembly without comments; server equality compares exact bytes.',
                'Presence of SPIR-V does not establish correct ray-traced material or geometry capture.'],
    )
    if backend == 'system':
        report['limits'].append('Native SetShader/SetConstants hook completeness is not established; zero counters do not prove absence. Compare bytecodes and draw-time state only.')
    target = run / 'shader-coverage.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(report=str(target), counts=report['counts'], shaders=[
        {k:s[k] for k in ('id', 'stage', 'bytes', 'sourceMatches', 'constantNames', 'drawsThroughSnapshot')} for s in shaders
    ]), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--game', type=Path, default=Path('local-data/Winx Club'))
    args = parser.parse_args()
    analyze(args.run.resolve(), args.game.resolve())

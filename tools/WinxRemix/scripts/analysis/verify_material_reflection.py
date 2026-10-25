"""Compare the adapter's CTAB interpretation with the installed Microsoft D3DX.

Uses preserved game-created shader bytes. No game or GPU process is started.
"""
import json
from pathlib import Path
import re
import sys
from analyze_shader_audit import D3dx, digest

build, corpus = map(Path, sys.argv[1:])
sdk = D3dx()
selected = {'AmbientCol', 'MatDiffuse', 'MatSpecular', 'ConstColor',
            'LightAmbientColorDir0', 'LightDiffuseColorDir0'}
events = [json.loads(line) for line in (build / 'reflection.jsonl').read_text().splitlines()]
records = []
for entry in events[:-1]:
    source = corpus / entry['file']
    bytecode = source.read_bytes()
    assembly, _ = sdk.disassemble(bytecode)
    expected = sorted([name, int(register), int(size)] for name, register, size in
                      re.findall(r'^//\s+(\w+)\s+c(\d+)\s+(\d+)\s*$', assembly, re.M)
                      if name in selected)
    assert sorted(entry['colors']) == expected, (entry, expected)
    if expected:
        assert entry['valid']
    records.append(dict(file=entry['file'], sha256=digest(bytecode), colors=expected,
                        reflectionValid=entry['valid']))
assert len(records) == 15, len(records)
# The eighth generated VS (0015) has only transforms after optimization.
assert sum(record['reflectionValid'] for record in records) == 8
assert sum(bool(record['colors']) for record in records) == 7
result = dict(status='PASS', boundaryChecks=events[-1]['boundaryChecks'],
              files=len(records), withColorInputs=7, records=records)
(build / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(f"PASS {result['boundaryChecks']} boundary checks; {len(records)} shaders compared with D3DX; 7 with named color inputs")

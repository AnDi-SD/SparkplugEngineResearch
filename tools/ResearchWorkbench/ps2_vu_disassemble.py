"""Bounded static VU decode using an external, pinned PCSX2 disassembler.

No VU instructions execute. Raw encodings and generated ISA belong in local-data.
Recognition, byte preservation, control-flow hints and semantics are separate.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = Path(__file__).with_name('ps2_vu_decoder_source.json')
MAX_PROGRAM_BYTES = 16384
MAX_METADATA_BYTES = 4 * 1024 * 1024
MAX_DECODER_BYTES = 16 * 1024 * 1024
TIMEOUT_SECONDS = 15


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_bounded(path: Path, maximum: int) -> bytes:
    if not path.is_file() or not 0 < path.stat().st_size <= maximum:
        raise ValueError(f'bounded nonempty file required: {path}')
    with path.open('rb') as stream:
        data = stream.read(maximum + 1)
    if not 0 < len(data) <= maximum:
        raise ValueError(f'file changed outside bound: {path}')
    return data


def verify_sources(directory: Path, contract: dict | None = None) -> dict:
    contract = contract or json.loads(read_bounded(CONTRACT, MAX_METADATA_BYTES))
    rows = []
    for row in contract['files']:
        name = row['name']
        if Path(name).name != name or '\\' in name:
            raise ValueError('flat pinned dependency filenames required')
        data = read_bounded(directory / name, MAX_METADATA_BYTES)
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if digest(data) != row['sha256'] or blob != row['gitBlobSha1']:
            raise ValueError(f'pinned decoder source mismatch: {name}')
        rows.append(dict(name=name, sha256=digest(data), gitBlobSha1=blob))
    return dict(commit=contract['commit'], spdx=contract['spdx'], files=rows)


def invoke(decoder: Path, arguments: list[str], text: str | None = None) -> str:
    result = subprocess.run([str(decoder), *arguments], input=text,
                            capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
                            check=False)
    if result.returncode or len(result.stdout) > MAX_METADATA_BYTES:
        raise ValueError(f'decoder protocol failed: exit{result.returncode}; {result.stderr[:400]}')
    return result.stdout


def verify_decoder(decoder: Path, expected_sha256: str, commit: str) -> dict:
    data = read_bounded(decoder, MAX_DECODER_BYTES)
    if digest(data) != expected_sha256.lower():
        raise ValueError('decoder executable SHA256 mismatch')
    identity = json.loads(invoke(decoder, ['--identity']))
    if (identity.get('protocol') != 1 or identity.get('upstreamCommit') != commit
            or identity.get('tableRepairs') != 4
            or identity.get('executesVu') is not False):
        raise ValueError('decoder identity mismatch')
    return dict(path=str(decoder.resolve()), sha256=digest(data), identity=identity)


def input_words(data: bytes) -> list[tuple[int, int]]:
    if not data or len(data) > MAX_PROGRAM_BYTES or len(data) % 8:
        raise ValueError('one1..2048-pair aligned VU image required')
    return list(struct.iter_unpack('<II', data))


def validate_rows(data: bytes, rows: list[dict]) -> None:
    words = input_words(data)
    if len(rows) != len(words):
        raise ValueError('decoder row count mismatch')
    for index, ((lower, upper), row) in enumerate(zip(words, rows)):
        for field, expected in [('pc', index * 8), ('lowerWord', lower), ('upperWord', upper)]:
            if type(row.get(field)) is not int or row[field] != expected:
                raise ValueError(f'decoder raw encoding/order mismatch at{index}')
        for field in ['lowerText', 'upperText', 'lowerSymbol', 'upperSymbol']:
            if not isinstance(row.get(field), str) or len(row[field]) > 1024:
                raise ValueError('bounded decoder strings required')
        if bool(upper & 0x80000000) != (row['lowerSymbol'] == 'LOI'):
            raise ValueError('I-bit lower immediate classification mismatch')


def decode_program(decoder: Path, data: bytes) -> list[dict]:
    lines = ''.join(f'{lower:08x} {upper:08x} {index * 8:08x}\n'
                    for index, (lower, upper) in enumerate(input_words(data)))
    rows = [json.loads(line) for line in invoke(decoder, [], lines).splitlines()]
    validate_rows(data, rows)
    return rows


def opcode(symbol: str, text: str) -> str | None:
    match = re.fullmatch(r'disExternalVUMI_([A-Za-z0-9_]+)', symbol)
    if not match or 'Bad OP' in text or text.lstrip().lower().startswith('.word'):
        return None
    return match[1]


def sign11(value: int) -> int:
    value &= 0x7FF
    return value - 0x800 if value & 0x400 else value


def annotate(rows: list[dict]) -> dict:
    if not 0 < len(rows) <= 2048:
        raise ValueError('bounded nonempty decoded image required')
    labels = {0}
    upper_ops, lower_ops, flag_counts = Counter(), Counter(), Counter()
    unknown_upper, unknown_lower, immediate, direct, indirect, anomalies, repairs = [], [], [], [], [], [], []
    registers = re.compile(r'\b(?:vf\d{2}(?:\.[xyzw])?|vi\d{2}|ACC|STATUS|MAC|CLIP|[QIPR])\b')
    for index, row in enumerate(rows):
        row['index'] = index
        row['flags'] = {name: bool(row['upperWord'] & (1 << bit))
                        for name, bit in [('I', 31), ('E', 30), ('M', 29), ('D', 28), ('T', 27)]}
        flag_counts.update(name for name, value in row['flags'].items() if value)
        up = opcode(row['upperSymbol'], row['upperText'])
        low = opcode(row['lowerSymbol'], row['lowerText'])
        row['upperOpcode'] = up
        row['upperRecognized'] = up is not None
        if ((row['upperWord'] & 0x3F) in {0x3C, 0x3D, 0x3E, 0x3F}
                and ((row['upperWord'] >> 6) & 31) == 1):
            row['decoderRepair'] = 'SUBA accumulator dispatch from pinned interpreter table'
            repairs.append(index)
        row['lowerKind'] = 'immediate-I' if row['flags']['I'] else 'instruction'
        row['lowerOpcode'] = None if row['flags']['I'] else low
        row['lowerRecognized'] = not row['flags']['I'] and low is not None
        if up is None: unknown_upper.append(index)
        else: upper_ops.update([up])
        if row['flags']['I']: immediate.append(index)
        elif low is None: unknown_lower.append(index)
        else: lower_ops.update([low])
        row['registerOperands'] = dict(upper=registers.findall(row['upperText']),
                                       lower=registers.findall(row['lowerText']))
        row['controlFlow'] = None
        if row['lowerKind'] == 'instruction' and low in {'B', 'BAL', 'IBEQ', 'IBNE', 'IBLTZ', 'IBGTZ', 'IBLEZ', 'IBGEZ'}:
            target = index + 1 + sign11(row['lowerWord'])
            within = 0 <= target < len(rows)
            row['controlFlow'] = dict(kind='direct-relative', opcode=low,
                targetIndexUnwrapped=target, targetInExtractedImage=within,
                targetLabel=f'L_{target:04X}' if within else None,
                delaySlotIndex=index + 1 if index + 1 < len(rows) else None,
                continuationIndex=index + 2 if low != 'B' and index + 2 < len(rows) else None,
                hardwareWrapApplied=False)
            direct.append(index)
            if within: labels.add(target)
        elif row['lowerKind'] == 'instruction' and low in {'JR', 'JALR'}:
            row['controlFlow'] = dict(kind='indirect', opcode=low, target=None,
                delaySlotIndex=index + 1 if index + 1 < len(rows) else None)
            indirect.append(index)
        # Upstream table selects IBGEZ, but its formatter spells it IBEZ.
        # Keep both verbatim text and the selected leaf symbol; never hide this.
        if low == 'IBGEZ' and row['lowerText'].strip().startswith('IBEZ'):
            row['formatterAnomaly'] = 'upstream IBGEZ leaf prints IBEZ'
            anomalies.append(index)
    for row in rows:
        row['label'] = f'L_{row["index"]:04X}' if row['index'] in labels else None
    return dict(pairs=len(rows), upperRecognized=len(rows) - len(unknown_upper),
        upperUnknownIndices=unknown_upper, lowerInstructionRecognized=sum(lower_ops.values()),
        lowerUnknownIndices=unknown_lower, lowerImmediateIndices=immediate,
        upperOpcodes=dict(sorted(upper_ops.items())), lowerOpcodes=dict(sorted(lower_ops.items())),
        flags=dict(flag_counts), directBranchIndices=direct, indirectBranchIndices=indirect,
        formatterAnomalyIndices=anomalies, repairedUpperIndices=repairs, labels=len(labels))


def reconstruct_encodings(rows: list[dict]) -> bytes:
    return b''.join(struct.pack('<II', row['lowerWord'], row['upperWord']) for row in rows)


def listing(rows: list[dict]) -> str:
    lines = ['# Static PCSX2 ISA decode. Raw prefix = lower32 upper32, little-endian words.',
             '# Labels use instruction-pair indices; targets are unwrapped, unit unqualified.',
             '# Recognition is not numeric semantics, timing, legal-encoding or reachability proof.']
    for row in rows:
        if row['label']: lines.append(row['label'] + ':')
        flags = ''.join(name for name, value in row['flags'].items() if value) or '-'
        edge = row['controlFlow']
        note = '' if edge is None else ' ; ' + json.dumps(edge, separators=(',', ':'))
        lines.append(f'{row["index"]:04X}: {row["lowerWord"]:08X} {row["upperWord"]:08X} [{flags}] '
                     f'{row["upperText"].strip()} || {row["lowerText"].strip()}{note}')
    return '\n'.join(lines) + '\n'


def bounded_destination(path: Path, base: Path) -> Path:
    path, base = path.resolve(), base.resolve()
    if path == base or not path.is_relative_to(base) or path.exists():
        raise ValueError('fresh output strictly within the designated workspace root required')
    return path


def run_inventory(inventory: Path, payloads: Path, decoder: Path, expected_sha256: str,
                  source_directory: Path, output: Path, report_path: Path) -> dict:
    output = bounded_destination(output, ROOT / 'local-data')
    report_path = bounded_destination(report_path, ROOT / '.private')
    sources = verify_sources(source_directory)
    decoder_identity = verify_decoder(decoder, expected_sha256, sources['commit'])
    inventory_data = read_bounded(inventory, MAX_METADATA_BYTES)
    manifest = json.loads(inventory_data)
    accepted = manifest.get('accepted', [])
    if (manifest.get('schema') != 1 or manifest.get('kind') != 'ps2-static-microprogram-upload-inventory'
            or not 0 < len(accepted) <= 64):
        raise ValueError('explicit1..64 accepted input images required')
    payloads = payloads.resolve()
    programs, rendered = [], {}
    stems = set()
    for entry in accepted:
        name = entry['extracted']['program']
        path = (payloads / name).resolve()
        stem = Path(name).stem
        if (Path(name).name != name or '\\' in name or path.parent != payloads
                or stem.casefold() in stems or entry.get('status') != 'ACCEPTED_STATIC_UPLOAD_CONTAINER'):
            raise ValueError('distinct flat payload names required')
        stems.add(stem.casefold())
        data = read_bounded(path, MAX_PROGRAM_BYTES)
        if (digest(data) != entry['extracted']['program_sha256'] or len(data) != entry['program_bytes']
                or len(data) != entry['instructions'] * 8):
            raise ValueError('input image identity/length mismatch')
        rows = decode_program(decoder, data)
        coverage = annotate(rows)
        reconstructed = reconstruct_encodings(rows)
        if reconstructed != data:
            raise ValueError('raw encoding reconstruction mismatch')
        document = dict(schema=1, inputSha256=digest(data), decoder=decoder_identity,
                        upstream=sources, coverage=coverage, instructions=rows,
                        semanticRecovery=False, unitQualified=False, runtimeQualified=False)
        rendered[name] = {
            stem + '.isa.json': (json.dumps(document, indent=2) + '\n').encode(),
            stem + '.isa.txt': listing(rows).encode(),
            stem + '.raw-roundtrip.bin': reconstructed}
        programs.append(dict(input=name, inputSha256=digest(data), sourceContainerAddress=entry['address'],
            bytes=len(data), coverage=coverage, rawEncodingRoundTrip=True, mnemonicAssemblyRoundTrip=False,
            files={f: digest(b) for f, b in rendered[name].items()}))
    report = dict(schema=1, kind='bounded-static-vu-isa-decode', inventorySha256=digest(inventory_data),
        adapterSourceSha256=digest(Path(__file__).with_name('ps2_vu_disassembler.cpp').read_bytes()),
        runnerSourceSha256=digest(Path(__file__).read_bytes()), decoder=decoder_identity, upstream=sources,
        programs=programs, imageCount=len(programs), instructionPairs=sum(p['coverage']['pairs'] for p in programs),
        scope=dict(unitQualified=False, executesVu=False, numericSemanticsQualified=False,
                   registerReadWriteDataflowRecovered=False, allGameProgramsEnumerated=False),
        limitations=['Upstream disassembler recognition does not validate every reserved encoding.',
                     'Raw-encoding reconstruction preserves bytes; no ISA assembler round-trip is claimed.',
                     'Direct branch targets are unwrapped; unknown VU unit prevents a hardware wrap assumption.',
                     'Indirect targets, E/D/T/M control behavior, delay hazards and reachable CFG remain open.',
                     'Operand register names are retained, not a complete read/write or memory dataflow model.',
                     'Four SUBA debugger-table cells are explicitly repaired from the pinned interpreter.',
                     'Legacy upstream formatter anomalies are retained separately from leaf symbols.'])
    output.mkdir(parents=True)
    for images in rendered.values():
        for name, data in images.items():
            (output / name).write_bytes(data)
    for program in programs:
        for name, expected in program['files'].items():
            if digest((output / name).read_bytes()) != expected:
                raise ValueError('output readback mismatch')
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--payloads', type=Path, required=True)
    parser.add_argument('--decoder', type=Path, required=True)
    parser.add_argument('--decoder-sha256', required=True)
    parser.add_argument('--decoder-source-directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report = run_inventory(args.inventory, args.payloads, args.decoder.resolve(), args.decoder_sha256,
                           args.decoder_source_directory, args.output, args.report)
    print(json.dumps(dict(images=report['imageCount'], pairs=report['instructionPairs'],
                          report=str(args.report), output=str(args.output))))


if __name__ == '__main__':
    main()

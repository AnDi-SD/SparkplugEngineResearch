#!/usr/bin/env python3
"""Bounded static PC PE shader inventory; never executes the inspected image.

Our DXSO framing validator is deliberately separate from SDK acceptance and from
game ownership/reachability. Raw extracted input bytes are local artifacts only.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import mmap
from pathlib import Path
import re
import struct
import sys

from inspect_serializer_manager import read_pe

ROOT = Path(__file__).resolve().parents[2]
LOCAL = ROOT / 'local-data/research-cache'
MAX_STREAM_BYTES = 65536
MAX_INSTRUCTIONS = 4096
VERSIONS = {((0xFFFE if stage == 'vs' else 0xFFFF) << 16) | (major << 8) | minor:
            (stage, major, minor) for stage in ('vs', 'ps')
            for major, minors in ((1, range(5)), (2, range(2)), (3, range(1))) for minor in minors}
# Operand counts (not DWORD lengths); DEF/DEFI/DEFB immediates are exceptions.
ARITY = {0: 0, 1: 2, 2: 3, 3: 3, 4: 4, 5: 3, 6: 2, 7: 2, 8: 3,
         9: 3, 10: 3, 11: 3, 12: 3, 13: 3, 14: 2, 15: 2, 16: 2,
         17: 3, 18: 4, 19: 2, 20: 3, 21: 3, 22: 3, 23: 3, 24: 3,
         25: 1, 26: 2, 27: 2, 28: 0, 29: 0, 30: 1, 31: 2, 32: 3,
         33: 3, 34: 4, 35: 2, 36: 2, 37: 4, 38: 1, 39: 0, 40: 1,
         41: 2, 42: 0, 43: 0, 44: 0, 45: 2, 46: 2, 47: 2, 48: 5,
         64: 1, 65: 1, 66: 3, 67: 2, 68: 2, 69: 2, 70: 2, 71: 2,
         72: 2, 73: 2, 74: 2, 76: 3, 77: 2, 78: 2, 79: 2, 80: 4,
         81: 5, 82: 2, 83: 2, 84: 2, 85: 2, 86: 1, 87: 2, 88: 4,
         89: 3, 90: 4, 91: 2, 92: 2, 93: 5, 94: 3, 95: 3, 96: 1}
KNOWN_BOUNDARIES = {0x4C8980: 'game-shader-specialization', 0x4CFFE0: 'game-template-compiler',
                    0x611241: 'linked-SDK-assemble', 0x611324: 'linked-SDK-compile',
                    0x4CA030: 'game-vertex-device-create-wrapper',
                    0x4C9EB0: 'game-pixel-device-create-wrapper',
                    0x4AE660: 'game-constant-parameter-name-resolver'}
KNOWN_LAYOUT_HASHES = {'3f022480bf55045da4bf692e4bc8862ed38fc024e8a964a558fbdfdf646dfc4f',
                       'c27ea9db4228781a12a90ae808807d4af1397a7e40dd8f5fff28f3c87cc62cdb'}
TEXT_PATTERN = re.compile(rb'[\x09\x0a\x0d\x20-\x7e]{4,}')
WIDE_TEXT_PATTERN = re.compile(rb'(?:[\x09\x0a\x0d\x20-\x7e]\x00){4,}')
TEXT_MARKER = re.compile(rb'(?i)(?:\b(?:vs|ps)[._][123][._][0-4]\b|INSERTION POINT|'
                         rb'BlendWeightCount|ColorMode\s*=|LightCount\s*=|bUseSpecular\s*=|'
                         rb'LightType\[|bHasUVTransform\[|USE_TEXCOORD|\.(?:rfx|vsh|psh)\b|'
                         rb'\bfloat[1-4]?(?:x[1-4])?\b|\bregister\s*\(|'
                         rb'\b(?:dp3|dp4|m4x4|texld)\s+[rcvot][0-9])')


def digest(data) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_dxso(data, offset=0, end=None) -> dict:
    """Validate bounded instruction framing/parameter tags, not full GPU semantics.

    SM1 uses opcode-specific arities; SM2/3 use encoded lengths, with explicit
    predicate/relative operands. Comments are skipped by their DWORD count and
    never interpreted as executable instructions. END must be the exact token.
    """
    end = min(len(data), end if end is not None else len(data), offset + MAX_STREAM_BYTES)
    result = {'validation': 'rejected', 'reason': '', 'length': 0, 'instructions': 0, 'comments': 0}
    def reject(reason):
        result['reason'] = reason
        return result
    if offset < 0 or offset + 4 > end:
        return reject('truncated-version')
    version = struct.unpack_from('<I', data, offset)[0]
    if version not in VERSIONS:
        return reject('unsupported-version')
    stage, major, minor = VERSIONS[version]
    result.update(stage=stage, major=major, minor=minor)
    if major == 1 and ((stage == 'vs' and minor != 1) or (stage == 'ps' and minor == 0)):
        return reject('legacy-or-invalid-profile-not-covered')
    at = offset + 4
    while at + 4 <= end:
        token = struct.unpack_from('<I', data, at)[0]
        op = token & 65535
        if op == 65535:
            if token != 65535:
                return reject('malformed-END')
            if not result['instructions']:
                return reject('empty-stream')
            result.update(validation='framing-valid', reason='bounded-END-and-instruction-framing', length=at + 4 - offset)
            return result
        if op == 65534:
            if token & 0x80000000:
                return reject('malformed-comment')
            size = 4 * (1 + ((token >> 16) & 32767))
            if at + size > end:
                return reject('truncated-comment')
            result['comments'] += 1
            at += size
            continue
        if op == 65533:
            if token != 65533 or stage != 'ps' or major != 1 or minor != 4:
                return reject('invalid-PHASE')
            at += 4
            continue
        if token & 0x80000000 or op not in ARITY:
            return reject('unknown-opcode-or-instruction-tag')
        count = ARITY[op]
        if major == 1:
            allowed = set(range(25)) | {31, 78, 79, 81} if stage == 'vs' else set(range(20)) | set(range(64, 90))
            if op not in allowed:
                return reject('opcode-outside-SM1-stage')
            if token & 0x3F000000:
                return reject('SM1-reserved-instruction-bits')
            if stage == 'ps' and op in (64, 66):
                count = 2 if minor == 4 else 1
            length = count
        else:
            if op == 37 and major == 3:  # SINCOS loses two approximation constants.
                count = 2
            length = (token >> 24) & 15
            predicate = int(bool(token & 0x10000000))
            if length < count + predicate or length > 2 * count + predicate:
                return reject('encoded-length-does-not-match-opcode')
        size = 4 * (1 + length)
        if at + size > end:
            return reject('truncated-instruction')
        args = struct.unpack_from('<' + 'I' * length, data, at + 4) if length else ()
        immediate = {47: 1, 48: 4, 81: 4}.get(op, 0)
        registers = args[:-immediate] if immediate else args
        if immediate and length != count:
            return reject('invalid-immediate-instruction-length')
        if any(not (arg & 0x80000000) for arg in registers):
            return reject('operand-without-parameter-tag')
        if major >= 2 and not immediate:
            # Relative addressing inserts a register token only after an operand
            # with bit13; predicate is an additional ordinary parameter token.
            logical = 0
            i = 0
            while i < len(args):
                arg = args[i]
                logical += 1
                i += 1
                if arg & 0x2000:
                    if i >= len(args):
                        return reject('missing-relative-address-token')
                    i += 1
            if logical != count + int(bool(token & 0x10000000)):
                return reject('unexpected-extra-operands')
        result['instructions'] += 1
        if result['instructions'] > MAX_INSTRUCTIONS:
            return reject('instruction-budget')
        at += size
    return reject('missing-END-or-byte-budget')


def pe_layout(data):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Expected a PE32 executable')
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Truncated PE header')
    count, optional_size = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    if pe + 24 + optional_size + 40 * count > len(data) or optional_size < 64:
        raise ValueError('Truncated PE section table')
    if struct.unpack_from('<H', data, pe + 24)[0] != 0x10B:
        raise ValueError('Only PE32 is covered')
    base, sections = read_pe(data)
    names = [data[pe + 24 + optional_size + i * 40:pe + 24 + optional_size + i * 40 + 8].split(b'\0')[0].decode('ascii', 'replace') for i in range(count)]
    return base, sections, names


def location(offset, base, sections, names):
    for section, name in zip(sections, names):
        if section.offset <= offset < section.offset + section.size:
            return {'fileOffset': offset, 'section': name, 'rva': section.address + offset - section.offset,
                    'va': base + section.address + offset - section.offset, 'executableSection': section.executable}
    return {'fileOffset': offset, 'section': None, 'rva': None, 'va': None, 'executableSection': None}


def source_kind(value):
    if re.search(rb'(?im)^\s*(?:vs|ps)[._][123][._][0-4]\s*(?:\r?$|[;/])', value) and re.search(rb'(?im)^\s*(?:mov|add|mul|mad|dp[34]|tex)\b', value):
        return 'assembly-program-text-candidate'
    if re.search(rb'\b(?:float[1-4]?(?:x[1-4])?|void)\s+\w+\s*\([^)]*\)[^;]*\{', value) and b'}' in value:
        return 'HLSL-program-text-candidate'
    if re.search(rb'INSERTION POINT|BlendWeightCount\s*=|ColorMode\s*=|LightCount\s*=|bUseSpecular\s*=|LightType\[|bHasUVTransform\[|USE_TEXCOORD', value):
        return 'source-generation-fragment'
    if re.search(rb'\.(?:vsh|psh|rfx)\b', value, re.I):
        return 'external-resource-reference'
    return 'compiler-language-or-shader-related-string'


def reference_candidates(data, targets, base, sections, names):
    """Absolute DWORD occurrences; Capstone verifies containing instruction only.

    No CFG/function-boundary or caller argument tracking claim. Data pointers and
    instruction-immediate candidates remain explicitly distinct.
    """
    try:
        import capstone
    except ImportError:
        try:
            sys.path.insert(0, str(LOCAL / 'python'))
            import capstone
        except ImportError:
            capstone = None
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32) if capstone else None
    result = {}
    for va in sorted(set(targets)):
        refs = []
        at = 0
        needle = struct.pack('<I', va)
        while (at := data.find(needle, at)) >= 0:
            row = location(at, base, sections, names)
            row['kind'] = 'absolute-DWORD-occurrence'
            if md and row['executableSection']:
                decoded = []
                # 0..11 prefix bytes +4-byte immediate fits maximum15-byte x86.
                section = next(s for s in sections if s.offset <= at < s.offset + s.size)
                for start in range(max(section.offset, at - 11), at + 1):
                    code = data[start:min(at + 15, section.offset + section.size)]
                    ins = next(md.disasm(code, base + section.address + start - section.offset, count=1), None)
                    if ins and start < at and start + ins.size >= at + 4:
                        decoded.append({'va': ins.address, 'bytes': ins.bytes.hex(), 'text': ins.mnemonic + ' ' + ins.op_str})
                if decoded:
                    row['kind'] = 'candidate-instruction-immediate-unproven-boundary'
                    row['decodings'] = decoded
            refs.append(row)
            at += 1
        result[str(va)] = refs
    return result, capstone.__version__ if capstone else None


def scan_image(path: Path, output: Path, sdk=None, seen=None):
    with path.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        base, sections, names = pe_layout(data)
        image_hash = digest(data)
        if seen is not None and image_hash in seen:
            return {'path': str(path), 'sha256': image_hash, 'bytes': len(data),
                    'duplicateOf': seen[image_hash]['path'], 'canonicalImageHash': image_hash,
                    'validation': 'same-run-entire-file-SHA256-equal'}
        folder = output / image_hash
        folder.mkdir(parents=True, exist_ok=False)
        rows, strings = [], []
        for version, profile in VERSIONS.items():
            at = 0
            needle = struct.pack('<I', version)
            while (at := data.find(needle, at)) >= 0:
                loc = location(at, base, sections, names)
                section = next((s for s in sections if s.offset <= at < s.offset + s.size), None)
                end = section.offset + section.size if section else len(data)
                parsed = parse_dxso(data, at, end)
                row = dict(loc, version=hex(version), profile='%s_%d_%d' % profile, parser=parsed)
                if parsed['validation'] == 'framing-valid':
                    raw = data[at:at + parsed['length']]
                    row['sha256'] = digest(raw)
                    row['classification'] = 'ownership-and-game-reachability-unestablished'
                    name = '%08x-%s.bin' % (at, row['sha256'][:16])
                    (folder / name).write_bytes(raw)
                    row['extracted'] = str((folder / name).relative_to(output))
                    if sdk:
                        try:
                            text, _ = sdk.disassemble(raw)
                            row['sdkValidation'] = 'disassembler-accepted'
                            (folder / (name + '.asm.txt')).write_text(text, encoding='utf-8')
                        except (RuntimeError, ValueError) as error:
                            row['sdkValidation'] = 'disassembler-rejected'
                            row['sdkError'] = str(error)
                rows.append(row)
                at += 1
        for pattern, encoding in ((TEXT_PATTERN, 'ascii'), (WIDE_TEXT_PATTERN, 'utf-16le')):
            for match in pattern.finditer(data):
                value = match.group()
                ascii_value = value if encoding == 'ascii' else value[::2]
                if not TEXT_MARKER.search(ascii_value):
                    continue
                kind = source_kind(ascii_value)
                row = dict(location(match.start(), base, sections, names), length=len(value), sha256=digest(value),
                           classification=kind, encoding=encoding, text=value.decode(encoding))
                # Extract programs/fragments, not every SDK diagnostic containing float.
                if kind in ('assembly-program-text-candidate', 'HLSL-program-text-candidate', 'source-generation-fragment'):
                    name = '%08x-%s.%s.txt' % (match.start(), row['sha256'][:16], encoding)
                    (folder / name).write_bytes(value)
                    row['extracted'] = str((folder / name).relative_to(output))
                strings.append(row)
        targets = [x['va'] for x in rows if x['parser']['validation'] == 'framing-valid' and x['va'] is not None]
        targets += [x['va'] for x in strings if x['classification'] != 'compiler-language-or-shader-related-string' and x['va'] is not None]
        refs, capstone_version = reference_candidates(data, targets, base, sections, names)
        calls = []
        for section, name in zip(sections, names):
            if not section.executable:
                continue
            end, at = section.offset + section.size, section.offset
            while (at := data.find(b'\xe8', at, end)) >= 0:
                if at + 5 <= end:
                    va = base + section.address + at - section.offset
                    target = va + 5 + struct.unpack_from('<i', data, at + 1)[0]
                    if image_hash in KNOWN_LAYOUT_HASHES and target in KNOWN_BOUNDARIES:
                        calls.append(dict(location(at, base, sections, names), target=target,
                                          targetLabel=KNOWN_BOUNDARIES[target], validation='E8-rel32-encoding-only-no-CFG-proof'))
                at += 1
        parameter_table = []
        table_raw = None
        if image_hash in KNOWN_LAYOUT_HASHES and base == 0x400000:
            for s in sections:
                off = 0x6EFAE0 - base - s.address
                if 0 <= off and off + 23 * 4 <= s.size:
                    raw = data[s.offset + off:s.offset + off + 23 * 4]
                    table_name = 'constant-parameter-pointers-006efae0.bin'
                    (folder / table_name).write_bytes(raw)
                    table_raw = dict(location(s.offset + off, base, sections, names), length=len(raw), sha256=digest(raw),
                                     extracted=str((folder / table_name).relative_to(output)))
                    for i in range(23):
                        pointer = struct.unpack_from('<I', data, s.offset + off + 4 * i)[0]
                        value = None
                        for p in sections:
                            rel = pointer - base - p.address
                            if 0 <= rel < p.size:
                                start = p.offset + rel
                                stop = data.find(b'\0', start, min(start + 256, p.offset + p.size))
                                if stop >= 0:
                                    value = data[start:stop].decode('ascii', 'replace')
                        parameter_table.append({'id': i, 'entryVA': 0x6EFAE0 + 4 * i, 'nameVA': pointer, 'name': value})
        boundary_windows = []
        if image_hash in KNOWN_LAYOUT_HASHES and capstone_version:
            import capstone
            md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
            for va, label in KNOWN_BOUNDARIES.items():
                for s in sections:
                    relative = va - base - s.address
                    if 0 <= relative and relative + 64 <= s.size:
                        raw = data[s.offset + relative:s.offset + relative + 64]
                        boundary_windows.append({'va': va, 'label': label, 'scope': '64-byte-linear-window-not-whole-function-or-CFG',
                                                 'instructions': [{'va': i.address, 'bytes': i.bytes.hex(), 'text': i.mnemonic + ' ' + i.op_str}
                                                                  for i in md.disasm(raw, va)]})
        result = {'path': str(path), 'sha256': image_hash, 'bytes': len(data), 'imageBase': base,
                'sections': [dict(name=n, rva=s.address, fileOffset=s.offset, rawBytes=s.size, executable=s.executable) for s, n in zip(sections, names)],
                'coverage': 'entire-raw-file-signature-search; candidate streams remain within one file-backed section',
                'dxsoCandidates': sorted(rows, key=lambda x: x['fileOffset']), 'shaderRelatedStrings': strings,
                'absoluteReferences': refs, 'directCallEncodingCandidates': calls,
                'knownParameterTable': {'va': 0x6EFAE0, 'expectedNamedEntries': 22, 'zeroSentinelIncluded': True, 'entries': parameter_table,
                                       'raw': table_raw,
                                       'identity': 'known-PC-build-address-interpretation; not automatically portable'},
                'knownBoundaryWindows': boundary_windows,
                'capstoneVersion': capstone_version,
                'counts': {'versionSignatures': len(rows), 'framingValid': sum(r['parser']['validation'] == 'framing-valid' for r in rows),
                           'parserRejectionReasons': dict(Counter(r['parser']['reason'] for r in rows if r['parser']['validation'] != 'framing-valid')),
                           'stringClassifications': dict(Counter(r['classification'] for r in strings))}}
        if seen is not None:
            seen[image_hash] = result
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('images', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path, help='New directory under local-data/research-cache')
    parser.add_argument('--sdk', help='Optional system D3DX DLL basename for disassembly only')
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(LOCAL.resolve()) or output == LOCAL.resolve():
        raise ValueError('Extracted original bytes must remain in a new local-data/research-cache directory')
    output.mkdir(parents=True, exist_ok=False)
    sdk = None
    if args.sdk:
        sys.path.insert(0, str(ROOT / 'tools/WinxRemix/scripts/shaders'))
        from pc_shader_sdk import D3dx
        sdk = D3dx(args.sdk)
    report = {'schema': 1, 'status': 'IN_PROGRESS', 'scriptSha256': digest(Path(__file__).read_bytes()), 'images': [], 'aliases': [],
              'sdk': {'path': str(sdk.path), 'sha256': digest(sdk.path.read_bytes())} if sdk else None,
              'limitations': ['Raw static scan does not expose dynamically decrypted/generated bytes.',
                              'Framing and SDK disassembly do not establish a game shader, GPU acceptance, ownership or reachability.',
                              'Absolute immediate and E8 references are candidates without CFG/argument/dataflow proof.',
                              'Printable ASCII/UTF16LE with tab/CR/LF and standard little-endian DXSO1/2/3 only; compression, other encodings, obfuscated text and arbitrary containers are outside coverage.',
                              'Linked SDK compiler tables/fragments must not be counted as game programs.',
                              'Known addresses are build-specific; parameter table mappings require independent input identity review.']}
    try:
        seen = {}
        for path in args.images:
            row = scan_image(path.resolve(), output, sdk, seen)
            report['aliases' if 'duplicateOf' in row else 'images'].append(row)
        report['status'] = 'STATIC_SCAN_COMPLETE'
    except BaseException as error:
        report['status'] = 'FAILED_INCOMPLETE'
        report['error'] = type(error).__name__ + ': ' + str(error)
        raise
    finally:
        (output / 'manifest.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'output': str(output), 'images': [{'path': x['path'], **x['counts']} for x in report['images']],
                      'aliases': report['aliases']}, indent=2))


if __name__ == '__main__':
    main()

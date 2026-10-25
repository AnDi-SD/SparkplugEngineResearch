#!/usr/bin/env python3
"""Inventory bounded PS2 DMA RET/NOP/MPG microprogram containers in an ELF.

Own static analysis, not a VU emulator/decompiler. A complete narrow grammar
match is a candidate. Extraction additionally requires a constant MIPS pointer
store and image-pinned, explicitly reviewed producer/upload evidence supplied
through --bindings. Unknown VIF commands fail closed; no search for an end bit
is used to invent a program boundary. Original bytes are written only below
local-data. The source ELF is never opened for writing.

Format references (no source from these implementations is incorporated):
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Dmac.h
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Vif_Codes.cpp
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from inspect_executable_architecture import read_elf_sections
from inspect_serializer_manager import PS2_SHA256

ROOT = Path(__file__).resolve().parents[2]
MAX_IMAGE_BYTES = 64 * 1024 * 1024
MAX_PACKET_BYTES = 0x10000
MAX_VU_MICRO_INSTRUCTIONS = 2048  # Upper storage bound, not a VU0/VU1 routing claim.


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def number(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def sections_for(data: bytes):
    if not 52 <= len(data) <= MAX_IMAGE_BYTES or data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('bounded little-endian ELF32 required')
    if struct.unpack_from('<H', data, 18)[0] != 8:
        raise ValueError('MIPS ELF required')
    start = struct.unpack_from('<I', data, 32)[0]
    stride, count, strings = struct.unpack_from('<HHH', data, 46)
    if stride < 40 or not 0 < count <= 256 or strings >= count or start + stride * count > len(data):
        raise ValueError('invalid ELF section table')
    for index in range(count):
        kind, offset, size = struct.unpack_from('<I8xII', data, start + index * stride + 4)
        if kind != 8 and offset + size > len(data):
            raise ValueError('truncated file-backed section')
    return [s for s in read_elf_sections(data) if s.section_type == 1 and s.flags & 2]


def window(data: bytes, sections, address: int, size: int):
    if size <= 0 or address < 0 or address + size > 0x100000000:
        raise ValueError('invalid virtual range')
    matches = [s for s in sections if s.address <= address and address + size <= s.address + s.size]
    if len(matches) != 1:
        raise ValueError('range must remain inside exactly one file-backed section')
    section = matches[0]
    offset = section.offset + address - section.address
    if offset + size > len(data):
        raise ValueError('truncated range')
    return data[offset:offset + size], offset


def parse_container(packet: bytes):
    """Require the entire DMA-defined body to be only NOP/MPG with contiguous VU addresses."""
    if len(packet) < 16 or len(packet) % 16 or len(packet) > MAX_PACKET_BYTES:
        raise ValueError('invalid packet size/alignment')
    tag, addr, vif0, vif1 = struct.unpack_from('<4I', packet)
    qwc = tag & 0xffff
    if tag >> 16 != 0x6000 or addr or vif0 or vif1 or qwc == 0:
        raise ValueError('not the supported zero-auxiliary DMA RET tag')
    if len(packet) != (qwc + 1) * 16:
        raise ValueError('DMA QWC does not match exact packet size')
    cursor, next_instruction, nops = 16, 0, 0
    chunks = []
    while cursor < len(packet):
        command_offset = cursor
        command = struct.unpack_from('<I', packet, cursor)[0]
        cursor += 4
        if command == 0:
            nops += 1
            continue
        if command >> 24 != 0x4a:
            raise ValueError('unsupported VIF command 0x%08x at +0x%x' % (command, command_offset))
        count = (command >> 16) & 255 or 256
        destination = command & 0xffff
        if destination != next_instruction:
            raise ValueError('MPG destination gap, overlap, or nonzero initial address')
        if destination + count > MAX_VU_MICRO_INSTRUCTIONS or cursor + count * 8 > len(packet):
            raise ValueError('MPG range exceeds packet/maximum VU micro memory')
        payload = packet[cursor:cursor + count * 8]
        chunks.append(dict(command_offset=command_offset, payload_offset=cursor,
                           destination_instruction=destination, instructions=count,
                           bytes=len(payload), sha256=digest(payload)))
        next_instruction += count
        cursor += count * 8
    if not chunks:
        raise ValueError('no MPG upload in DMA body')
    return dict(qwc=qwc, bytes=len(packet), nop_words=nops, chunks=chunks,
                instructions=next_instruction, program_bytes=next_instruction * 8)


def scan_containers(data: bytes, sections):
    candidates, rejected = [], []
    for section in sections:
        for relative in range((-section.address) % 16, section.size - 15, 16):
            offset = section.offset + relative
            tag, addr, vif0, vif1 = struct.unpack_from('<4I', data, offset)
            if tag >> 16 != 0x6000 or addr or vif0 or vif1 or not tag & 0xffff:
                continue
            size = ((tag & 0xffff) + 1) * 16
            address = section.address + relative
            row = dict(address=address, file_offset=offset, declared_bytes=size)
            try:
                if size > MAX_PACKET_BYTES or relative + size > section.size:
                    raise ValueError('DMA packet exceeds bounded file-backed range')
                raw = data[offset:offset + size]
                row.update(parse_container(raw), sha256=digest(raw), section=section.name)
                for chunk in row['chunks']:
                    chunk['source_address'] = address + chunk['payload_offset']
                    chunk['file_offset'] = offset + chunk['payload_offset']
                candidates.append(row)
            except ValueError as error:
                row['reason'] = str(error)
                rejected.append(row)
    return candidates, rejected


def constant_stores(data: bytes, sections, global_pointer: int | None = None):
    """Straight-line literal propagation only; branches/unknown instructions invalidate state.

    Records are syntactic static xrefs, not reachable execution. The supported
    operations never retain a value across an unknown instruction or a control
    transfer. Source and base provenance are retained for audit.
    """
    result = []
    seed = {0: (0, [])}
    if global_pointer is not None:
        seed[28] = (global_pointer, [])
    for section in sections:
        if not section.flags & 4:
            continue
        known = dict(seed)
        for relative in range(0, section.size - 3, 4):
            at = section.address + relative
            word = struct.unpack_from('<I', data, section.offset + relative)[0]
            op, rs, rt, rd = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            imm = word & 0xffff
            signed = imm - 0x10000 if imm & 0x8000 else imm
            if op == 15 and rs == 0:
                known[rt] = (imm << 16, [at])
            elif op in (9, 13):
                if rs in known:
                    value, proof = known[rs]
                    known[rt] = (((value + signed) if op == 9 else value | imm) & 0xffffffff, proof + [at])
                else:
                    known.pop(rt, None)
            elif op == 0 and word & 63 in (0x21, 0x25, 0x2d) and (rs == 0 or rt == 0):
                source = rs if rt == 0 else rt
                if source in known:
                    known[rd] = (known[source][0], known[source][1] + [at])
                else:
                    known.pop(rd, None)
            elif op == 0x2b:
                if rs in known:
                    result.append(dict(instruction_address=at, instruction_word=word,
                        target_address=(known[rs][0] + signed) & 0xffffffff,
                        value=known[rt][0] if rt in known else None,
                        value_provenance=known[rt][1] if rt in known else [],
                        base_provenance=known[rs][1], base_register=rs))
            elif op in (0x28, 0x29, 0x1f, 0x39, 0x3f):
                pass  # Stores do not change general-purpose registers.
            elif op in (0x20, 0x21, 0x23, 0x24, 0x25, 0x27, 0x1e, 0x37):
                known.pop(rt, None)
            elif word == 0:
                pass
            else:
                known = dict(seed)
            known[0] = (0, [])
    return result


def direct_calls(data: bytes, sections, target: int):
    result = []
    for section in sections:
        if section.flags & 4:
            for relative in range(0, section.size - 3, 4):
                word = struct.unpack_from('<I', data, section.offset + relative)[0]
                at = section.address + relative
                if word >> 26 in (2, 3) and (((at + 4) & 0xf0000000) | ((word & 0x3ffffff) << 2)) == target:
                    result.append(dict(address=at, kind='JAL' if word >> 26 == 3 else 'J'))
    return result


def verify_binding(data, sections, candidate, stores, binding):
    matches = [s for s in stores if s['value'] == candidate['address'] and
               s['instruction_address'] == number(binding['pointer_store_address'])]
    if len(matches) != 1:
        raise ValueError('reviewed pointer store does not match literal MIPS provenance')
    evidence = binding.get('code_evidence', [])
    if len(evidence) < 2 or len(evidence) > 16:
        raise ValueError('explicit initializer and upload proof windows required')
    roles = {e.get('role') for e in evidence}
    if not {'initializer', 'upload'} <= roles:
        raise ValueError('separate initializer and upload proof roles required')
    ranges = [(number(e['address']), number(e['address']) + number(e['bytes'])) for e in evidence]
    if any(a < d and c < b for index, (a, b) in enumerate(ranges)
           for c, d in ranges[index + 1:]):
        raise ValueError('proof windows must be distinct and non-overlapping')
    verified = []
    for item in evidence:
        address, size = number(item['address']), number(item['bytes'])
        if address % 4 or size % 4 or not 0 < size <= 4096:
            raise ValueError('bounded aligned MIPS proof window required')
        raw, offset = window(data, sections, address, size)
        if not any(s.flags & 4 and s.address <= address and address + size <= s.address + s.size for s in sections):
            raise ValueError('proof window must be inside an executable file-backed section')
        if digest(raw) != item['sha256'].lower():
            raise ValueError('reviewed code window hash changed')
        verified.append(dict(item, file_offset=offset))
    store_at = matches[0]['instruction_address']
    if not any(e.get('role') == 'initializer' and number(e['address']) <= store_at < number(e['address']) + number(e['bytes']) for e in evidence):
        raise ValueError('pointer store not covered by reviewed initializer')
    return dict(binding, pointer_store=matches[0], code_evidence=verified,
                acceptance='complete DMA/VIF grammar + literal pointer producer + reviewed image-pinned upload route')


def extract(data, candidate, destination: Path):
    stem = 'vu-%08x' % candidate['address']
    packet = data[candidate['file_offset']:candidate['file_offset'] + candidate['bytes']]
    program = b''.join(packet[c['payload_offset']:c['payload_offset'] + c['bytes']] for c in candidate['chunks'])
    if len(program) != candidate['program_bytes']:
        raise ValueError('program reconstruction length mismatch')
    restored = bytearray(packet)
    for chunk in candidate['chunks']:
        start = chunk['destination_instruction'] * 8
        source = program[start:start + chunk['bytes']]
        if digest(source) != chunk['sha256']:
            raise ValueError('program chunk reconstruction hash mismatch')
        restored[chunk['payload_offset']:chunk['payload_offset'] + chunk['bytes']] = source
    if restored != packet:
        raise ValueError('DMA/VIF reconstruction mismatch')
    binary, container, words = stem + '.bin', stem + '.dma-vif.bin', stem + '.vu-words.txt'
    (destination / binary).write_bytes(program)
    (destination / container).write_bytes(packet)
    text = ['# VU 64-bit instruction word pairs; not decoded assembly or recovered semantics.',
            '# micro instruction index: lower32 upper32 (little-endian source words)']
    text += ['%04x: %08x %08x' % (i, *struct.unpack_from('<2I', program, i * 8))
             for i in range(len(program) // 8)]
    (destination / words).write_text('\n'.join(text) + '\n', encoding='utf-8')
    return dict(program=binary, container=container, word_listing=words,
                program_sha256=digest(program), packet_round_trip_equal=True,
                program_reread_equal=(destination / binary).read_bytes() == program,
                container_reread_equal=(destination / container).read_bytes() == packet)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, default=ROOT / 'local-data/Winx Club the game PS2/SLES_532.19')
    parser.add_argument('--expected-sha256', default=PS2_SHA256)
    parser.add_argument('--bindings', type=Path, help='Private image-pinned reviewed producer/upload facts; no extraction without them')
    parser.add_argument('--output', required=True, type=Path, help='Fresh payload directory below local-data')
    parser.add_argument('--report', required=True, type=Path, help='Fresh metadata file below .private/evidence')
    args = parser.parse_args()
    payloads, report_path = args.output.resolve(), args.report.resolve()
    if not payloads.is_relative_to((ROOT / 'local-data').resolve()) or not report_path.is_relative_to((ROOT / '.private/evidence').resolve()):
        parser.error('payloads belong in local-data; report belongs in private evidence')
    if payloads.exists() or report_path.exists():
        parser.error('fresh output directory and metadata file required')
    if not 52 <= args.elf.stat().st_size <= MAX_IMAGE_BYTES:
        parser.error('bounded ELF input size required before reading')
    data = args.elf.read_bytes()
    if digest(data) != args.expected_sha256.lower():
        parser.error('original ELF SHA256 mismatch')
    sections = sections_for(data)
    metadata = json.loads(args.bindings.read_text(encoding='utf-8-sig')) if args.bindings else {}
    if metadata and metadata['elf_sha256'].lower() != digest(data):
        parser.error('bindings refer to another original image')
    gp = number(metadata['global_pointer']) if metadata.get('global_pointer') is not None else None
    stores = constant_stores(data, sections, gp)
    candidates, rejected = scan_containers(data, sections)
    bindings = {number(b['packet_address']): b for b in metadata.get('programs', [])}
    if len(bindings) != len(metadata.get('programs', [])):
        parser.error('duplicate program binding')
    accepted, unresolved = [], []
    for candidate in candidates:
        row = dict(candidate, pointer_stores=[s for s in stores if s['value'] == candidate['address']])
        if candidate['address'] not in bindings:
            row.update(status='CANDIDATE', reason='no reviewed producer/upload binding')
            unresolved.append(row)
            continue
        row['provenance'] = verify_binding(data, sections, candidate, stores, bindings[candidate['address']])
        row.update(status='ACCEPTED_STATIC_UPLOAD_CONTAINER', target_unit=None,
                   target_unit_qualified=False, runtime_qualified=False)
        accepted.append(row)
    missing = sorted(set(bindings) - {c['address'] for c in candidates})
    if missing:
        raise ValueError('reviewed program no longer passes complete container grammar: ' + repr(missing))
    payloads.mkdir(parents=True, exist_ok=False)
    for row in accepted:
        row['extracted'] = extract(data, row, payloads)
    related_calls = {label: direct_calls(data, sections, number(address))
                     for label, address in metadata.get('direct_call_targets', {}).items()}
    table = metadata.get('sparse_table', {})
    table_writes = []
    if table:
        table_writes = [s for s in stores if number(table['base']) <= s['target_address'] < number(table['observed_end_exclusive'])]
    report = dict(schema=1, kind='ps2-static-microprogram-upload-inventory',
        original=dict(path=str(args.elf.resolve()), bytes=len(data), sha256=digest(data)),
        tool_sha256=digest(Path(__file__).read_bytes()),
        bindings_sha256=digest(args.bindings.read_bytes()) if args.bindings else None,
        scan=dict(section_count=len(sections), scanned_bytes=sum(s.size for s in sections),
                  grammar='16-byte aligned zero-auxiliary DMA RET; entire QWC body is NOP/MPG; contiguous VU addresses starting at0',
                  rejected_header_count=len(rejected), rejected_reasons=dict(Counter(r['reason'] for r in rejected))),
        summary=dict(accepted_containers=len(accepted), unresolved_candidates=len(unresolved),
                     mpg_chunks=sum(len(p['chunks']) for p in accepted),
                     micro_instructions=sum(p['instructions'] for p in accepted),
                     program_bytes=sum(p['program_bytes'] for p in accepted),
                     container_bytes=sum(p['bytes'] for p in accepted)),
        accepted=accepted, candidates=unresolved, rejected=rejected,
        direct_call_sites=related_calls, sparse_table=dict(table, observed_literal_stores=table_writes),
        related_class=metadata.get('related_class'),
        limits=['Static container and source-byte identity only; no VU execution, numerical semantics or rendered-image qualification.',
                'Original program length is established by MPG uploads inside the DMA QWC boundary, not by VU end-bit heuristics.',
                'Scanned complete narrow-grammar RET containers only. Other DMA tags, interleaved VIF commands and dynamically constructed uploads may exist.',
                'Static J/JAL and literal-store references do not prove reachable calls, runtime selection frequency or lifetime.',
                'Sparse-table initialized slots do not prove the extent of the table or exhaustive selector values.',
                'Other upload routes, direct VU0 micro-memory writes, overlay/data files and full game coverage remain open.',
                'DMA/VIF producer instructions, VU payloads and GS/GIF state are distinct. No GS data is presented as a shader.',
                'Word listing is lossless lower/upper word evidence, not a VU ISA disassembly or reconstructed HLSL.'])
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(report=report_path.as_posix(), payload_directory=payloads.as_posix(), **report['summary']), indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

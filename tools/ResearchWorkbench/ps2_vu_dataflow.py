"""Bounded static VU operand footprints and delay-aware direct control flow.

Own analysis of ISA facts, with pinned PCSX2 sources as the reference. No VU
arithmetic, pipeline timing, address wrapping or indirect target is invented.
"""
from __future__ import annotations
import argparse
from collections import Counter, deque
import json
from pathlib import Path
import re

from ps2_vu_disassemble import (ROOT, MAX_METADATA_BYTES, bounded_destination,
    digest, opcode, read_bounded, reconstruct_encodings, sign11, validate_rows)

LANES = 'xyzw'
CONTRACT = Path(__file__).with_name('ps2_vu_dataflow_source.json')


def lanes(code: int) -> str:
    return ''.join(lane for lane, bit in zip(LANES, (24, 23, 22, 21)) if code & (1 << bit))


def footprint(op: str | None, code: int, upper: bool, immediate: bool = False) -> dict:
    """Potential architectural operand accesses, not scheduled reaching values."""
    fs, ft, fd = (code >> 11) & 31, (code >> 16) & 31, (code >> 6) & 31
    is_, it, id_ = fs & 15, ft & 15, fd & 15
    mask = lanes(code)
    reads, writes, constants, ignored = set(), set(), set(), set()
    memory, notes = [], []
    expression = None
    classified = True

    def vf(reg, selected, write=False):
        for lane in selected:
            token = f'vf{reg:02d}.{lane}'
            (ignored if write and reg == 0 else constants if not write and reg == 0
             else writes if write else reads).add(token)

    def vi(reg, write=False):
        token = f'vi{reg:02d}'
        (ignored if write and reg == 0 else constants if not write and reg == 0
         else writes if write else reads).add(token)

    def access(kind, base, offset=0, update=0, selected=mask, width=32, signed=True):
        vi(base)
        if update: vi(base, True)
        memory.append(dict(kind=kind, base=f'vi{base:02d}', baseInterpretation='s16' if signed else 'u16',
            qwordOffset=offset, requestedBaseUpdate=update, baseUpdate=update if base else 0,
            updateBeforeAccess=bool(base and update < 0),
            lanes=selected, elementBits=width, hardwareWrapApplied=False))

    if immediate:
        writes.add('I')
        expression = f'I.bits = 0x{code:08X} (after upper reads old I)'
    elif upper:
        binary = re.fullmatch(r'(ADD|SUB|MUL|MADD|MSUB|MAX|MINI)(A?)([xyzwiq]?)', op or '')
        if op == 'NOP':
            expression = 'NOP'
        elif op in {'ABS', 'FTOI0', 'FTOI4', 'FTOI12', 'FTOI15', 'ITOF0', 'ITOF4', 'ITOF12', 'ITOF15'}:
            vf(fs, mask); vf(ft, mask, True)
            expression = f'vf{ft:02d}.{mask} = VU_{op}(vf{fs:02d}.{mask})'
        elif op == 'CLIP':
            vf(fs, 'xyz'); vf(ft, 'w'); reads.add('CLIP'); writes.add('CLIP')
            expression = f'CLIP = VU_CLIP_HISTORY(CLIP, vf{fs:02d}.xyz, vf{ft:02d}.w)'
        elif op in {'OPMULA', 'OPMSUB'}:
            vf(fs, 'xyz'); vf(ft, 'xyz')
            if op == 'OPMSUB':
                reads.update('ACC.' + c for c in 'xyz'); vf(fd, 'xyz', True)
            else: writes.update('ACC.' + c for c in 'xyz')
            writes.update({'MAC', 'STATUS'}); reads.update({'MAC', 'STATUS'})
            dest = f'vf{fd:02d}.xyz' if op == 'OPMSUB' else 'ACC.xyz'
            args = f'vf{fs:02d}.yzx, vf{ft:02d}.zxy'
            if op == 'OPMSUB': args = 'ACC.xyz, ' + args
            expression = f'{dest} = VU_{op}({args}); xyz fixed, mask ignored'
        elif binary:
            operation, accumulator, broadcast = binary.groups()
            vf(fs, mask)
            if broadcast in 'iq' and broadcast:
                reads.add(broadcast.upper())
                right = broadcast.upper()
            elif broadcast:
                vf(ft, broadcast if mask else '')
                right = f'vf{ft:02d}.{broadcast}'
            else:
                vf(ft, mask); right = f'vf{ft:02d}.{mask}'
            if operation in {'MADD', 'MSUB'}: reads.update('ACC.' + c for c in mask)
            if accumulator: writes.update('ACC.' + c for c in mask)
            else: vf(fd, mask, True)
            if operation not in {'MAX', 'MINI'}:
                # Flag aggregation includes preserved/current pipeline flags;
                # whole-register sets are deliberately conservative here.
                reads.update({'MAC', 'STATUS'}); writes.update({'MAC', 'STATUS'})
            dest = 'ACC' if accumulator else f'vf{fd:02d}'
            args = f'vf{fs:02d}.{mask}, {right}'
            if operation in {'MADD', 'MSUB'}: args = f'ACC.{mask}, ' + args
            expression = f'{dest}.{mask} = VU_{operation}({args})'
        else: classified = False
    elif op in {'IADD', 'ISUB', 'IAND', 'IOR'}:
        vi(is_); vi(it); vi(id_, True)
        expression = f'vi{id_:02d} = low16({op}(vi{is_:02d}, vi{it:02d}))'
    elif op in {'IADDI', 'IADDIU', 'ISUBIU'}:
        vi(is_); vi(it, True)
        imm = ((code >> 10) & 0x7800) | (code & 0x7FF)
        if op == 'IADDI':
            imm = (code >> 6) & 31
            if imm & 16: imm -= 32
        expression = f'vi{it:02d} = low16({op}(vi{is_:02d}, {imm}))'
    elif op in {'MOVE', 'MR32'}:
        vf(fs, ''.join(LANES[(LANES.index(c) + 1) % 4] for c in mask) if op == 'MR32' else mask)
        vf(ft, mask, True)
        expression = f'vf{ft:02d}.{mask} = {op}(vf{fs:02d}) (raw words)'
    elif op == 'MFIR':
        vi(is_); vf(ft, mask, True)
        expression = f'vf{ft:02d}.{mask}.bits = sign_extend16(vi{is_:02d})'
    elif op == 'MTIR':
        component = LANES[(code >> 21) & 3]
        vf(fs, component); vi(it, True)
        expression = f'vi{it:02d} = low16(vf{fs:02d}.{component}.bits)'
    elif op in {'LQ', 'LQI', 'LQD', 'SQ', 'SQI', 'SQD', 'ILW', 'ILWR', 'ISW', 'ISWR'}:
        load = op.startswith('L') or op.startswith('IL')
        integer = op.startswith('I')
        base = it if op.startswith('SQ') else is_
        update = 1 if op in {'LQI', 'SQI'} else -1 if op in {'LQD', 'SQD'} else 0
        offset = sign11(code) if op in {'LQ', 'SQ', 'ILW', 'ISW'} else 0
        access('load' if load else 'store', base, offset, update, width=16 if integer and load else 32,
               signed=op in {'LQ', 'SQ', 'ILW', 'ISW'})
        if integer: vi(it, load)
        elif load: vf(ft, mask, True)
        else: vf(fs, mask)
        if op in {'ISW', 'ISWR'}: notes.append('selected32-bit memory words contain vi low16 and zero upper16')
        if op in {'ILW', 'ILWR'}:
            notes.append('multiple selected lanes overwrite oneVI; final selected lane wins')
        expression = (f'{op}.{mask}(vi{base:02d}, {offset}); '
                      f'base update {update if base else 0:+d} (requested {update:+d})')
    elif op in {'DIV', 'SQRT', 'RSQRT'}:
        if op != 'SQRT': vf(fs, LANES[(code >> 21) & 3])
        vf(ft, LANES[(code >> 23) & 3])
        reads.add('STATUS'); writes.update({'Q.pending', 'STATUS'})
        expression = f'Q.pending = VU_{op}(selectedVFcomponents)'
        notes.append('Q result is pending; latency/exception/control-state not simulated')
    elif op == 'WAITQ':
        reads.add('Q.pending'); writes.add('Q')
        expression = 'WAITQ (pending Q completion barrier)'
    elif op in {'FCAND', 'FCEQ', 'FCOR'}:
        reads.add('CLIP'); vi(1, True)
        expression = f'vi01 = {op}(CLIP, 0x{code & 0xFFFFFF:06X})'
    elif op == 'FCSET':
        writes.add('CLIP'); expression = f'CLIP = 0x{code & 0xFFFFFF:06X}'
    elif op == 'FCGET':
        reads.add('CLIP'); vi(it, True); expression = f'vi{it:02d} = low12(CLIP)'
    elif op in {'FMAND', 'FMEQ', 'FMOR'}:
        reads.add('MAC'); vi(is_); vi(it, True)
        expression = f'vi{it:02d} = {op}(MAC, vi{is_:02d})'
    elif op in {'B', 'BAL', 'JR', 'JALR', 'IBEQ', 'IBNE', 'IBLTZ', 'IBGTZ', 'IBLEZ', 'IBGEZ'}:
        if op in {'JR', 'JALR', 'IBEQ', 'IBNE', 'IBLTZ', 'IBGTZ', 'IBLEZ', 'IBGEZ'}: vi(is_)
        if op in {'IBEQ', 'IBNE'}: vi(it)
        if op in {'BAL', 'JALR'}: vi(it, True)
        expression = f'{op} after one delay pair'
        notes.append('branch VI backup/latency and branch-in-delay behavior are not evaluated')
    elif op in {'XTOP', 'XITOP'}:
        reads.add('VIF.TOP' if op == 'XTOP' else 'VIF.ITOP'); vi(it, True)
        expression = f'vi{it:02d} = low16(VIF.{"TOP" if op == "XTOP" else "ITOP"})'
    elif op == 'XGKICK':
        vi(is_)
        memory.append(dict(kind='gif-kick', base=f'vi{is_:02d}', qwordOffset=0,
            byteLength=None, hardwareWrapApplied=False))
        expression = f'XGKICK(vi{is_:02d}); GIF packet length/state unresolved'
    else: classified = False
    return dict(opcode='LOI' if immediate else op, classified=classified,
        reads=sorted(reads), writes=sorted(writes), constantReads=sorted(constants),
        ignoredConstantWrites=sorted(ignored), memory=memory, expression=expression, notes=notes)


def branch(row: dict, index: int) -> dict | None:
    if row['upperWord'] & 0x80000000: return None
    op = opcode(row['lowerSymbol'], row['lowerText'])
    code = row['lowerWord']
    if op in {'B', 'BAL', 'IBEQ', 'IBNE', 'IBLTZ', 'IBGTZ', 'IBLEZ', 'IBGEZ'}:
        return dict(kind='direct', target=index + 1 + sign11(code), conditional=op not in {'B', 'BAL'},
                    linkReturn=index + 2 if op == 'BAL' else None)
    if op in {'JR', 'JALR'}:
        return dict(kind='indirect', register=f'vi{(code >> 11) & 15:02d}',
                    linkReturn=index + 2 if op == 'JALR' else None)
    return None


def pair_priority(row: dict) -> dict:
    """Pinned interpreter pair arbitration, independently expressed.

    Metadata tests whole VF register numbers, even for disjoint/empty masks.
    This records a reference model, not EE hardware/pipeline qualification.
    """
    uop = opcode(row['upperSymbol'], row['upperText'])
    lop = opcode(row['lowerSymbol'], row['lowerText'])
    uc, lc = row['upperWord'], row['lowerWord']
    upper = footprint(uop, uc, True)
    immediate = bool(uc & 0x80000000)
    lower = footprint(lop, lc, False, immediate)
    if not upper['classified'] or not lower['classified']:
        return dict(resolved=False, referenceModel='pinned-PCSX2-interpreters')
    if immediate:
        return dict(resolved=True, referenceModel='pinned-PCSX2-interpreters',
            lowerExecution='publish-I-after-upper', discardedLower=False,
            lowerOldVFRegisters=[], lowerOldClip=False,
            effectiveLowerWrites=['I'], effectiveLowerMemory=[])
    binary = re.fullmatch(r'(ADD|SUB|MUL|MADD|MSUB|MAX|MINI)(A?)([xyzwiq]?)', uop or '')
    unary = {'ABS', 'FTOI0', 'FTOI4', 'FTOI12', 'FTOI15', 'ITOF0', 'ITOF4', 'ITOF12', 'ITOF15'}
    uw = ((uc >> 16) & 31) if uop in unary else (
        ((uc >> 6) & 31) if uop == 'OPMSUB' or (binary and not binary.group(2)) else 0)
    lw = (lc >> 16) & 31 if lop in {'MOVE', 'MR32', 'MFIR', 'LQ', 'LQI', 'LQD'} else 0
    lr = set()
    if lop in {'MOVE', 'MR32', 'MTIR', 'SQ', 'SQI', 'SQD', 'DIV', 'RSQRT'}: lr.add((lc >> 11) & 31)
    if lop in {'DIV', 'SQRT', 'RSQRT'}: lr.add((lc >> 16) & 31)
    clip_write = uop == 'CLIP' and lop == 'FCSET'
    discarded = bool((uw and uw == lw) or clip_write)
    reasons = []
    if uw and uw == lw: reasons.append('same-whole-VF-destination')
    if clip_write: reasons.append('same-CLIP-destination')
    return dict(resolved=True, referenceModel='pinned-PCSX2-interpreters',
        upperVFWriteRegister=uw, lowerVFWriteRegister=lw,
        lowerExecution='discard-entire-lower' if discarded else 'execute-with-conflicting-old-inputs',
        discardedLower=discarded, discardReasons=reasons,
        lowerOldVFRegisters=[f'vf{uw:02d}'] if not discarded and uw and uw in lr else [],
        lowerOldClip=bool(not discarded and uop == 'CLIP' and lop in {'FCAND', 'FCEQ', 'FCOR', 'FCGET'}),
        effectiveLowerWrites=[] if discarded else lower['writes'],
        effectiveLowerMemory=[] if discarded else lower['memory'])


def direct_graph(rows: list[dict], entry: int = 0) -> dict:
    """Context states keep a branch's delay pair distinct from normal entry.

    Conditional outcomes are both possible. Unknown indirect targets and a
    branch in a delay slot are unresolved exits, never guessed fallthroughs.
    """
    if not 0 <= entry < len(rows) <= 2048: raise ValueError('bounded entry/image required')
    queue = deque([(entry, None, False)])
    seen, edges, exits, gaps = set(), set(), [], []
    while queue:
        state = queue.popleft()
        if state in seen: continue
        if len(seen) >= len(rows) * 8: raise ValueError('control state bound exceeded')
        seen.add(state)
        pc, pending, end_pending = state
        row = rows[pc]
        upper = footprint(opcode(row['upperSymbol'], row['upperText']), row['upperWord'], True)
        lower = footprint(opcode(row['lowerSymbol'], row['lowerText']), row['lowerWord'], False,
                          bool(row['upperWord'] & 0x80000000))
        if not upper['classified'] or not lower['classified']:
            gaps.append(dict(index=pc, reason='unsupported opcode control effects unresolved'))
            continue
        control = branch(row, pc)
        eflag = bool(row['upperWord'] & 0x40000000)
        if row['upperWord'] & 0x38000000:
            gaps.append(dict(index=pc, reason='M/D/T control needs unit/debug environment'))
            continue
        if pending is not None and control is not None:
            gaps.append(dict(index=pc, reason='branch in branch delay pair', pending=pending))
            continue
        next_end = eflag
        if pending is not None:
            if pending == 'indirect':
                if end_pending and not eflag:
                    exits.append(dict(index=pc, kind='end-after-delay', nextControl=pending,
                        postPairState=dict(nextIndex=None, pendingTarget=None, endCountdown=0),
                        referenceResumeIndex=None, resumeResolved=False))
                    continue
                gaps.append(dict(index=pc, reason='indirect target after delay'))
                continue
            destinations = [(pending, None, next_end)]
        elif control:
            pending_targets = [control['target']] if control['kind'] == 'direct' else ['indirect']
            if control.get('conditional'): pending_targets.append(pc + 2)
            destinations = [(pc + 1, target, next_end) for target in pending_targets]
        else: destinations = [(pc + 1, None, next_end)]
        if end_pending and not eflag:
            # Both pinned interpreters install an expiring branch BEFORE E
            # stop. Their outer Execute loop additionally resolves a branch
            # initiated in the E-delay pair before leaving the stopped unit.
            # Preserve both pair-local and outer-loop states; this is not
            # hardware or observed MSCNT producer qualification.
            for next_pc, next_pending, _ in destinations:
                resume = next_pc if next_pending is None else next_pending
                exits.append(dict(index=pc, kind='end-after-delay', nextControl=pending,
                    postPairState=dict(nextIndex=next_pc, pendingTarget=next_pending, endCountdown=0),
                    referenceResumeIndex=resume if isinstance(resume, int) else None,
                    resumeResolved=isinstance(resume, int)))
            continue
        for dest in destinations:
            if not 0 <= dest[0] < len(rows):
                gaps.append(dict(index=pc, reason='target outside extracted image', target=dest[0]))
                continue
            edges.add((state, dest)); queue.append(dest)
    def encode(state): return [state[0], state[1], state[2]]
    return dict(entry=entry, reachableIndices=sorted({s[0] for s in seen}),
        states=[encode(s) for s in sorted(seen, key=repr)],
        edges=[dict(source=encode(a), target=encode(b)) for a, b in sorted(edges, key=repr)],
        exits=exits, unresolved=gaps, conditionalOutcomes='both',
        indirectTargetsResolved=False, hardwareWrapApplied=False, unvisitedProvenUnreachable=False)


def end_continuations(rows: list[dict], initial: dict) -> dict:
    """Explore statically known stopped-unit resume entries, no data state.

    Distinct entries are visited once, with64 segment and8*image cumulative
    context bounds. Exhaustion stays an explicit gap instead of partial success.
    """
    queue, discovered, graphs, links, gaps = deque([initial['entry']]), {initial['entry']}, [], [], []
    states = 0
    while queue:
        entry = queue.popleft()
        if len(graphs) >= 64:
            gaps.append(dict(entry=entry, reason='continuation segment bound exceeded'))
            continue
        graph = initial if entry == initial['entry'] else direct_graph(rows, entry)
        if states + len(graph['states']) > len(rows) * 8:
            gaps.append(dict(entry=entry, reason='continuation context bound exceeded'))
            continue
        graphs.append(graph); states += len(graph['states'])
        for exit_ in graph['exits']:
            target = exit_['referenceResumeIndex']
            if target is None or not 0 <= target < len(rows):
                gaps.append(dict(entry=graph['entry'], index=exit_['index'],
                    reason='unresolved or outside-image stopped-unit resume', target=target))
                continue
            links.append(dict(entry=graph['entry'], index=exit_['index'], resumeEntry=target))
            if target not in discovered:
                discovered.add(target); queue.append(target)
    return dict(referenceModel='pinned-PCSX2-interpreter-Execute', segments=graphs,
        resumeEdges=links, unresolved=gaps, carriesRegisterValues=False,
        actualMscntSequenceQualified=False, hardwareQualified=False)


def analyze(rows: list[dict]) -> dict:
    if not 0 < len(rows) <= 2048: raise ValueError('bounded image required')
    instructions = []
    reads, writes, memory, unsupported = Counter(), Counter(), Counter(), []
    for index, row in enumerate(rows):
        upper = footprint(opcode(row['upperSymbol'], row['upperText']), row['upperWord'], True)
        lower = footprint(opcode(row['lowerSymbol'], row['lowerText']), row['lowerWord'], False,
                          bool(row['upperWord'] & 0x80000000))
        for half, facts in [('upper', upper), ('lower', lower)]:
            reads.update(facts['reads']); writes.update(facts['writes'])
            memory.update(m['kind'] for m in facts['memory'])
            if not facts['classified']: unsupported.append(dict(index=index, half=half, opcode=facts['opcode']))
        potential = sorted((set(upper['writes']) & (set(lower['reads']) | set(lower['writes'])))
                           | (set(lower['writes']) & (set(upper['reads']) | set(upper['writes']))))
        instructions.append(dict(index=index, pc=index * 8, upper=upper, lower=lower,
            samePairPotentialDependencies=potential, pairPriority=pair_priority(row), branch=branch(row, index)))
    initial = direct_graph(rows)
    return dict(instructions=instructions, directControlFlow=initial,
        stoppedUnitContinuations=end_continuations(rows, initial),
        summary=dict(pairs=len(rows), classifiedHalves=len(rows) * 2 - len(unsupported),
            unsupported=unsupported, potentialReadOccurrences=dict(sorted(reads.items())),
            potentialWriteOccurrences=dict(sorted(writes.items())), memoryOccurrences=dict(memory)),
        scope=dict(operandFootprints=True, symbolicMemory=True, scheduledReachingDefinitions=False,
            numericSemantics=False, unitQualified=False, executesVu=False,
            pairPriorityReferenceModel=True, stoppedUnitContinuationReferenceModel=True,
            dualIssueDiscardPriorityQualified=False, wholeProgramRecovered=False))


def pseudocode(document: dict) -> str:
    lines = ['# Static operand-transfer pseudocode; VU_* arithmetic is deliberately opaque.',
             '# Potential effects are separate from pinned-reference pair arbitration; flag pipelines remain unresolved.',
             '# Unknown indirect targets, physical address wrap and GIF state remain explicit.']
    for row in document['instructions']:
        lines.append(f'L_{row["index"]:04X}: {row["upper"]["expression"] or "UNCLASSIFIED"} '
                     f'|| {row["lower"]["expression"] or "UNCLASSIFIED"}')
        priority = row['pairPriority']
        if priority['resolved']:
            lines.append(f'  reference pair: {priority["lowerExecution"]}; '
                         f'oldVF={priority["lowerOldVFRegisters"]}; oldCLIP={priority["lowerOldClip"]}')
        for half in ('upper', 'lower'):
            facts = row[half]
            if facts['reads'] or facts['writes']:
                lines.append(f'  {half}: read={facts["reads"]}; write={facts["writes"]}')
    return '\n'.join(lines) + '\n'


def run(isa_report: Path, isa_directory: Path, source_directory: Path,
        output: Path, report_path: Path) -> dict:
    output = bounded_destination(output, ROOT / 'local-data')
    report_path = bounded_destination(report_path, ROOT / '.private')
    contract = json.loads(read_bounded(CONTRACT, MAX_METADATA_BYTES))
    for source in contract['files']:
        data = read_bounded(source_directory / source['name'], MAX_METADATA_BYTES)
        if digest(data) != source['sha256']: raise ValueError('reference source hash mismatch')
    raw_report = read_bounded(isa_report, MAX_METADATA_BYTES)
    prior = json.loads(raw_report)
    if prior.get('kind') != 'bounded-static-vu-isa-decode' or not 0 < len(prior['programs']) <= 64:
        raise ValueError('bounded original ISA report required')
    results, rendered, stems = [], {}, set()
    for program in prior['programs']:
        names = [name for name in program['files'] if name.endswith('.isa.json')]
        if len(names) != 1: raise ValueError('one ISA document per program required')
        name = names[0]
        if Path(name).name != name or '\\' in name: raise ValueError('flat filename required')
        stem = name.removesuffix('.isa.json')
        if not stem or stem.casefold() in stems: raise ValueError('distinct output stems required')
        stems.add(stem.casefold())
        raw = read_bounded(isa_directory / name, MAX_METADATA_BYTES)
        if digest(raw) != program['files'][name]: raise ValueError('ISA document identity mismatch')
        document = json.loads(raw); rows = document['instructions']
        if not isinstance(rows, list) or not 0 < len(rows) <= 2048: raise ValueError('bounded image required')
        code = reconstruct_encodings(rows); validate_rows(code, rows)
        if digest(code) != program['inputSha256']: raise ValueError('raw instruction identity mismatch')
        result = analyze(rows)
        result['inputSha256'] = digest(code)
        files = {stem + '.flow.json': (json.dumps(result, indent=2) + '\n').encode(),
                 stem + '.pseudo.txt': pseudocode(result).encode()}
        if rendered.keys() & files.keys(): raise ValueError('duplicate output name')
        rendered.update(files)
        results.append(dict(input=name, inputSha256=digest(code), summary=result['summary'],
            reachableUnderDirectEdges=len(result['directControlFlow']['reachableIndices']),
            unresolvedControl=result['directControlFlow']['unresolved'],
            files={name: digest(data) for name, data in files.items()}))
    output.mkdir(parents=True)
    for name, data in rendered.items():
        path = output / name; path.write_bytes(data)
        if digest(path.read_bytes()) != digest(data): raise ValueError('output readback mismatch')
    report = dict(schema=1, kind='bounded-vu-static-dataflow', isaReportSha256=digest(raw_report),
        toolSha256=digest(Path(__file__).read_bytes()), sourceContractSha256=digest(CONTRACT.read_bytes()),
        programs=results, imageCount=len(results), pairs=sum(p['summary']['pairs'] for p in results),
        unsupportedHalves=sum(len(p['summary']['unsupported']) for p in results),
        scope=result['scope'], provenance=contract)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with report_path.open('x', encoding='utf-8') as stream: json.dump(report, stream, indent=2); stream.write('\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('isa-report', 'isa-directory', 'source-directory', 'output', 'report'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    report = run(args.isa_report, args.isa_directory, args.source_directory, args.output, args.report)
    print(json.dumps({key: report[key] for key in ('imageCount', 'pairs', 'unsupportedHalves')}))


if __name__ == '__main__': main()

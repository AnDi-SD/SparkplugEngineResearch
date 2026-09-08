#!/usr/bin/env python3
"""Execute only the PS2 spAnimTrack interval leaf in a bounded MIPS guest.

No EE/MMI/VU interpretation is attempted: this leaf uses ordinary integer/FPU
instructions, has no callees/imports and never launches an ELF/game process.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
import time

from inspect_serializer_manager import PS2_SHA256, read_elf, image_slice
from pc_instruction_emulator import ROOT, run_bounded

ENTRY, SIZE = 0x11f980, 0x10c
RETURN, DATA = 0x20000000, 0x21000000


def guest(output):
    if not output.resolve().is_relative_to(ROOT/'local-data/results'):
        raise ValueError('Local result path required')
    started = time.perf_counter()
    raw = (ROOT/'local-data/Winx Club the game PS2/SLES_532.19').read_bytes()
    if hashlib.sha256(raw).hexdigest().upper() != PS2_SHA256:
        raise ValueError('Pristine PS2 ELF required')
    body = image_slice(raw, read_elf(raw), ENTRY, SIZE)
    sys.path[:0] = [str(ROOT/'.codex-tmp/emulation-python')]
    import unicorn
    from unicorn import mips_const as registers
    machine = unicorn.Uc(unicorn.UC_ARCH_MIPS, unicorn.UC_MODE_MIPS64 | unicorn.UC_MODE_LITTLE_ENDIAN)
    machine.ctl_set_cpu_model(registers.UC_CPU_MIPS64_R4000)
    # Synthetic CPU state supplies an enabled FPU; this is not a PS2 startup.
    machine.reg_write(registers.UC_MIPS_REG_CP0_STATUS,
                      machine.reg_read(registers.UC_MIPS_REG_CP0_STATUS) | (1 << 29))
    machine.mem_map(ENTRY & ~4095, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
    machine.mem_write(ENTRY, body)
    machine.mem_map(RETURN, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_EXEC)
    machine.mem_map(DATA, 4096, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
    instruction_count = [0]
    last_addresses = []

    def code(uc, address, size, user):
        if not ENTRY <= address < ENTRY+SIZE:
            raise RuntimeError(f'Guest left audited interval leaf: {address:#x}')
        instruction_count[0] += 1
        last_addresses.append(address)
        del last_addresses[:-8]

    def interrupt(uc, number, user):
        raise RuntimeError(f'No guest interrupt/OS forwarding: {number}, PC={uc.reg_read(registers.UC_MIPS_REG_PC):#x}, last={list(map(hex,last_addresses))}')

    machine.hook_add(unicorn.UC_HOOK_CODE, code)
    machine.hook_add(unicorn.UC_HOOK_INTR, interrupt)
    rows = []
    for times in ((0.0,), (0.0, 1.0), (.25, .75), (0.0, 1.0, 2.0), (0.0, .25, .75, 1.5)):
        machine.mem_write(DATA, struct.pack('<'+'f'*len(times), *times))
        cache = 0
        for seconds in (-.25, 0, .125, .25, .5, .75, 1, 1.5, 2, 3, .5, .125, 0):
            previous = cache
            machine.mem_write(DATA+256, struct.pack('<f', 999))
            for name, value in {'A1': DATA, 'A2': len(times), 'A3': DATA+256, 'T0': cache,
                                'RA': RETURN, 'F12': struct.unpack('<I', struct.pack('<f', seconds))[0]}.items():
                machine.reg_write(getattr(registers, 'UC_MIPS_REG_'+name), value)
            before = instruction_count[0]
            machine.emu_start(ENTRY, RETURN, timeout=100_000, count=1_000)
            if machine.reg_read(registers.UC_MIPS_REG_PC) != RETURN:
                raise RuntimeError('Guest interval exhausted its 100ms/1000-instruction limit')
            cache = machine.reg_read(registers.UC_MIPS_REG_V0)
            factor = struct.unpack('<f', machine.mem_read(DATA+256, 4))[0]
            if seconds <= times[0] or len(times) == 1:
                expected = (0, 0.0)
            elif seconds >= times[-1]:
                expected = (0, 0.0) if len(times) == 2 else (len(times)-2, 1.0)
            else:
                index = max(i for i, value in enumerate(times) if value <= seconds)
                expected = (index, (seconds-times[index])/(times[index+1]-times[index]))
            assert cache == expected[0] and abs(factor-expected[1]) < 1e-6, (times, seconds, cache, factor, expected)
            rows.append({'times': times, 'seconds': seconds, 'previousIndex': previous,
                         'index': cache, 'fraction': factor, 'instructions': instruction_count[0]-before})
    report = {'status': 'passed', 'ps2Sha256': PS2_SHA256, 'entry': f'0x{ENTRY:08X}',
              'bodySize': SIZE, 'bodySha256': hashlib.sha256(body).hexdigest().upper(),
              'probeSha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
              'rows': rows, 'cases': len(rows), 'instructions': instruction_count[0],
              'seconds': time.perf_counter()-started,
              'guest': 'Unicorn MIPS64 R4000 little endian; ordinary integer and float32 instruction leaf only',
              'limits': ['1000 instructions / 100ms per call; 30s separate process',
                         'no callees, imports, interrupts, loader, MMI/VU or game execution',
                         'finite increasing times; nonnegative valid cache; empty/negative-cache inputs excluded',
                         'ABI arguments inferred from the leaf; serializer/Squad/whole PS2 playback not executed']}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('status', 'cases', 'instructions', 'seconds')}))
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--guest', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raise SystemExit(guest(args.output) if args.guest else run_bounded(Path(__file__), ['--output', str(args.output)]))

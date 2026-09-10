#!/usr/bin/env python3
"""Bounded guest-only x86 evidence harness; never loads or starts WinxClub.

Optional dependencies are local, not installed automatically: pefile/capstone
in local-data/research-cache/python, Unicorn 2.1.4 in .codex-tmp/emulation-python.
Unicorn API: https://www.unicorn-engine.org/docs/tutorial.html
The PE loader/OS are NOT emulated. Imports, interrupts and privileged I/O are
not forwarded to the host. Synthetic memory and explicit seams are evidence
fixtures, not proof of an in-game integration. Use run_bounded for process cap.
"""
from __future__ import annotations

import collections
import math
from pathlib import Path
import struct
import subprocess
import sys

from inspect_serializer_manager import PC_SHA256, sha256

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x400000
STACK = 0x30000000
HEAP = 0x31000000
RETURN = 0x32000000
ARENA_SIZE = 0x10000
INTEGRATION_ARENA_SIZE = 0x20000
CHARACTER_ARENA_SIZE = 0x40000
INSTRUCTION_LIMIT = 100_000
TIMEOUT_US = 2_000_000
PROCESS_TIMEOUT = 30
FILE_INSTRUCTION_LIMIT = 1_000_000
FILE_TIMEOUT_US = 8_000_000


def execution_limits(profile):
    """Explicit fresh-guest profiles; historical micro limits remain default."""
    if profile == 'micro': return INSTRUCTION_LIMIT, TIMEOUT_US
    if profile == 'file': return FILE_INSTRUCTION_LIMIT, FILE_TIMEOUT_US
    if profile == 'character': return 4_000_000, 16_000_000
    # Research-only constructor slice: measured original byte-loop exit at
    # 4,683,530 instructions. Explicitly authorized 10 September 2026; fresh
    # guests only, unchanged external 30-second child and memory bounds.
    if profile == 'protected-constructor': return 6_000_000, 24_000_000
    raise ValueError('Explicit micro, file, character or protected-constructor execution profile required')


def run_bounded(script: Path, arguments=()) -> int:
    """Only a separate Python guest harness runs; never a game process."""
    try:
        return subprocess.run([sys.executable, str(script), '--guest', *arguments],
                              cwd=ROOT, timeout=PROCESS_TIMEOUT).returncode
    except subprocess.TimeoutExpired:
        print('FAIL: guest evidence process exceeded 30 seconds', file=sys.stderr)
        return 2


class PcInstructions:
    def __init__(self, path: Path | None = None, *, arena_size: int = ARENA_SIZE,
                 execution_profile: str = 'micro', code_cache_mode: str = 'page'):
        execution_limits(execution_profile)
        if code_cache_mode not in ('bytes', 'page'):
            raise ValueError('Explicit bytes or page instruction cache required')
        self.code_cache_mode = code_cache_mode
        self.execution_profile = execution_profile
        if arena_size not in (ARENA_SIZE, INTEGRATION_ARENA_SIZE, CHARACTER_ARENA_SIZE):
            raise ValueError('Explicit 64KiB, 128KiB or 256KiB arena required')
        self.arena_size = arena_size
        raw = (path or ROOT/'local-data/pc-pristine/WinxClub.exe').read_bytes()
        if sha256(raw) != PC_SHA256:
            raise ValueError('Refusing non-pristine PC image')
        sys.path[:0] = [str(ROOT/'.codex-tmp/emulation-python'),
                        str(ROOT/'local-data/research-cache/python')]
        import pefile
        import capstone
        import unicorn
        from unicorn import x86_const
        self.uc, self.xr = unicorn, x86_const
        # Header/section mapping only: no loader imports/resources are used.
        # CP117 checks the complete mapped-image hash against full parsing.
        pe = pefile.PE(data=raw, fast_load=True)
        size = (pe.OPTIONAL_HEADER.SizeOfImage+4095) & ~4095
        if pe.OPTIONAL_HEADER.ImageBase != BASE or not 0 < size < 0x4000000:
            raise ValueError('Unexpected image mapping')
        self.size = size
        self.mu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        # RWX is guest memory only, required for the original self-modifying
        # bridge pointer resolution. No guest address is a host code pointer.
        self.mu.mem_map(BASE, size)
        self.mu.mem_write(BASE, pe.get_memory_mapped_image())
        self.mu.mem_map(STACK, ARENA_SIZE, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
        self.mu.mem_map(HEAP, self.arena_size, unicorn.UC_PROT_READ | unicorn.UC_PROT_WRITE)
        self.decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.code_cache = {}
        self.code_cache_pages = {}
        if code_cache_mode == 'page':
            # API mem_write does not invoke Unicorn's guest-write hook. Cover
            # both paths; instructions straddling pages belong to both sets.
            write_memory, unmap_memory = self.mu.mem_write, self.mu.mem_unmap
            def write(address, data):
                self._invalidate_code(address, len(data), host=True)
                return write_memory(address, data)
            def unmap(address, size):
                self._invalidate_code(address, size, host=True)
                return unmap_memory(address, size)
            self.mu.mem_write, self.mu.mem_unmap = write, unmap
            self.mu.hook_add(unicorn.UC_HOOK_MEM_WRITE, self._guest_write,
                             begin=BASE, end=BASE+size-1)
        self.tail = collections.deque(maxlen=12)
        self.seams = {}  # address -> explicit fixture callback, no default API shim
        self.visits = collections.Counter()
        self.mu.hook_add(unicorn.UC_HOOK_CODE, self._code)
        self.mu.hook_add(unicorn.UC_HOOK_MEM_INVALID, self._invalid)
        self.mu.hook_add(unicorn.UC_HOOK_INTR, self._interrupt)
        self.reset_arena()

    def reset_arena(self):
        self.mu.mem_write(HEAP, bytes(self.arena_size))
        self.allocated = 0

    def allocate(self, size: int) -> int:
        aligned = (size+15) & ~15
        if size <= 0 or self.allocated+aligned > self.arena_size:
            raise ValueError('Synthetic arena exhausted')
        address = HEAP+self.allocated
        self.allocated += aligned
        return address

    def uint(self, address: int) -> int:
        return struct.unpack('<I', self.mu.mem_read(address,4))[0]

    def put_uint(self, address: int, value: int):
        self.mu.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def floats(self, address: int, count: int) -> tuple[float,...]:
        return struct.unpack('<'+'f'*count, self.mu.mem_read(address,count*4))

    def put_floats(self, address: int, values):
        self.mu.mem_write(address, struct.pack('<'+'f'*len(values), *values))

    def reg(self, name: str) -> int:
        return self.mu.reg_read(getattr(self.xr,'UC_X86_REG_'+name))

    def set_reg(self, name: str, value: int):
        self.mu.reg_write(getattr(self.xr,'UC_X86_REG_'+name),value)

    def _stop(self, reason):
        self.reason = reason
        self.mu.emu_stop()

    def _invalidate_code(self, address, size, *, host=False):
        if size <= 0 or address >= BASE+self.size or address+size <= BASE:
            return
        first, last = max(address, BASE)>>12, min(address+size-1, BASE+self.size-1)>>12
        for page in range(first, last+1):
            entries = self.code_cache_pages.pop(page, ())
            if entries and host:
                # API writes can retain a stale translated instruction size.
                # Guest writes already invalidate Unicorn translations itself.
                self.mu.ctl_remove_cache(page<<12, (page+1)<<12)
            for entry in entries:
                self.code_cache.pop(entry, None)

    def _guest_write(self, mu, access, address, size, value, user):
        self._invalidate_code(address, size)

    def _mnemonic(self, mu, address, length):
        if self.code_cache_mode == 'page':
            cached = self.code_cache.get(address)
            if cached is not None and cached[0] == length:
                return cached[1]
            raw = bytes(mu.mem_read(address, length))
            ins = next(self.decoder.disasm(raw, address), None)
            mnemonic = ins.mnemonic if ins else 'invalid'
            self.code_cache[address] = (length, mnemonic)
            for page in range(address>>12, ((address+length-1)>>12)+1):
                self.code_cache_pages.setdefault(page, set()).add(address)
            return mnemonic
        raw = bytes(mu.mem_read(address, length))
        key = (address, raw)
        if key not in self.code_cache:
            ins = next(self.decoder.disasm(raw, address), None)
            self.code_cache[key] = ins.mnemonic if ins else 'invalid'
        return self.code_cache[key]

    def _code(self, mu, address, length, _):
        self.visits[address] += 1
        self.tail.append(address)
        if address == self.stop_at:
            self._stop('requested boundary')
            return
        if address in self.seams:
            self.seams[address](self)
            return
        if not BASE <= address < BASE+self.size:
            self._stop('external execution denied')
            return
        if self._mnemonic(mu, address, length) in {
            'invalid','syscall','sysenter','int','int1','int3','in','out',
            'insb','insw','insd','outsb','outsw','outsd','hlt','cli','sti',
        }:
            self._stop('OS/privileged instruction denied')

    def _invalid(self, mu, access, address, size, value, _):
        self.reason = f'invalid memory {access} at {address:#x}, size {size}'
        return False  # no permissive map-on-fault

    def _interrupt(self, mu, number, _):
        self._stop(f'interrupt {number} denied')

    def fixture_return(self, argument_bytes: int = 0, eax: int | None = None):
        """Explicit seam helper. A caller must document each replaced callee."""
        sp = self.reg('ESP')
        self.set_reg('EIP', self.uint(sp))
        self.set_reg('ESP', sp+4+argument_bytes)
        if eax is not None:
            self.set_reg('EAX',eax)

    def fixture_seh_chain(self):
        """Explicit synthetic FS:0 storage for non-throwing MSVC SEH prologues.

        This is NOT a Windows TEB/loader or exception dispatcher. A separate
        guest data segment keeps linear address zero unmapped. No API forwarding
        or map-on-fault is enabled. Calling twice resets only the chain sentinel.
        """
        gdt, teb = 0x33000000, 0x33001000
        if not getattr(self, '_seh_fixture', False):
            rw = self.uc.UC_PROT_READ | self.uc.UC_PROT_WRITE
            self.mu.mem_map(gdt, 0x2000, rw)
            # 32-bit, present, writable data segment with a 4096-byte limit.
            descriptor = struct.pack('<HHBBBB', 0xfff, teb & 0xffff,
                                     (teb >> 16) & 0xff, 0x93, 0x40, teb >> 24)
            self.mu.mem_write(gdt+8, descriptor)
            self.mu.mem_write(gdt+16, struct.pack('<HHBBBB', 0xffff, 0, 0, 0x93, 0xcf, 0))
            self.mu.mem_write(gdt+24, struct.pack('<HHBBBB', 0xffff, 0, 0, 0x9b, 0xcf, 0))
            self.mu.reg_write(self.xr.UC_X86_REG_GDTR, (0, gdt, 0x1f, 0))
            # Loading FS also activates segment-cache semantics in Unicorn;
            # retain a flat 32-bit stack/code rather than its initial null SS.
            for name in ('DS', 'ES', 'SS'):
                self.set_reg(name, 16)
            self.set_reg('CS', 24)
            self.set_reg('FS', 8)
            self._seh_fixture = True
        self.put_uint(teb, 0xffffffff)
        return teb

    def fixture_push_x87(self, value: float):
        """Finite-value CRT/math seam helper, not original engine evidence.

        Unicorn FP0..7 are physical (mantissa:uint64, exponent:uint16) slots.
        Push updates TOP and the full x87 tag word, preserving older values.
        """
        if not math.isfinite(value):
            raise ValueError('The explicit x87 seam supports finite values only')
        fraction, exponent = math.frexp(abs(value))
        encoded = (int(math.ldexp(fraction,64)), exponent-1+16383) if value else (0,0)
        if math.copysign(1,value)<0:
            encoded = encoded[0], encoded[1] | 0x8000
        top = ((self.reg('FPSW')>>11)-1)&7
        self.mu.reg_write(getattr(self.xr,'UC_X86_REG_FP'+str(top)),encoded)
        self.set_reg('FPSW',(self.reg('FPSW')&~0x3800)|(top<<11))
        tag = 1 if value==0 else 0
        self.set_reg('FPTAG',(self.reg('FPTAG')&~(3<<(top*2)))|(tag<<(top*2)))

    def run(self, entry: int, this: int = 0, args=(), *, stop_at: int | None = None,
            callee_pop: bool = True):
        instruction_limit, timeout_us = execution_limits(self.execution_profile)
        self.last_execution_limits = {'profile': self.execution_profile,
                                      'instructionLimit': instruction_limit, 'timeoutUs': timeout_us}
        self.mu.mem_write(STACK, bytes(ARENA_SIZE))
        sp = STACK+0xe000
        for index,value in enumerate((RETURN,*args)):
            self.put_uint(sp+4*index,value)
        for name,value in [('ESP',sp),('ECX',this),('EAX',0),('EDX',0),
                           ('EBX',0xb1b1b1b1),('EBP',0xb2b2b2b2),
                           ('EDI',0xb3b3b3b3),('ESI',0xb4b4b4b4),
                           ('EFLAGS',0x202),('FPCW',0x37f),('FPSW',0),('FPTAG',0xffff)]:
            self.set_reg(name,value)
        self.stop_at = stop_at
        self.reason = 'instruction/time cap'
        self.tail.clear()
        self.visits.clear()
        try:
            self.mu.emu_start(entry,RETURN,timeout=timeout_us,count=instruction_limit)
        except self.uc.UcError as error:
            raise AssertionError(f'{self.reason}: {error}; tail={list(map(hex,self.tail))}') from error
        if stop_at is not None and self.reg('EIP') == stop_at:
            return
        expected_sp = sp+4+(4*len(args) if callee_pop else 0)
        if self.reg('EIP') == RETURN and self.reg('ESP') != expected_sp:
            raise AssertionError(f'completed call stack mismatch: expected {expected_sp:#x}, '
                                 f'observed {self.reg("ESP"):#x}; check calling convention; '
                                 f'entry={entry:#x}, instructions={sum(self.visits.values())}')
        if self.reg('EIP') != RETURN or self.reg('ESP') != expected_sp:
            raise AssertionError(f'{self.reason}; ip={self.reg("EIP"):#x}, '
                                 f'sp={self.reg("ESP"):#x}, tail={list(map(hex,self.tail))}')

"""Bounded original PS2 scalar prefixes, explicitly excluding R5900 extras.

This is a convenience wrapper around the existing MIPS64/R4000 experiment,
not a PS2 CPU implementation. SQ/LQ, MMI, COP2 and system interrupts stop it.
The opt-in integer-movz profile uses5KC with an explicit integer allowlist;
it does not enable generic MIPS64 instructions as an R5900 substitute.
Each instance executes once; exact prefix entry/registers/stops are evidence.
"""
import hashlib,struct,sys
from functools import lru_cache
from capture_native_ranges import ROOT,PS2,EXPECTED,read_elf_sections,read_window
sys.path.insert(0,str(ROOT/'.codex-tmp/emulation-python'))
import unicorn
from unicorn import mips_const as registers


@lru_cache(maxsize=1)
def pristine():
    raw=PS2.read_bytes()
    if hashlib.sha256(raw).hexdigest().upper()!=EXPECTED['ps2']:raise ValueError('Pristine PS2 required')
    return raw,read_elf_sections(raw)


class Ps2ScalarPrefix:
    RETURN=0x20000000

    def __init__(self,ranges,*,profile='r4000'):
        ranges=tuple(ranges)
        if not 0<len(ranges)<=64 or sum(n for a,n in ranges)>0x40000:raise ValueError('Explicit bounded code ranges required')
        self.u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN)
        if profile not in ('r4000','integer-movz'):raise ValueError('Reviewed scalar profile required')
        self.profile=profile
        self.u.ctl_set_cpu_model(registers.UC_CPU_MIPS64_R4000 if profile=='r4000' else registers.UC_CPU_MIPS64_5KC)
        self.ranges=ranges;self.pages=set();self.executed=False;self.trace=[];self.stop=None
        raw,sections=pristine()
        for a,n in ranges:
            self.map(a,n);self.write(a,read_window('ps2',raw,a,n,sections)[0])
        self.map(self.RETURN,4096)
        self.reg('RA',self.RETURN)

    def map(self,address,size):
        if not 0<size<=0x10000 or address<0 or address+size>0x100000000:raise ValueError('Explicit mapped window required')
        wanted=set(range(address&~4095,(address+size+4095)&~4095,4096))-self.pages
        if len(self.pages|wanted)>96:raise ValueError('Prefix memory exceeds384KiB')
        for page in sorted(wanted):self.u.mem_map(page,4096)
        self.pages|=wanted

    def reg(self,name,value=None):
        key=getattr(registers,'UC_MIPS_REG_'+name)
        if value is not None:self.u.reg_write(key,value)
        return self.u.reg_read(key)

    def read(self,address,size):return bytes(self.u.mem_read(address,size))
    def write(self,address,data):self.u.mem_write(address,bytes(data))
    def uint(self,address):return int.from_bytes(self.read(address,4),'little')
    def put_uint(self,address,value):self.write(address,struct.pack('<I',value&0xffffffff))

    def run(self,entry,stops,*,count=2000,timeout_us=100000):
        if self.executed:raise ValueError('Discard each prefix guest after its single execution')
        if not 0<count<=100000 or not 0<timeout_us<=2000000:raise ValueError('Explicit instruction/time cap required')
        stops=frozenset(stops)
        if not stops or len(stops)>16:raise ValueError('Explicit1..16 stop addresses required')
        # A code hook cannot observe an unmapped fetch. Map only original words
        # at consumer stops; they are never executed. RETURN is synthetic.
        raw,sections=pristine()
        for a in stops:
            if a!=self.RETURN:
                self.map(a,4);self.write(a,read_window('ps2',raw,a,4,sections)[0])
        self.executed=True
        def observe(u,address,size,user):
            if address in stops:self.stop=address;u.emu_stop();return
            if not any(a<=address and address+4<=a+n for a,n in self.ranges):raise RuntimeError(f'Outside declared original prefix {address:08X}')
            word=self.uint(address)
            if word>>26 in (0x1e,0x1f,0x12,0x1c):raise RuntimeError(f'Excluded R5900 instruction {address:08X}')
            if word>>26==0x11 and (word>>21)&31==16 and 0x18<=word&63<=0x1f:
                raise RuntimeError(f'Unreviewed R5900 COP1 accumulator instruction {address:08X}')
            if self.profile=='integer-movz':
                # SLL/NOP, JR/JALR, MOVZ, ADDU/DADDU; BEQ/BNE, ADDIU,
                # ANDI, LW/LBU, SB/SW, LD/SD. No floating point, HI/LO,
                # multiply/divide, MMI, privileged or device instructions.
                opcode=word>>26
                allowed=(word&63 in (0,8,9,10,0x21,0x2d)) if opcode==0 else opcode in (4,5,9,0xc,0x23,0x24,0x28,0x2b,0x37,0x3f)
                if not allowed:raise RuntimeError(f'Outside reviewed integer-movz ISA {address:08X}: {word:08X}')
            self.trace.append(address)
        def interrupt(u,number,user):raise RuntimeError(f'No PS2 interrupt forwarding {number}; last original {self.trace[-1]:08X}' if self.trace else f'No PS2 interrupt forwarding {number}')
        self.u.hook_add(unicorn.UC_HOOK_CODE,observe)
        self.u.hook_add(unicorn.UC_HOOK_INTR,interrupt)
        self.u.emu_start(entry,0x23000000,timeout=timeout_us,count=count)
        if self.stop is None:raise RuntimeError(f'Prefix instruction/time cap at {self.reg("PC"):08X}')
        return dict(entry=f'{entry:08X}',stop=f'{self.stop:08X}',instructions=len(self.trace),profile=self.profile,
            completion='original return' if self.stop==self.RETURN else 'declared original prefix boundary;guest discarded')

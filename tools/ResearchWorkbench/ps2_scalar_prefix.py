"""Bounded original PS2 scalar prefixes, explicitly excluding R5900 extras.

This is a convenience wrapper around the existing MIPS64/R4000 experiment,
not a PS2 CPU implementation. SQ/LQ, MMI, COP2 and system interrupts stop it
by default. Explicit stack_window/upper64 enable only aligned SP-based SQ/LQ
outside delay slots, with recorded 128-bit state and a scalar ISA allowlist.
SQRT.S is rejected:EE uses FT,while this generic MIPS CPU reads FS.
The opt-in integer-movz profile uses5KC with an explicit integer allowlist;
it does not enable generic MIPS64 instructions as an R5900 substitute.
The opt-in integer-squares profile interprets three accumulator instructions
only for exact bounded integer squares;it records every host-interpreted
instruction,keeps original code bytes,and rejects FCR reads/writes.
Explicit unsigned_division with stack state validates original DIVU/MFHI/MFLO
for nonzero divisors and positive31-bit quotient/remainder;no register seams.
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

    def __init__(self,ranges,*,profile='r4000',stack_window=None,upper64=None,unsigned_division=False,
                 integer_multiply=False,hi1_lo1=None,raw_transfers=False,fpr_words=None,transfer_windows=None,
                 raw_swc1_beq_delay=False,stack_store_delay=False,packet_calls=None,packet_gp=None,
                 scalar_control=False,scalar_control_windows=None):
        ranges=tuple(ranges)
        if not 0<len(ranges)<=64 or sum(n for a,n in ranges)>0x40000:raise ValueError('Explicit bounded code ranges required')
        self.u=unicorn.Uc(unicorn.UC_ARCH_MIPS,unicorn.UC_MODE_MIPS64|unicorn.UC_MODE_LITTLE_ENDIAN)
        if profile not in ('r4000','integer-movz','integer-squares'):raise ValueError('Reviewed scalar profile required')
        self.profile=profile
        self.u.ctl_set_cpu_model(registers.UC_CPU_MIPS64_5KC if profile=='integer-movz' else registers.UC_CPU_MIPS64_R4000)
        self.accumulator_extension=None
        if profile=='integer-squares':
            from ps2_integer_square_accumulator import IntegerSquareAccumulator
            self.accumulator_extension=IntegerSquareAccumulator()
        self.ranges=ranges;self.pages=set();self.executed=False;self.trace=[];self.stop=None
        self.stack_extension=None
        if stack_window is not None or upper64 is not None:
            if stack_window is None or upper64 is None:raise ValueError('Stack window and explicit upper64 state must be supplied together')
            from ps2_stack_spills import StackSpills
            self.stack_extension=StackSpills(stack_window,upper64,store_delay=stack_store_delay)
            start,length=self.stack_extension.window
            if any(start<a+n and a<start+length for a,n in ranges+((self.RETURN,4096),)):
                raise ValueError('Stack window must not overlap original code or RETURN')
        elif stack_store_delay:raise ValueError('Stack store-delay option requires explicit stack state')
        if type(stack_store_delay) is not bool:raise ValueError('Explicit Boolean stack store-delay option required')
        self.division_extension=None
        if type(unsigned_division) is not bool:raise ValueError('Explicit Boolean unsigned_division option required')
        if unsigned_division:
            if self.stack_extension is None or profile=='integer-movz':raise ValueError('Unsigned division requires explicit stack state and R4000-based profile')
            from ps2_unsigned_division import UnsignedDivision
            self.division_extension=UnsignedDivision()
        self.multiply_extension=None;self.raw_transfer_extension=None
        if type(integer_multiply) is not bool or type(raw_transfers) is not bool or type(raw_swc1_beq_delay) is not bool:
            raise ValueError('Explicit Boolean packet seam options required')
        if integer_multiply:
            if self.stack_extension is None or profile!='r4000':raise ValueError('Multiply seam requires explicit stack state and r4000 profile')
            from ps2_integer_multiply import Positive31Multiply
            self.multiply_extension=Positive31Multiply(hi1_lo1)
        elif hi1_lo1 is not None:raise ValueError('HI1/LO1 state requires explicit multiply opt-in')
        if raw_transfers:
            if self.stack_extension is None or profile!='r4000':raise ValueError('Raw transfer seam requires explicit stack state and r4000 profile')
            from ps2_raw_transfers import RawTransfers
            self.raw_transfer_extension=RawTransfers(fpr_words,transfer_windows,beq_store_delay=raw_swc1_beq_delay)
            for start,length,_ in self.raw_transfer_extension.windows:
                if any(start<a+n and a<start+length for a,n in ranges+((self.RETURN,4096),)):
                    raise ValueError('Transfer memory must not overlap original code or RETURN')
        elif fpr_words is not None or transfer_windows is not None or raw_swc1_beq_delay:raise ValueError('Raw FPR state requires explicit transfer opt-in')
        raw,sections=pristine()
        for a,n in ranges:
            self.map(a,n);self.write(a,read_window('ps2',raw,a,n,sections)[0])
        self.map(self.RETURN,4096)
        self.reg('RA',self.RETURN)
        self.call_extension=None
        if packet_calls is not None:
            from ps2_packet_calls import PacketCallBoundaries
            if not isinstance(packet_calls,PacketCallBoundaries):raise ValueError('Only typed external packet primitives supported')
            self.call_extension=packet_calls;packet_calls.attach(self,packet_gp)
        elif packet_gp is not None:raise ValueError('Packet GP requires explicit typed primitive environment')
        if type(scalar_control) is not bool:raise ValueError('Explicit Boolean scalar-control option required')
        self.control_extension=None
        if scalar_control:
            if profile!='r4000' or self.stack_extension is None:raise ValueError('Explicit scalar control requires fresh r4000 profile and stack state')
            from ps2_scalar_control import ScalarControlPairs
            self.control_extension=ScalarControlPairs(scalar_control_windows)
            for start,length,_ in self.control_extension.windows:
                if any(start<a+n and a<start+length for a,n in ranges+((self.RETURN,4096),)):
                    raise ValueError('Control data windows must not overlap code or RETURN')
        elif scalar_control_windows is not None:raise ValueError('Control windows require explicit scalar-control option')

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
            if self.division_extension is not None:self.division_extension.verify_pending(self)
            if address in stops:
                if self.trace and address==self.trace[-1]+4:
                    previous=self.uint(self.trace[-1]);op=previous>>26
                    # Unicorn may finish a MIPS branch delay-slot operation
                    # despite emu_stop from its code hook. Do not certify a
                    # pre-instruction boundary there. Callers can stop at the
                    # preceding branch instead. Direct entry to the same word
                    # from another address is not a delay-slot execution.
                    transfer=(op in (2,3,4,5,6,7,0x14,0x15,0x16,0x17)
                        or op==1 and (previous>>16)&31 in (0,1,2,3,0x10,0x11,0x12,0x13)
                        or op==0 and previous&63 in (8,9)
                        or op in (0x10,0x11,0x12) and (previous>>21)&31==8)
                    if transfer:raise RuntimeError(f'Unqualified stop in branch delay slot {address:08X};stop before branch {self.trace[-1]:08X}')
                self.stop=address;u.emu_stop();return
            if self.call_extension is not None:
                plan=self.call_extension.plan(self,address)
                if plan is not None:
                    if len(self.trace)>=count:raise RuntimeError('Logical instruction cap before external primitive effect')
                    self.call_extension.commit(self,plan);return
            if not any(a<=address and address+4<=a+n for a,n in self.ranges):raise RuntimeError(f'Outside declared original prefix {address:08X}')
            word=self.uint(address)
            if self.control_extension is not None:
                plan=self.control_extension.plan(self,address,word,stops)
                if plan is not None:
                    if len(self.trace)+len(plan['addresses'])>count:raise RuntimeError('Logical instruction cap before interpreted control pair')
                    self.trace.extend(plan['addresses']);self.control_extension.commit(self,plan);return
            if self.stack_extension is not None:
                plan=self.stack_extension.plan(self,address,word,stops)
                if plan is not None:
                    if len(self.trace)+len(plan['addresses'])>count:raise RuntimeError('Logical instruction cap before interpreted stack operation')
                    self.trace.extend(plan['addresses']);self.stack_extension.commit(self,plan);return
                if len(self.trace)>=count:raise RuntimeError('Logical instruction cap in stack-spill profile')
            for ext in (self.multiply_extension,self.raw_transfer_extension):
                if ext is not None:
                    plan=ext.plan(self,address,word,stops)
                    if plan is not None:
                        if len(self.trace)+len(plan['addresses'])>count:raise RuntimeError('Logical instruction cap before interpreted packet seam')
                        if ext is self.raw_transfer_extension and self.call_extension is not None:
                            self.call_extension.validate_transfer(plan)
                        self.trace.extend(plan['addresses']);ext.commit(self,plan);return
            if self.raw_transfer_extension is not None:self.raw_transfer_extension.guard_scalar(word)
            if self.accumulator_extension is not None:
                plan=self.accumulator_extension.plan(self,address,word,stops)
                if plan is not None:
                    if len(self.trace)+len(plan['addresses'])>count:raise RuntimeError('Logical instruction cap before interpreted accumulator operation')
                    self.trace.extend(plan['addresses']);self.accumulator_extension.commit(self,plan);return
                if len(self.trace)>=count:raise RuntimeError('Logical instruction cap in integer-square profile')
            if word>>26 in (0x1e,0x1f,0x12,0x1c):raise RuntimeError(f'Excluded R5900 instruction {address:08X}')
            if word>>26==0x11 and (word>>21)&31==16 and word&63==4:
                raise RuntimeError(f'Unreviewed R5900 SQRT.S operand/rounding semantics {address:08X};generic MIPS uses a different source register')
            if word>>26==0x11 and (word>>21)&31==16 and 0x18<=word&63<=0x1f:
                raise RuntimeError(f'Unreviewed R5900 COP1 accumulator instruction {address:08X}')
            division_qualified=self.division_extension.qualify(self,address,word) if self.division_extension is not None else False
            if self.stack_extension is not None and not division_qualified:self.stack_extension.guard_scalar(word,address)
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
        result=dict(entry=f'{entry:08X}',stop=f'{self.stop:08X}',instructions=len(self.trace),profile=self.profile,
            completion='original return' if self.stop==self.RETURN else 'declared original prefix boundary;guest discarded')
        if self.accumulator_extension is not None:
            result.update(instructionCountScope='Original instruction addresses;includes explicitly host-interpreted accumulator and B/slot pairs',
                accumulatorInterpretation=self.accumulator_extension.events,
                accumulatorScope='Only exact small-integer squares/nonnegative24-bit sums;not general EE FPU;FCR status read/write rejected;guest code unchanged')
        if self.stack_extension is not None:
            ext=self.stack_extension
            result.update(stackWindow=[f'{v:08X}' for v in ext.window],
                initialUpper64=[f'{v:016X}' for v in ext.initial_upper64],finalUpper64=[f'{v:016X}' for v in ext.upper64],
                stackSpillInterpretation=ext.events,
                instructionCountScope='Original instruction addresses,including all explicitly interpreted operations',
                stackStoreDelayEnabled=ext.store_delay,
                stackSpillScope='Aligned SP-based SQ/LQ within explicit mapped stack window;only opt-in BEQ-or-JAL/SQ paired slots;explicit upper64 GPR shadow;scalar ISA allowlist;not general EE CPU;guest code unchanged')
        if self.division_extension is not None:
            if self.division_extension.pending is not None:raise RuntimeError('Unverified DIVU/MFHI/MFLO effect at completion')
            result.update(unsignedDivisionObservations=self.division_extension.events,
                unsignedDivisionScope='Original native DIVU/MFHI/MFLO,nonzero divisor,quotient/remainder<=INT32_MAX,no delay slots;HI0/LO0 and GPR low64 validated. No host-written result,HI1/LO1 or general EE division claim.')
        if self.multiply_extension is not None:
            ext=self.multiply_extension
            result.update(multiplyInterpretation=ext.events,initialHi1Lo1=list(ext.initial_hi1_lo1),finalHi1Lo1=list(ext.hi1_lo1),
                multiplyScope='Explicit host interpretation of original MULT(rd)/MULT1 words;nonnegative low32 operands/product<=INT32_MAX,upper64 GPR preservation,explicitHI1/LO1;no delay slots or general MMI/pipeline claim.',
                instructionCountScope='Original instruction addresses,including explicitly host-interpreted operations')
        if self.raw_transfer_extension is not None:
            ext=self.raw_transfer_extension
            result.update(rawTransferInterpretation=ext.events,initialFprWords=list(ext.initial_fpr_words),finalFprWords=list(ext.fpr_words),
                transferWindows=list(ext.windows),beqStoreDelayEnabled=ext.beq_store_delay,
                rawTransferScope='Explicit host MOVZ and raw32-bit LWC1/SWC1 interpretation;only opt-in BEQ/SWC1 paired slots;no other delay slots,COP1 arithmetic or FCR/status semantics;permissions and aligned mapped windows required.',
                instructionCountScope='Original instruction addresses,including explicitly host-interpreted operations')
        if self.call_extension is not None:
            ext=self.call_extension
            result.update(externalPrimitiveObservations=ext.events,remainingHeapAllocations=ext.allocations,
                externalPrimitiveScope='Only explicitly typed terminal heap callbacks,memset,memcpy and ignored packet diagnostic;original JAL/RA/heap-wrapper traces validated;explicit bounded bump arena is research environment,not game allocator;external bodies not included in original instruction count.')
        if self.control_extension is not None:
            result.update(scalarControlInterpretation=self.control_extension.events,
                scalarControlScope='All encountered integer J/JAL/JR/JALR,BEQ/BNE/BEQL/BNEL,BLTZ/BGEZ interpreted explicitly with validated scalar/stack/raw transfer slots;unknown controls/slots refuse before backend execution. Fresh instance only;no native backend integer transfers,hiddenflagpatches,codepatches or interrupt forwarding. Not general EE CPU/timing/exception semantics.')
        return result

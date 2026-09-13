"""Opt-in validation of native DIVU/MFHI/MFLO in a narrow EE-compatible domain.

The guest executes original words on Unicorn. This checker neither patches
code nor supplies register results. Nonzero divisors and quotient/remainder
<=INT32_MAX avoid claiming unresolved high-bit extension behavior. HI1/LO1,
signed division, writes to HI/LO and delay-slot uses remain excluded.

Register scope: Sony EE Core User's Manual,3.1/3.1.1,
https://docs.alexrp.com/mips/ee.pdf . Independent instruction implementation:
https://github.com/PCSX2/pcsx2/blob/master/pcsx2/R5900OpcodeImpl.cpp .
"""
from ps2_integer_square_accumulator import transfer


class UnsignedDivision:
    def __init__(self):
        self.pending=None;self.known=None;self.events=[]

    @staticmethod
    def candidate(word):return word>>26==0 and word&63 in (0x1b,0x10,0x12)

    def verify_pending(self,p):
        if self.pending is None:return
        row=self.pending
        actual=(p.reg('HI'),p.reg('LO'))
        if actual!=tuple(row['expectedHiLo']):raise RuntimeError('Native DIVU/HI/LO effect differs from qualified positive31-bit domain')
        if row['operation']!='DIVU':
            rd=row['destination'];value=p.reg(str(rd))
            if value!=row['expectedRegister']:raise RuntimeError('Native MFHI/MFLO destination differs from qualified effect')
            if p.stack_extension.upper64[rd]!=row['upper64Before']:raise RuntimeError('MFHI/MFLO changed explicit upper64 GPR state')
            row.update(registerAfter=f'{value:016X}')
        else:self.known=actual
        row.update(verified=True,hiAfter=f'{actual[0]:016X}',loAfter=f'{actual[1]:016X}')
        self.events.append(row);self.pending=None

    def qualify(self,p,address,word):
        if transfer(word) and any(a<=address+4 and address+8<=a+n for a,n in p.ranges):
            if self.candidate(p.uint(address+4)):raise ValueError('DIVU/MFHI/MFLO delay slot not qualified;rejected before branch')
        if not self.candidate(word):return False
        if p.trace and p.trace[-1]+4==address and transfer(p.uint(p.trace[-1])):
            raise ValueError('DIVU/MFHI/MFLO reached in live branch delay slot')
        operation=word&63;row=dict(operationAddress=f'{address:08X}',word=f'{word:08X}',hiBefore=f'{p.reg("HI"):016X}',loBefore=f'{p.reg("LO"):016X}')
        if operation==0x1b:
            if word&0xffc0:raise ValueError('DIVU requires zero reserved rd/sa fields')
            rs,rt=(word>>21)&31,(word>>16)&31;dividend=p.reg(str(rs))&0xffffffff;divisor=p.reg(str(rt))&0xffffffff
            if divisor==0:raise ValueError('DIVU zero divisor outside qualified domain')
            quotient,remainder=divmod(dividend,divisor)
            if max(quotient,remainder)>0x7fffffff:raise ValueError('DIVU quotient/remainder exceed qualified positive31-bit domain')
            row.update(operation='DIVU',rs=rs,rt=rt,dividend=dividend,divisor=divisor,quotient=quotient,remainder=remainder,expectedHiLo=[remainder,quotient])
        else:
            if word&0x03ff07c0:raise ValueError('MFHI/MFLO reserved fields must be zero')
            if self.known is None:raise ValueError('MFHI/MFLO requires a verified earlier DIVU in this guest')
            if (p.reg('HI'),p.reg('LO'))!=self.known:raise ValueError('HI/LO changed outside the qualified DIVU sequence')
            rd=(word>>11)&31;value=self.known[0 if operation==0x10 else 1] if rd else 0
            row.update(operation='MFHI' if operation==0x10 else 'MFLO',destination=rd,expectedHiLo=list(self.known),registerBefore=f'{p.reg(str(rd)):016X}',upper64Before=p.stack_extension.upper64[rd],expectedRegister=value)
        self.pending=row
        return True

"""Opt-in EE MULT(rd)/MULT1 in the positive31 operand/product domain.

This is an explicit host interpretation, not a native Unicorn instruction.
Original words remain unchanged. Only low32 nonnegative operands and a product
<=INT32_MAX are admitted, so both sign-extended HI/LO words are unambiguous.
HI1/LO1 are explicit caller state, and upper64 GPR halves are preserved.
No timing, pipeline, overflow, other MMI or branch-delay multiply is modeled.

ISA provenance: Sony EE Core User's Manual 2.11.1 and3.1.2,
https://docs.alexrp.com/mips/ee.pdf . Independent implementation checked at
PCSX2 commit265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34:
pcsx2/R5900OpcodeImpl.cpp MULT; pcsx2/MMI.cpp MULT1.
No upstream implementation code is incorporated here.
"""
from ps2_integer_square_accumulator import transfer

MASK64 = (1 << 64) - 1


def reject_delay(p, address, word, candidate, label):
    if transfer(word) and any(a <= address + 4 and address + 8 <= a + n for a, n in p.ranges):
        if candidate(p.uint(address + 4)):
            raise ValueError(label + ' delay slot not qualified; rejected before branch')
    if p.trace and p.trace[-1] + 4 == address and transfer(p.uint(p.trace[-1])):
        raise ValueError(label + ' reached in live branch delay slot')


class Positive31Multiply:
    def __init__(self, hi1_lo1):
        if (not isinstance(hi1_lo1, (tuple, list)) or len(hi1_lo1) != 2
                or any(type(x) is not int or not 0 <= x <= MASK64 for x in hi1_lo1)):
            raise ValueError('Explicit unsigned64 HI1/LO1 initial state required')
        self.initial_hi1_lo1 = tuple(hi1_lo1)
        self.hi1_lo1 = list(hi1_lo1)
        self.events = []

    @staticmethod
    def candidate(word):
        return word >> 26 in (0, 0x1c) and word & 63 == 0x18

    def plan(self, p, address, word, stops):
        if not self.candidate(word):
            if transfer(word): reject_delay(p, address, word, self.candidate, 'MULT/MULT1')
            return None
        reject_delay(p, address, word, self.candidate, 'MULT/MULT1')
        if (word >> 6) & 31:
            raise ValueError('MULT/MULT1 reserved sa bits must be zero')
        rs, rt, rd = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
        left, right = p.reg(str(rs)) & 0xffffffff, p.reg(str(rt)) & 0xffffffff
        product = left * right
        if max(left, right, product) > 0x7fffffff:
            raise ValueError('MULT/MULT1 operands/product outside positive31 domain')
        return dict(addresses=[address], operationAddress=f'{address:08X}', word=f'{word:08X}',
            operation='MULT1' if word >> 26 == 0x1c else 'MULT', rs=rs, rt=rt, rd=rd,
            left=left, right=right, product=product, hiLoBefore=[p.reg('HI'), p.reg('LO')],
            hi1Lo1Before=list(self.hi1_lo1), registerBefore=p.reg(str(rd)),
            upper64Before=p.stack_extension.upper64[rd], nextPc=address + 4)

    def commit(self, p, plan):
        if plan['operation'] == 'MULT':
            p.reg('HI', 0); p.reg('LO', plan['product'])
        else:
            self.hi1_lo1[:] = [0, plan['product']]
        if plan['rd']:
            p.reg(str(plan['rd']), plan['product'])
        p.reg('PC', plan['nextPc'])
        plan.update(hiLoAfter=[p.reg('HI'), p.reg('LO')], hi1Lo1After=list(self.hi1_lo1),
            registerAfter=p.reg(str(plan['rd'])), upper64After=p.stack_extension.upper64[plan['rd']])
        self.events.append(plan)

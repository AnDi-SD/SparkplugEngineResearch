"""Explicit MOVZ and LWC1/SWC1 raw-word host interpretation for packet probes.

No floating arithmetic or generic FPU status is represented. All32 FPR words
and bounded readable/writable memory windows are explicit input; other COP1
instructions are rejected while this shadow state is active. MOVZ tests the
full low64 condition and preserves upper64 GPR state. Delay-slot operations
are rejected by default. Explicit beq_store_delay permits only integer BEQ
with one raw SWC1 slot, planned/validated atomically before any mutation.

ISA provenance: EE Core User's Manual2.8,2.10.1,3.1.1,8.2,
https://docs.alexrp.com/mips/ee.pdf ; independent MOVZ semantics checked at
PCSX2265d123dc33ee0ddf9c31e5bdf6c03f9d64e9d34 R5900OpcodeImpl.cpp:888.
This is original research-tool code, not copied upstream implementation.
"""
from ps2_integer_multiply import reject_delay
from ps2_integer_square_accumulator import transfer


class RawTransfers:
    def __init__(self, fpr_words, windows, *, beq_store_delay=False):
        if type(beq_store_delay) is not bool:
            raise ValueError('Explicit Boolean BEQ/SWC1 delay option required')
        self.beq_store_delay = beq_store_delay
        if (not isinstance(fpr_words, (tuple, list)) or len(fpr_words) != 32
                or any(type(x) is not int or not 0 <= x <= 0xffffffff for x in fpr_words)):
            raise ValueError('Explicit32 raw FPR words required')
        if not isinstance(windows, (tuple, list)) or not 1 <= len(windows) <= 16:
            raise ValueError('Explicit1..16 transfer memory windows required')
        normalized = []
        for row in windows:
            if not isinstance(row, (tuple, list)) or len(row) != 3:
                raise ValueError('Transfer window is address,size,r-or-rw')
            a, n, mode = row
            if (type(a) is not int or type(n) is not int or not 0 <= a < 0x100000000
                    or not 0 < n <= 0x10000 or a + n > 0x100000000 or mode not in ('r', 'rw')):
                raise ValueError('Bounded32-bit transfer memory window required')
            if any(a < b + m and b < a + n for b, m, _ in normalized):
                raise ValueError('Transfer windows must not overlap')
            normalized.append((a, n, mode))
        if sum(n for _, n, _ in normalized) > 0x20000:
            raise ValueError('Transfer windows exceed128KiB')
        self.windows = tuple(normalized)
        self.initial_fpr_words = tuple(fpr_words)
        self.fpr_words = list(fpr_words)
        self.events = []

    @staticmethod
    def candidate(word):
        return word >> 26 in (0x31, 0x39) or word >> 26 == 0 and word & 63 == 0x0a

    @staticmethod
    def guard_scalar(word):
        if word >> 26 == 0x11:
            raise ValueError('Other COP1 operations incompatible with raw FPR shadow')

    def plan(self, p, address, word, stops):
        if self.beq_store_delay and word >> 26 == 4 and any(a <= address + 4 and address + 8 <= a + n for a, n in p.ranges):
            delayed = p.uint(address + 4)
            if delayed >> 26 == 0x39:
                if address + 4 in stops:
                    raise ValueError('Stop inside interpreted BEQ/SWC1 slot; stop before branch')
                if p.trace and p.trace[-1] + 4 == address and transfer(p.uint(p.trace[-1])):
                    raise ValueError('BEQ/SWC1 pair reached in live branch delay slot')
                rs, rt = (word >> 21) & 31, (word >> 16) & 31
                taken = p.reg(str(rs)) == p.reg(str(rt))
                displacement = (word & 0xffff) - (0x10000 if word & 0x8000 else 0)
                next_pc = address + 4 + displacement * 4 if taken else address + 8
                if next_pc in (address, address + 4): raise ValueError('Self/slot BEQ target outside reviewed delay profile')
                if next_pc not in stops and not any(a <= next_pc and next_pc + 4 <= a + n for a, n in p.ranges):
                    raise ValueError('Interpreted BEQ target outside declared original ranges')
                # BEQ changes no GPR, so the slot reads the same captured
                # state. Slot permission/alignment/fault checks finish first.
                slot = self.plan(p, address + 4, delayed, stops)
                return dict(addresses=[address, address + 4], operationAddress=f'{address:08X}',
                    operation='BEQ/SWC1', word=f'{word:08X}', rs=rs, rt=rt,
                    left=p.reg(str(rs)), right=p.reg(str(rt)), taken=taken, slot=slot, nextPc=next_pc)
        if not self.candidate(word):
            if transfer(word): reject_delay(p, address, word, self.candidate, 'Raw transfer/MOVZ')
            return None
        reject_delay(p, address, word, self.candidate, 'Raw transfer/MOVZ')
        op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
        row = dict(addresses=[address], operationAddress=f'{address:08X}', word=f'{word:08X}', nextPc=address + 4)
        if op == 0:
            if (word >> 6) & 31: raise ValueError('MOVZ reserved sa bits must be zero')
            rd = (word >> 11) & 31
            row.update(operation='MOVZ', rs=rs, rt=rt, rd=rd, source=p.reg(str(rs)),
                condition=p.reg(str(rt)), registerBefore=p.reg(str(rd)), upper64Before=p.stack_extension.upper64[rd])
            return row
        offset = (word & 0xffff) - (0x10000 if word & 0x8000 else 0)
        effective = ((p.reg(str(rs)) & 0xffffffff) + offset) & 0xffffffff
        store = op == 0x39
        if effective % 4: raise ValueError('Raw FPR transfer must be word aligned')
        window = next((r for r in self.windows if r[0] <= effective and effective + 4 <= r[0] + r[1]), None)
        if window is None or store and window[2] != 'rw':
            raise ValueError('Raw FPR transfer outside declared memory permissions')
        if effective & ~4095 not in p.pages: raise ValueError('Transfer memory must already be mapped')
        before = p.read(effective, 4)
        row.update(operation='SWC1' if store else 'LWC1', base=rs, fpr=rt, offset=offset,
            effectiveAddress=f'{effective:08X}', memoryBefore=before.hex(), fprBefore=self.fpr_words[rt],
            transferWord=self.fpr_words[rt] if store else int.from_bytes(before, 'little'))
        return row

    def commit(self, p, plan):
        if plan['operation'] == 'BEQ/SWC1':
            slot = plan['slot']
            p.write(int(slot['effectiveAddress'], 16), slot['transferWord'].to_bytes(4, 'little'))
            slot.update(fprAfter=self.fpr_words[slot['fpr']],
                memoryAfter=p.read(int(slot['effectiveAddress'], 16), 4).hex())
        elif plan['operation'] == 'MOVZ':
            if plan['rd'] and plan['condition'] == 0: p.reg(str(plan['rd']), plan['source'])
            plan.update(registerAfter=p.reg(str(plan['rd'])), upper64After=p.stack_extension.upper64[plan['rd']])
        else:
            if plan['operation'] == 'LWC1': self.fpr_words[plan['fpr']] = plan['transferWord']
            else: p.write(int(plan['effectiveAddress'], 16), plan['transferWord'].to_bytes(4, 'little'))
            plan.update(fprAfter=self.fpr_words[plan['fpr']],
                memoryAfter=p.read(int(plan['effectiveAddress'], 16), 4).hex())
        p.reg('PC', plan['nextPc'])
        self.events.append(plan)

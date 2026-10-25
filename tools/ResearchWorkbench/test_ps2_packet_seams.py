"""Explicit interpreted EE seams; not a hardware or whole-class oracle.

Planning tests are pure host tests. OriginalWord tests map pristine ELF words
and use the bounded guest with recorded host interpretations, never patching
original instructions. Run guest tests only in the shared serial budget slot.
"""
import unittest
from types import SimpleNamespace
from ps2_integer_multiply import Positive31Multiply
from ps2_raw_transfers import RawTransfers
from ps2_scalar_prefix import Ps2ScalarPrefix
from test_ps2_stack_spills import PlanGuest
from test_support import UPPER


class PacketSeamPlanningTests(unittest.TestCase):
    def fixture(self, words=None):
        p = PlanGuest(words)
        p.stack_extension = SimpleNamespace(upper64=list(UPPER))
        p.reg('HI', 0x123456789abcdef0); p.reg('LO', 0xfedcba9876543210)
        return p

    def test_mult_and_mult1_select_only_own_pair_and_preserve_upper(self):
        for opcode in (0, 0x1c):
            p = self.fixture(); p.reg('2', 0xabcdef0000000007); p.reg('3', 13)
            ext = Positive31Multiply((0x123456789abcdef0, 0xfedcba9876543210))
            word = opcode << 26 | 2 << 21 | 3 << 16 | 4 << 11 | 0x18
            plan = ext.plan(p, 0x1000, word, set()); ext.commit(p, plan)
            self.assertEqual(p.reg('4'), 91); self.assertEqual(p.stack_extension.upper64, list(UPPER))
            self.assertEqual((p.reg('HI'), p.reg('LO')), (0, 91) if opcode == 0 else (0x123456789abcdef0, 0xfedcba9876543210))
            self.assertEqual(ext.hi1_lo1, [0, 91] if opcode else [0x123456789abcdef0, 0xfedcba9876543210])
            self.assertEqual(p.reg('PC'), 0x1004)

    def test_product_bound_reserved_and_delay_reject_without_mutation(self):
        for a, b in ((0x80000000, 0), (0, 0xffffffff), (65536, 65536)):
            p = self.fixture(); p.reg('2', a); p.reg('3', b); before = dict(p.regs)
            ext = Positive31Multiply((1, 2))
            with self.assertRaisesRegex(ValueError, 'positive31'): ext.plan(p, 0x1000, 0x00432018, set())
            self.assertEqual(p.regs, before); self.assertEqual(ext.hi1_lo1, [1, 2]); self.assertFalse(ext.events)
        p = self.fixture({0x1000: 0x10000001, 0x1004: 0x00432018}); ext = Positive31Multiply((1, 2))
        with self.assertRaisesRegex(ValueError, 'reserved'): ext.plan(p, 0x1000, 0x00432058, set())
        with self.assertRaisesRegex(ValueError, 'before branch'): ext.plan(p, 0x1000, 0x10000001, set())


        p.trace = [0x1000]
        with self.assertRaisesRegex(ValueError, 'live branch'): ext.plan(p, 0x1004, 0x00432018, set())
        self.assertFalse(ext.events)

    def test_zero_destination_and_operand_alias(self):
        p = self.fixture(); p.reg('2', 7); p.reg('3', 11)
        ext = Positive31Multiply((1, 2))
        ext.commit(p, ext.plan(p, 0x1000, 0x00430018, set()))
        self.assertNotIn('0', p.regs); self.assertEqual(p.reg('LO'), 77)
        ext.commit(p, ext.plan(p, 0x1004, 0x00431018, set()))
        self.assertEqual(p.reg('2'), 77); self.assertEqual(p.stack_extension.upper64, list(UPPER))

    def test_raw_words_permissions_and_movz_full64_condition(self):
        p = self.fixture(); p.reg('2', 0xffffffff22000000); p.reg('3', 0x22000004)
        p.write(0x22000000, bytes.fromhex('4523c17f'))
        ext = RawTransfers([0] * 32, [(0x22000000, 4, 'r'), (0x22000004, 4, 'rw')])
        ext.commit(p, ext.plan(p, 0x1000, 0xC4400000, set()))
        ext.commit(p, ext.plan(p, 0x1004, 0xE4600000, set()))
        self.assertEqual(p.read(0x22000004, 4), bytes.fromhex('4523c17f'))
        self.assertEqual(ext.fpr_words[0], 0x7fc12345)
        with self.assertRaisesRegex(ValueError, 'permissions'): ext.plan(p, 0x1008, 0xE4400000, set())
        p.reg('2', 7); p.reg('3', 1 << 32); p.reg('4', 99)
        ext.commit(p, ext.plan(p, 0x1008, 0x0043200a, set()))
        self.assertEqual(p.reg('4'), 99)
        p.reg('3', 0); ext.commit(p, ext.plan(p, 0x100c, 0x0043200a, set()))
        self.assertEqual(p.reg('4'), 7); self.assertEqual(p.stack_extension.upper64, list(UPPER))

    def test_transfer_configuration_alignment_mapping_and_cop1_guard(self):
        for words, windows in [([0] * 31, [(0, 4, 'r')]), ([0] * 32, []),
                ([0] * 32, [(0, 4, 'r'), (2, 4, 'rw')]), ([0] * 32, [(0, 4, 'w')])]:
            with self.assertRaises(ValueError): RawTransfers(words, windows)
        p = self.fixture(); ext = RawTransfers([0] * 32, [(0x22000000, 16, 'rw')])
        p.reg('2', 0x22000001)
        with self.assertRaisesRegex(ValueError, 'aligned'): ext.plan(p, 0x1000, 0xC4400000, set())
        p.reg('2', 0x22000000); p.pages.clear()
        with self.assertRaisesRegex(ValueError, 'mapped'): ext.plan(p, 0x1000, 0xC4400000, set())
        with self.assertRaisesRegex(ValueError, 'Other COP1'): ext.guard_scalar(0x46000000)
        p.words = {0x1004: 0xE4400000}
        with self.assertRaisesRegex(ValueError, 'before branch'): ext.plan(p, 0x1000, 0x10000001, set())




class NormalizationDelayPlanningTests(unittest.TestCase):
    def fixture(self, equal=True):
        p = PlanGuest({0x1004: 0xE4800000})
        p.stack_extension = SimpleNamespace(upper64=list(UPPER))
        p.reg('2', 7); p.reg('3', 7 if equal else 8); p.reg('4', 0x22000000)
        ext = RawTransfers([0x7FC12345] + [0] * 31, [(0x22000000, 4096, 'rw')], beq_store_delay=True)
        return p, ext

    def test_taken_and_not_taken_store_slot_once_with_two_trace_addresses(self):
        for equal in (False, True):
            p, ext = self.fixture(equal)
            plan = ext.plan(p, 0x1000, 0x10430002, set()); ext.commit(p, plan)
            self.assertEqual(plan['addresses'], [0x1000, 0x1004])
            self.assertEqual(p.read(0x22000000, 4), bytes.fromhex('4523c17f'))
            self.assertEqual(p.reg('PC'), 0x100c if equal else 0x1008)
            self.assertEqual(plan['taken'], equal); self.assertEqual(p.stack_extension.upper64, list(UPPER))
            self.assertEqual(len(ext.events), 1)

    def test_slot_stop_fault_permissions_and_target_reject_before_mutation(self):
        for mode in ('stop', 'alignment', 'permission', 'target'):
            p, ext = self.fixture(); before = bytes(p.memory); registers = dict(p.regs)
            if mode == 'alignment': p.reg('4', 0x22000001); registers = dict(p.regs)
            if mode == 'permission': ext.windows = ((0x22000000, 4096, 'r'),)
            word = 0x1043007f if mode == 'target' else 0x10430002
            with self.assertRaises(ValueError): ext.plan(p, 0x1000, word, {0x1004} if mode == 'stop' else set())
            self.assertEqual(bytes(p.memory), before); self.assertEqual(p.regs, registers); self.assertFalse(ext.events)

    def test_other_branch_slots_and_default_option_stay_rejected(self):
        for word in (0x14430002, 0x50430002):
            p, ext = self.fixture()
            with self.assertRaisesRegex(ValueError, 'before branch'): ext.plan(p, 0x1000, word, set())
        p, _ = self.fixture(); ext = RawTransfers([0] * 32, [(0x22000000, 4096, 'rw')])
        with self.assertRaisesRegex(ValueError, 'before branch'): ext.plan(p, 0x1000, 0x10430002, set())
        p, ext = self.fixture(); p.words[0x1004] = 0x0043200A
        with self.assertRaisesRegex(ValueError, 'before branch'): ext.plan(p, 0x1000, 0x10430002, set())


class OriginalWordInterpretationTests(unittest.TestCase):
    def prefix(self, ranges, transfers=False, beq_delay=False):
        p = Ps2ScalarPrefix(ranges, stack_window=(0x22000000, 4096), upper64=UPPER,
            integer_multiply=True, hi1_lo1=(0x123456789abcdef0, 0xfedcba9876543210),
            raw_transfers=transfers, fpr_words=[0xA5A5A5A5] * 32 if transfers else None,
            transfer_windows=[(0x21000000, 4096, 'r'), (0x21001000, 4096, 'rw')] if transfers else None,
            raw_swc1_beq_delay=beq_delay)
        p.map(0x22000000, 4096); p.reg('SP', 0x22000800)
        p.reg('HI', 77); p.reg('LO', 88)
        return p

    def test_original_mult_rd_and_mult1_words_recorded_code_unchanged(self):
        for address, opcode, rs, rt, rd in [(0x15deb4, 'MULT', 'V0', 'V1', 'T1'),
                (0x1609e4, 'MULT1', 'A2', 'V0', 'V0')]:
            for left, right in [(0, 17), (7, 13), (65535, 84), (0x7fffffff, 1)]:
                p = self.prefix([(address, 4)]); p.reg(rs, left); p.reg(rt, right)
                before = p.read(address, 4); result = p.run(address, [address + 4])
                self.assertEqual(p.reg(rd), left * right); self.assertEqual(p.read(address, 4), before)
                self.assertEqual(result['multiplyInterpretation'][0]['operation'], opcode)
                self.assertEqual(p.stack_extension.upper64, list(UPPER))

    def test_original_movz_zero_nonzero_high32_and_raw_fp_roundtrip(self):
        for condition in (0, 1, 1 << 32):
            p = self.prefix([(0x15eec4, 4)], True); p.reg('T7', condition); p.reg('T6', 0x8000)
            p.run(0x15eec4, [0x15eec8]); self.assertEqual(p.reg('T6'), 0 if condition == 0 else 0x8000)
            self.assertEqual(p.stack_extension.upper64, list(UPPER))
        for word in (0, 0x80000000, 0x7FC12345, 0x7f800000, 0x00000001):
            p = self.prefix([(0x15ef3c, 8)], True)
            p.map(0x21000000, 8192); p.reg('T6', 0x21000000); p.reg('S0', 0x21001000)
            p.write(0x21000000, word.to_bytes(4, 'little')); p.write(0x21001000, b'\xCD' * 4)
            result = p.run(0x15ef3c, [0x15ef44])
            self.assertEqual(p.read(0x21001000, 4), word.to_bytes(4, 'little'))
            self.assertEqual([x['operation'] for x in result['rawTransferInterpretation']], ['LWC1', 'SWC1'])

    def test_opt_in_default_rejection_and_finite_limits(self):
        p = Ps2ScalarPrefix([(0x1609e4, 4)], stack_window=(0x22000000, 4096), upper64=UPPER)
        with self.assertRaisesRegex(RuntimeError, 'Excluded R5900'): p.run(0x1609e4, [0x1609e8])
        p = self.prefix([(0x15deb4, 4)]); p.reg('V0', 65536); p.reg('V1', 65536)
        with self.assertRaisesRegex(ValueError, 'positive31'): p.run(0x15deb4, [0x15deb8])
        self.assertFalse(p.trace); self.assertFalse(p.multiply_extension.events)
        p = self.prefix([(0x15deb4, 8)]); p.reg('V0', 7); p.reg('V1', 13)
        with self.assertRaises(RuntimeError): p.run(0x15deb4, [0x15debc], count=1)
        self.assertEqual(len(p.trace), 1)
        with self.assertRaisesRegex(ValueError, 'Discard'): p.run(0x15deb4, [0x15deb8])

    def test_original_normalization_beq_swc1_pair_and_cap(self):
        for color in (0, 1):
            p = self.prefix([(0x160a14, 8)], True, True)
            p.map(0x21001000, 4096); p.reg('S0', color); p.reg('V0', 0x21001000)
            p.raw_transfer_extension.fpr_words[0] = 0x7FC12345
            p.write(0x21001000, b'\xCD' * 16)
            result = p.run(0x160a14, [0x160a68, 0x160a1c])
            self.assertEqual(p.read(0x21001008, 4), bytes.fromhex('4523c17f'))
            self.assertEqual(p.reg('PC'), 0x160a1c if color else 0x160a68)
            self.assertEqual(result['rawTransferInterpretation'][0]['operation'], 'BEQ/SWC1')
            self.assertEqual(p.trace, [0x160a14, 0x160a18])
        p = self.prefix([(0x160a14, 8)], True, True)
        p.map(0x21001000, 4096); p.reg('V0', 0x21001000); before = p.read(0x21001000, 16)
        with self.assertRaisesRegex(RuntimeError, 'instruction cap'):
            p.run(0x160a14, [0x160a68, 0x160a1c], count=1)
        self.assertEqual(p.read(0x21001000, 16), before); self.assertFalse(p.trace)


if __name__ == '__main__': unittest.main()

"""Meaningful domain/ISA/cap checks for the explicit analysis CPU profile."""
import struct,unittest
from ps2_scalar_prefix import Ps2ScalarPrefix
from ps2_integer_square_accumulator import IntegerSquareAccumulator


def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]


class PlanRecord:
    """Synthetic planning-only record;not executable game evidence."""
    def __init__(self,words=None):
        self.words=words or {};self.ranges=[(0x1000,0x100)];self.trace=[];self.registers={}
    def uint(self,a):return self.words[a]
    def reg(self,k,value=None):
        if value is not None:self.registers[k]=value
        return self.registers.get(k,0)


class IntegerSquareGuards(unittest.TestCase):
    def test_small_signed_integers_and_positive_zero(self):
        for value in (-2048,-1000,0,1,2048):
            p=PlanRecord();p.reg('F3',bits(value));a=IntegerSquareAccumulator()
            plan=a.plan(p,0x1000,0x4603181a,set());self.assertIsNone(a.accumulator)
            a.commit(p,plan);self.assertEqual(a.accumulator,value*value)

    def test_unsupported_numeric_inputs_leave_state_unchanged(self):
        for value in (bits(.5),bits(2049),bits(-2049),0x80000000,1,0x7f800000,0x7fc00000):
            p=PlanRecord();p.reg('F3',value);a=IntegerSquareAccumulator();before=dict(p.registers)
            with self.assertRaises(ValueError):a.plan(p,0x1000,0x4603181a,set())
            self.assertEqual(p.registers,before);self.assertIsNone(a.accumulator);self.assertFalse(a.events)

    def test_encoding_and_uninitialized_accumulator(self):
        for word in (0x4602181a,0x4603185a,0x46031819,0x4601081e,0x4605285c):
            p=PlanRecord();a=IntegerSquareAccumulator()
            with self.assertRaises(ValueError):a.plan(p,0x1000,word,set())
            self.assertIsNone(a.accumulator);self.assertFalse(p.registers)

    def test_exact_sum_bound_and_madd_preserves_accumulator(self):
        p=PlanRecord();p.reg('F5',bits(1));a=IntegerSquareAccumulator();a.accumulator=0xffffff
        with self.assertRaisesRegex(ValueError,'24-bit'):a.plan(p,0x1000,0x4605285c,set())
        self.assertEqual(a.accumulator,0xffffff);self.assertEqual(p.reg('F1'),0)
        a.accumulator=25;plan=a.plan(p,0x1000,0x4605285c,set());a.commit(p,plan)
        self.assertEqual(p.reg('F1'),bits(26));self.assertEqual(a.accumulator,25)

    def test_fcr_read_and_write_are_explicitly_rejected(self):
        for word in (0x4442f800,0x44c2f800):
            with self.assertRaisesRegex(ValueError,'FCR status'):IntegerSquareAccumulator().plan(PlanRecord(),0x1000,word,set())

    def test_conditional_and_self_target_branches_rejected_before_mutation(self):
        for word in (0x11080002,0x1000ffff,0x10000000,0x10000100):
            p=PlanRecord({0x1004:0x4601081c});a=IntegerSquareAccumulator();a.accumulator=25
            with self.assertRaises(ValueError):a.plan(p,0x1000,word,set())
            self.assertEqual(a.accumulator,25);self.assertFalse(p.registers)

    def test_original_square_sequence_keeps_code_bytes(self):
        p=Ps2ScalarPrefix([(0x3828b0,0x10)],profile='integer-squares');p.map(0x22000000,4096);p.reg('SP',0x22000800)
        p.reg('F3',bits(1000));p.reg('F1',bits(1000));p.reg('F5',bits(0));before=p.read(0x3828b0,0x10)
        r=p.run(0x3828b0,[0x3828c0]);self.assertEqual(p.reg('F1'),bits(2000000));self.assertEqual(p.read(0x3828b0,0x10),before)
        self.assertEqual(r['instructions'],4);self.assertEqual(len(r['accumulatorInterpretation']),3)
        with self.assertRaisesRegex(ValueError,'Discard'):p.run(0x3828b0,[0x3828c0])

    def original_branch_pair(self):
        p=Ps2ScalarPrefix([(0x293288,8),(0x2932b0,4)],profile='integer-squares')
        p.accumulator_extension.accumulator=22509;p.reg('F1',bits(4));p.reg('F0',0xa5a5a5a5)
        return p

    def test_original_unconditional_branch_and_delay_pair(self):
        p=self.original_branch_pair();before=p.read(0x293288,8);r=p.run(0x293288,[0x2932b0])
        self.assertEqual(p.reg('F0'),bits(22525));self.assertEqual(p.accumulator_extension.accumulator,22509)
        self.assertEqual(p.read(0x293288,8),before);self.assertEqual(p.trace,[0x293288,0x29328c]);self.assertEqual(r['instructions'],2)

    def test_pair_obeys_logical_cap_and_delay_boundary(self):
        p=self.original_branch_pair()
        with self.assertRaisesRegex(RuntimeError,'Logical instruction cap'):p.run(0x293288,[0x2932b0],count=1)
        self.assertEqual(p.reg('F0'),0xa5a5a5a5);self.assertEqual(p.accumulator_extension.accumulator,22509);self.assertEqual(p.trace,[])
        p=self.original_branch_pair()
        with self.assertRaisesRegex(ValueError,'delay slot'):p.run(0x293288,[0x29328c])
        self.assertEqual(p.reg('F0'),0xa5a5a5a5);self.assertFalse(p.accumulator_extension.events)

    def test_default_and_other_isa_exclusions_remain(self):
        p=Ps2ScalarPrefix([(0x3828b0,4)])
        with self.assertRaisesRegex(RuntimeError,'Unreviewed R5900 COP1'):p.run(0x3828b0,[p.RETURN])
        for address in (0x14e158,0x40aa40):
            p=Ps2ScalarPrefix([(address,4)],profile='integer-squares')
            with self.assertRaisesRegex(RuntimeError,'Excluded R5900'):p.run(address,[p.RETURN])


if __name__=='__main__':unittest.main()

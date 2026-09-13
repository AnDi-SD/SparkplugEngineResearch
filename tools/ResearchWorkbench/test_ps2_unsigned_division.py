"""Native instruction effects and rejection boundaries for the DIVU opt-in."""
import unittest
from types import SimpleNamespace
from ps2_scalar_prefix import Ps2ScalarPrefix
from ps2_unsigned_division import UnsignedDivision
from test_support import UPPER
from test_ps2_stack_spills import PlanGuest


class UnsignedDivisionGuards(unittest.TestCase):
    def prefix(self,ranges,**kwargs):
        p=Ps2ScalarPrefix(ranges,stack_window=(0x22000000,4096),upper64=UPPER,unsigned_division=True,**kwargs)
        p.map(0x22000000,4096);p.reg('SP',0x22000800);p.reg('HI',0x123456789abcdef0);p.reg('LO',0xfedcba9876543210)
        return p

    def test_original_divu_mfhi_positive_domain(self):
        for a,b in [(0,1501),(1,1501),(1500,1501),(1501,1501),(581869302,1501),(3890346734,501),(0xffffffff,1501),(0xffffffff,0xfffffffe)]:
            p=self.prefix([(0x25fd28,8)]);p.reg('V0',a if a<0x80000000 else a|0xffffffff00000000);p.reg('V1',b if b<0x80000000 else b|0xffffffff00000000);code=p.read(0x25fd28,8)
            r=p.run(0x25fd28,[0x25fd30],timeout_us=500000);q,rem=divmod(a,b)
            self.assertEqual((p.reg('HI'),p.reg('LO'),p.reg('V1')),(rem,q,rem));self.assertEqual(p.read(0x25fd28,8),code)
            self.assertEqual([v['operation'] for v in r['unsignedDivisionObservations']],['DIVU','MFHI']);self.assertTrue(all(v['verified'] for v in r['unsignedDivisionObservations']))
            self.assertEqual(p.stack_extension.upper64,list(UPPER));self.assertEqual(p.trace,[0x25fd28,0x25fd2c])

    def test_original_divu_mflo_and_ignored_input_high32(self):
        for a,b in [(0,501),(0x12345678,501),(0xffffffff,1501),(0x7fffffff,1)]:
            p=self.prefix([(0x11529c,8)]);p.reg('V1',0x1234567800000000|a);p.reg('S1',0xfedcba9800000000|b);p.reg('A0',0xa5a5a5a5a5a5a5a5)
            r=p.run(0x11529c,[0x1152a4],timeout_us=500000);q,rem=divmod(a,b)
            self.assertEqual((p.reg('HI'),p.reg('LO'),p.reg('A0')),(rem,q,q));self.assertEqual(r['unsignedDivisionObservations'][-1]['operation'],'MFLO')
            self.assertEqual(p.stack_extension.upper64,list(UPPER))

    def test_zero_divisor_and_high_bit_results_reject_before_execution(self):
        for a,b in [(123,0),(0x80000000,1),(0xffffffff,1),(0xf0000000,0xffffffff)]:
            p=self.prefix([(0x25fd28,8)]);p.reg('V0',a);p.reg('V1',b)
            with self.assertRaises(ValueError):p.run(0x25fd28,[0x25fd30])
            self.assertEqual((p.reg('HI'),p.reg('LO')),(0x123456789abcdef0,0xfedcba9876543210));self.assertFalse(p.trace);self.assertFalse(p.division_extension.events)

    def test_mfhi_requires_original_divu_in_same_guest(self):
        p=self.prefix([(0x25fd2c,4)]);p.reg('V1',123)
        with self.assertRaisesRegex(ValueError,'earlier DIVU'):p.run(0x25fd2c,[0x25fd30])
        self.assertEqual(p.reg('V1'),123);self.assertFalse(p.trace)

    def test_default_stack_profile_still_rejects_divu(self):
        p=Ps2ScalarPrefix([(0x25fd28,8)],stack_window=(0x22000000,4096),upper64=UPPER);p.reg('V0',1);p.reg('V1',1501)
        with self.assertRaisesRegex(ValueError,'stack-spill scalar ISA'):p.run(0x25fd28,[0x25fd30])
        self.assertFalse(p.trace);self.assertIsNone(p.division_extension)

    def test_option_requires_explicit_stack_and_r4000(self):
        for kwargs in [dict(unsigned_division=True),dict(unsigned_division=1),dict(unsigned_division=True,stack_window=(0x22000000,4096),upper64=UPPER,profile='integer-movz')]:
            with self.assertRaises(ValueError):Ps2ScalarPrefix([(0x25fd28,8)],**kwargs)

    def test_exact_preinstruction_stop_and_cap_remain(self):
        p=self.prefix([(0x25fd28,8)]);p.reg('V0',1);p.reg('V1',1501)
        r=p.run(0x25fd28,[0x25fd28]);self.assertFalse(r['unsignedDivisionObservations']);self.assertFalse(p.trace)
        p=self.prefix([(0x25fd28,8)]);p.reg('V0',1);p.reg('V1',1501)
        with self.assertRaises(RuntimeError):p.run(0x25fd28,[0x25fd30],count=1)
        self.assertEqual(p.trace,[0x25fd28]);self.assertFalse(p.division_extension.events)
        with self.assertRaisesRegex(ValueError,'Discard'):p.run(0x25fd28,[0x25fd30])

    def synthetic(self,words=None):
        p=PlanGuest(words);p.stack_extension=SimpleNamespace(upper64=list(UPPER));p.reg('2',100);p.reg('3',7)
        return p

    def test_delay_and_reserved_encodings_reject_without_mutation(self):
        p=self.synthetic({0x1000:0x10000001,0x1004:0x0043001b});d=UnsignedDivision();before=dict(p.regs)
        with self.assertRaisesRegex(ValueError,'before branch'):d.qualify(p,0x1000,0x10000001)
        p.trace=[0x1000]
        with self.assertRaisesRegex(ValueError,'live branch'):d.qualify(p,0x1004,0x0043001b)
        p.trace=[]
        for word in (0x0043081b,0x0043005b,0x00201810,0x00001852):
            with self.assertRaisesRegex(ValueError,'reserved'):d.qualify(p,0x1000,word)
        self.assertEqual(p.regs,before);self.assertFalse(d.events);self.assertIsNone(d.pending)

    def test_mismatch_never_becomes_verified_evidence(self):
        p=self.synthetic();d=UnsignedDivision();self.assertTrue(d.qualify(p,0x1000,0x0043001b))
        with self.assertRaisesRegex(RuntimeError,'differs'):d.verify_pending(p)
        self.assertFalse(d.events);self.assertIsNone(d.known)

    def test_zero_destination_planning(self):
        p=self.synthetic();d=UnsignedDivision();d.qualify(p,0x1000,0x0043001b);p.reg('HI',2);p.reg('LO',14);d.verify_pending(p)
        self.assertTrue(d.qualify(p,0x1004,0x00000010));d.verify_pending(p)
        self.assertEqual(p.reg('0'),0);self.assertEqual(d.events[-1]['expectedRegister'],0);self.assertEqual(p.stack_extension.upper64,list(UPPER))

    def test_integer_square_profile_can_opt_in_without_relaxing_acc_guard(self):
        p=self.prefix([(0x25fd28,8)],profile='integer-squares');p.reg('V0',100);p.reg('V1',7);r=p.run(0x25fd28,[0x25fd30])
        self.assertEqual(p.reg('V1'),2);self.assertFalse(r['accumulatorInterpretation'])
        p=self.prefix([(0x25f358,4)],profile='integer-squares')
        with self.assertRaisesRegex(ValueError,'only legal'):p.run(0x25f358,[p.RETURN])


if __name__=='__main__':unittest.main()

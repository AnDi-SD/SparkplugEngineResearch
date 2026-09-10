"""Original leaves and a real excluded stack instruction guard the wrapper."""
import unittest
from ps2_scalar_prefix import Ps2ScalarPrefix


class PrefixGuards(unittest.TestCase):
    def test_original_true_and_false(self):
        for entry,value in ((0x286450,1),(0x288c60,0)):
            p=Ps2ScalarPrefix([(entry,8)]);p.run(entry,[p.RETURN])
            self.assertEqual(p.reg('V0'),value)
            with self.assertRaisesRegex(ValueError,'Discard'):p.run(entry,[p.RETURN])

    def test_real_sq_rejected_before_memory_write(self):
        p=Ps2ScalarPrefix([(0x14e158,4)]);p.map(0x22000000,4096);p.reg('SP',0x22000000)
        p.write(0x22000000,b'!'*64);before=p.read(0x22000000,64)
        with self.assertRaisesRegex(RuntimeError,'Excluded R5900'):p.run(0x14e158,[p.RETURN])
        self.assertEqual(p.read(0x22000000,64),before)

    def test_exact_boundary_precedes_excluded_instruction(self):
        p=Ps2ScalarPrefix([(0x14e158,4)]);r=p.run(0x14e158,[0x14e158])
        self.assertEqual(r['instructions'],0)

    def test_uncaptured_delay_slot_rejected(self):
        p=Ps2ScalarPrefix([(0x286450,4)])
        with self.assertRaisesRegex(RuntimeError,'Outside declared'):p.run(0x286450,[p.RETURN])

    def test_accumulator_encoding_is_not_generic_mips(self):
        p=Ps2ScalarPrefix([(0x286640,4)])
        with self.assertRaisesRegex(RuntimeError,'Unreviewed R5900 COP1'):p.run(0x286640,[p.RETURN])

    def test_movz_is_not_available_on_default_r4000(self):
        p=Ps2ScalarPrefix([(0x16bc20,12)])
        with self.assertRaisesRegex(RuntimeError,'interrupt forwarding 20; last original 0016BC28'):
            p.run(0x16bc20,[0x16bc30])

    def test_original_movz_in_delay_slot_uses_low64_condition(self):
        for condition in (0,1,0x80000000,0x100000000,0xffffffffffffffff):
            p=Ps2ScalarPrefix([(0x16bc20,12)],profile='integer-movz');p.reg('A0',condition)
            r=p.run(0x16bc20,[0x16bc30])
            self.assertEqual(p.reg('V1'),2 if condition else 0)
            self.assertEqual(p.trace,[0x16bc20,0x16bc24,0x16bc28])
            self.assertEqual(r['profile'],'integer-movz')

    def test_original_lw_sign_extends_in_movz_profile(self):
        for value in (0,0x7fffffff,0x80000000,0xffffffff):
            p=Ps2ScalarPrefix([(0x16bc34,4)],profile='integer-movz');p.map(0x21000000,4096)
            p.reg('S0',0x21000000);p.put_uint(0x2100004c,value);p.run(0x16bc34,[0x16bc38])
            self.assertEqual(p.reg('V1'),value|(0xffffffff00000000 if value&0x80000000 else 0))

    def test_movz_profile_keeps_excluded_words_and_bounds(self):
        for entry,error in ((0x14e158,'Excluded R5900'),(0x286640,'Unreviewed R5900 COP1')):
            p=Ps2ScalarPrefix([(entry,4)],profile='integer-movz')
            with self.assertRaisesRegex(RuntimeError,error):p.run(entry,[p.RETURN])
        p=Ps2ScalarPrefix([(0x286450,4)],profile='integer-movz')
        with self.assertRaisesRegex(RuntimeError,'Outside declared'):p.run(0x286450,[p.RETURN])
        p=Ps2ScalarPrefix([(0x16bc20,12)],profile='integer-movz')
        p.run(0x16bc20,[0x16bc20]);self.assertEqual(p.trace,[])
        with self.assertRaisesRegex(ValueError,'Discard'):p.run(0x16bc20,[0x16bc20])


if __name__=='__main__':unittest.main()

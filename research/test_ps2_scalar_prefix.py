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


if __name__=='__main__':unittest.main()

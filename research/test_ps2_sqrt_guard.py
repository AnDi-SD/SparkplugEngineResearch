"""Prevent a demonstrated EE/MIPS operand mismatch from becoming evidence."""
import unittest
from ps2_scalar_prefix import Ps2ScalarPrefix


class SqrtGuard(unittest.TestCase):
    def test_original_sqrt_rejected_without_destination_change(self):
        for profile in ('r4000','integer-squares','integer-movz'):
            p=Ps2ScalarPrefix([(0x2921e4,4)],profile=profile)
            self.assertEqual(p.uint(0x2921e4),0x46010044)
            p.reg('F0',0x41100000);p.reg('F1',0x41c80000)
            with self.assertRaisesRegex(RuntimeError,'Unreviewed R5900 SQRT.S'):
                p.run(0x2921e4,[0x2921e8])
            self.assertEqual(p.reg('F0'),0x41100000);self.assertEqual(p.reg('F1'),0x41c80000)
            self.assertEqual(p.trace,[])

    def test_explicit_boundary_before_sqrt_remains_valid(self):
        p=Ps2ScalarPrefix([(0x2921e4,4)]);p.reg('F1',0x41c80000)
        result=p.run(0x2921e4,[0x2921e4]);self.assertEqual(result['instructions'],0)
        self.assertEqual(p.reg('F1'),0x41c80000)


if __name__=='__main__':unittest.main()

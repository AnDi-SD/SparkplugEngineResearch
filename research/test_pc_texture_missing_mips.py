"""External CRT boundary guards, independent of the proprietary executable."""
import unittest
from types import SimpleNamespace
from probe_pc_texture_missing_mips import MissingMipFixture
from pc_compact_texture_device import install_texture_device


class Registers:
    def __init__(self,mantissa,exponent):
        self.values={'FPSW':3<<11,'FPTAG':0,'EDX':0}
        self.xr=SimpleNamespace(UC_X86_REG_FP3=3)
        self.mu=SimpleNamespace(reg_read=lambda index:(mantissa,exponent))
    def reg(self,name):return self.values[name]
    def set_reg(self,name,value):self.values[name]=value
    def fixture_return(self,*,eax):self.values['EAX']=eax


class MissingMipBoundaries(unittest.TestCase):
    def test_ftol_preserves_fraction_below_integer(self):
        # Rounding this extended value to binary64 first incorrectly gives1.
        for sign in (0,0x8000):
            p=Registers((1<<64)-1,16382|sign)
            fixture=SimpleNamespace(ftol_calls=0)
            MissingMipFixture.ftol(fixture,p)
            self.assertEqual((p.values['EDX'],p.values['EAX']),(0,0))
            self.assertEqual((p.values['FPSW']>>11)&7,4)
            self.assertEqual((p.values['FPTAG']>>6)&3,3)

    def test_ftol_signed_64_result(self):
        for exponent,expected in ((16383+40,(256,0)),((16383+40)|0x8000,(0xffffff00,0))):
            p=Registers(1<<63,exponent);MissingMipFixture.ftol(SimpleNamespace(ftol_calls=0),p)
            self.assertEqual((p.values['EDX'],p.values['EAX']),expected)

    def test_dimensions_rejected_before_guest(self):
        for dimensions in ((17,16),(16,0),(3,2),(16,16,16)):
            with self.assertRaises(AssertionError):MissingMipFixture(b'',dimensions)

    def test_compact_profile_rejected_before_memory_access(self):
        for value in (object,42,None):
            with self.assertRaisesRegex(ValueError,'texture COM fixture class'):
                install_texture_device(None,64,fixture_class=value)


if __name__=='__main__':unittest.main()

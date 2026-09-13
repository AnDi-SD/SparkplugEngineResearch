"""CPU semantics/bounds checks, distinct from game-class coverage evidence."""
import unittest
from ps2_scalar_prefix import Ps2ScalarPrefix
from ps2_stack_spills import StackSpills
from test_support import UPPER


class PlanGuest:
    """Synthetic read-only planning fixture; never executed as game code."""
    def __init__(self,words=None):
        self.ranges=((0x1000,0x100),);self.trace=[];self.pages={0x22000000}
        self.words=words or {};self.regs={'SP':0x22000800};self.memory=bytearray(b'\xa5'*4096)
    def uint(self,a):return self.words[a]
    def reg(self,n,v=None):
        if v is not None:self.regs[n]=v
        return self.regs.get(n,0)
    def read(self,a,n):return bytes(self.memory[a-0x22000000:a-0x22000000+n])
    def write(self,a,b):self.memory[a-0x22000000:a-0x22000000+len(b)]=b


class StackSpillGuards(unittest.TestCase):
    def prefix(self,ranges):
        p=Ps2ScalarPrefix(ranges,stack_window=(0x22000000,4096),upper64=UPPER)
        p.map(0x22000000,4096);p.reg('SP',0x22000800);p.write(0x22000000,b'\xa5'*4096)
        return p

    def test_original_sq_writes_exactly_sixteen_bytes(self):
        p=self.prefix([(0x37e7a0,4)]);p.reg('S0',0x123456789abcdef0);before=p.read(0x22000000,4096)
        code=p.read(0x37e7a0,4);r=p.run(0x37e7a0,[0x37e7a4]);expected=bytearray(before)
        expected[0x800:0x810]=(0x123456789abcdef0).to_bytes(8,'little')+UPPER[16].to_bytes(8,'little')
        self.assertEqual(p.read(0x22000000,4096),expected);self.assertEqual(p.read(0x37e7a0,4),code)
        self.assertEqual(r['instructions'],1);self.assertEqual(len(r['stackSpillInterpretation']),1)

    def test_original_lq_loads_both_halves_without_memory_write(self):
        p=self.prefix([(0x37e920,4)]);low=0xfedcba9876543210;high=0x8877665544332211
        p.write(0x22000800,low.to_bytes(8,'little')+high.to_bytes(8,'little'));before=p.read(0x22000000,4096)
        p.run(0x37e920,[0x37e924]);self.assertEqual(p.reg('S0'),low);self.assertEqual(p.stack_extension.upper64[16],high)
        self.assertEqual(p.read(0x22000000,4096),before)

    def test_original_scalar_move_preserves_upper_half(self):
        p=self.prefix([(0x37e7a4,4)]);p.reg('A0',0x0123456789abcdef)
        p.run(0x37e7a4,[0x37e7a8]);self.assertEqual(p.reg('S1'),0x0123456789abcdef)
        self.assertEqual(p.stack_extension.upper64,list(UPPER))

    def test_zero_register_store_and_load_planning(self):
        # Raw synthetic SQ/LQ r0,0(SP), with nonzero source memory.
        for word,expected in ((0x7fa00000,b'\0'*16),(0x7ba00000,b'\xa5'*16)):
            p=PlanGuest();ext=StackSpills((0x22000000,4096),UPPER);plan=ext.plan(p,0x1000,word,set());ext.commit(p,plan)
            self.assertEqual(p.read(0x22000800,16),expected);self.assertEqual(ext.upper64[0],0);self.assertNotIn('0',p.regs)
            self.assertEqual(plan['registerAfter'],'0'*32)

    def test_alignment_window_and_mapping_reject_before_mutation(self):
        for sp in (0x22000801,0x21fffff0,0x22001000):
            p=self.prefix([(0x37e7a0,4)]);p.reg('SP',sp);before=p.read(0x22000000,4096)
            with self.assertRaises(ValueError):p.run(0x37e7a0,[0x37e7a4])
            self.assertEqual(p.read(0x22000000,4096),before);self.assertFalse(p.stack_extension.events);self.assertFalse(p.trace)
        p=PlanGuest();p.pages=set();ext=StackSpills((0x22000000,4096),UPPER)
        with self.assertRaisesRegex(ValueError,'mapped'):ext.plan(p,0x1000,0x7fb00000,set())

    def test_signed_offset_and_low32_base(self):
        p=PlanGuest();p.reg('SP',0xffffffff22000810);ext=StackSpills((0x22000000,4096),UPPER)
        plan=ext.plan(p,0x1000,0x7fb0fff0,set());self.assertEqual(plan['effectiveAddress'],'22000800');self.assertEqual(plan['offset'],-16)

    def test_nonstack_and_delay_slots_rejected_read_only(self):
        p=PlanGuest({0x1004:0x7fb00000});ext=StackSpills((0x22000000,4096),UPPER);before=bytes(p.memory)
        with self.assertRaisesRegex(ValueError,'SP-based'):ext.plan(p,0x1000,0x7c900000,set())
        with self.assertRaisesRegex(ValueError,'before branch'):ext.plan(p,0x1000,0x0c040000,set())
        p.words[0x1000]=0x0c040000;p.trace=[0x1000]
        with self.assertRaisesRegex(ValueError,'live branch'):ext.plan(p,0x1004,0x7fb00000,set())
        self.assertEqual(bytes(p.memory),before);self.assertEqual(p.regs,{'SP':0x22000800});self.assertFalse(ext.events)

    def test_limit_and_preinstruction_stop(self):
        p=self.prefix([(0x37e79c,12)]);before=p.read(0x22000000,4096)
        r=p.run(0x37e79c,[0x37e79c]);self.assertEqual(r['instructions'],0);self.assertEqual(p.read(0x22000000,4096),before)
        p=self.prefix([(0x37e79c,12)])
        with self.assertRaises(RuntimeError):p.run(0x37e79c,[0x37e7a4],count=1)
        self.assertEqual(p.trace,[0x37e79c]);self.assertEqual(len(p.stack_extension.events),1)
        self.assertEqual(p.read(0x22000800,16),b'\xa5'*16)
        with self.assertRaisesRegex(ValueError,'Discard'):p.run(0x37e79c,[0x37e7a4])

    def test_explicit_state_and_code_separation(self):
        for kwargs in ({'stack_window':(0x22000000,4096)}, {'upper64':UPPER}, {'stack_window':(0x22000000,4096),'upper64':[0]*31}, {'stack_window':(0x22000000,4096),'upper64':[1]*32}, {'stack_window':(0x37e790,4096),'upper64':UPPER}):
            with self.assertRaises(ValueError):Ps2ScalarPrefix([(0x37e790,4)],**kwargs)
        for window in ((0x22000001,4096),(0x22000000,15),(0x22000000,0x10010)):
            with self.assertRaises(ValueError):StackSpills(window,UPPER)

    def test_unqualified_upper_half_users_and_sqrt_still_block(self):
        for address,error in ((0x40aa40,'Excluded R5900'),(0x2921e4,'SQRT.S')):
            p=self.prefix([(address,4)])
            with self.assertRaisesRegex(RuntimeError,error):p.run(address,[p.RETURN])
            self.assertEqual(p.stack_extension.upper64,list(UPPER));self.assertFalse(p.stack_extension.events)
        with self.assertRaises(ValueError):StackSpills.guard_scalar(0x48000000,0x1000)


if __name__=='__main__':unittest.main()

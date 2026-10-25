"""Pure planning tests only; original-word execution is separate evidence."""
import unittest
from ps2_stack_spills import StackSpills
from test_support import UPPER


class PlanGuest:
    def __init__(self,word=0x10800003,slot=0x7fb00000):
        self.ranges=((0x1000,0x100),);self.trace=[];self.pages={0x22000000}
        self.words={0x1000:word,0x1004:slot};self.regs={'SP':0x22000800,'4':0,'16':0x123456789abcdef0,'31':0x9999}
        self.memory=bytearray(b'\xa5'*4096)
    def uint(self,address):return self.words[address]
    def reg(self,name,value=None):
        if value is not None:self.regs[name]=value
        return self.regs.get(name,0)
    def read(self,address,size):return bytes(self.memory[address-0x22000000:address-0x22000000+size])
    def write(self,address,data):self.memory[address-0x22000000:address-0x22000000+len(data)]=data


class StackStoreDelayPlanningTests(unittest.TestCase):
    def test_beq_taken_and_not_taken_store_once(self):
        for condition,target in ((0,0x1010),(1<<40,0x1008)):
            p=PlanGuest();p.reg('4',condition);ext=StackSpills((0x22000000,4096),UPPER,store_delay=True)
            before=bytes(p.memory);plan=ext.plan(p,0x1000,p.uint(0x1000),set())
            self.assertEqual(bytes(p.memory),before);self.assertEqual(plan['addresses'],[0x1000,0x1004]);self.assertEqual(plan['nextPc'],target)
            ext.commit(p,plan);self.assertEqual(p.reg('PC'),target)
            self.assertEqual(p.read(0x22000800,16),p.reg('16').to_bytes(8,'little')+UPPER[16].to_bytes(8,'little'))
            self.assertEqual(len(ext.events),1);self.assertEqual(ext.upper64,list(UPPER))

    def test_jal_link_visible_to_sq_ra_slot(self):
        p=PlanGuest(0x0c000410,0x7fbf0000);ext=StackSpills((0x22000000,4096),UPPER,store_delay=True)
        plan=ext.plan(p,0x1000,p.uint(0x1000),set());self.assertEqual(p.reg('31'),0x9999)
        ext.commit(p,plan);self.assertEqual(p.reg('31'),0x1008);self.assertEqual(p.reg('PC'),0x1040)
        self.assertEqual(p.read(0x22000800,16),(0x1008).to_bytes(8,'little')+UPPER[31].to_bytes(8,'little'))

    def test_refusals_before_any_link_store_or_trace_mutation(self):
        for word,slot,stop,sp in ((0x10800003,0x7fb00000,{0x1004},0x22000800),
                (0x10800003,0x7fb00000,set(),0x22000801),(0x14800003,0x7fb00000,set(),0x22000800),
                (0x10800003,0x7bb00000,set(),0x22000800),(0x0c001000,0x7fbf0000,set(),0x22000800)):
            p=PlanGuest(word,slot);p.reg('SP',sp);ext=StackSpills((0x22000000,4096),UPPER,store_delay=True)
            before=bytes(p.memory);registers=dict(p.regs)
            with self.assertRaises(ValueError):ext.plan(p,0x1000,word,stop)
            self.assertEqual(bytes(p.memory),before);self.assertEqual(p.regs,registers);self.assertFalse(ext.events);self.assertFalse(p.trace)
        p=PlanGuest();ext=StackSpills((0x22000000,4096),UPPER)
        with self.assertRaisesRegex(ValueError,'before branch'):ext.plan(p,0x1000,p.uint(0x1000),set())
        p.words[0xffc]=0x10800003;p.trace=[0xffc];ext=StackSpills((0x22000000,4096),UPPER,store_delay=True)
        with self.assertRaisesRegex(ValueError,'live branch'):ext.plan(p,0x1000,p.uint(0x1000),set())


if __name__=='__main__':unittest.main()

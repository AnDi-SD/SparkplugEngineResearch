"""Pure explicit-control planning checks;no original guest runs here."""
import unittest
from ps2_scalar_control import ScalarControlPairs,MASK64
from ps2_stack_spills import StackSpills
from ps2_raw_transfers import RawTransfers
from test_support import UPPER

class PlanGuest:
    def __init__(self,word=0x1080000f,slot=0):
        self.ranges=((0x1000,0x100),);self.pages={0x22000000};self.trace=[];self.words={0x1000:word,0x1004:slot}
        self.regs=[0]*32;self.regs[29]=0x22000800;self.memory=bytearray(b'\xa5'*4096)
        self.call_extension=None;self.stack_extension=StackSpills((0x22000000,4096),UPPER);self.raw_transfer_extension=None
    def reg(self,name,value=None):
        index={'SP':29,'RA':31}.get(name,int(name) if name.isdigit() else -1)
        if name=='PC':
            if value is not None:self.pc=value
            return getattr(self,'pc',0)
        if value is not None and index:self.regs[index]=value&MASK64
        return self.regs[index] if index else 0
    def uint(self,address):return self.words[address]
    def read(self,address,size):return bytes(self.memory[address-0x22000000:address-0x22000000+size])
    def write(self,address,data):self.memory[address-0x22000000:address-0x22000000+len(data)]=data

class ScalarControlPlanningTests(unittest.TestCase):
    def extension(self):return ScalarControlPairs([(0x22000000,4096,'rw')])
    def test_beq_and_bne_use_full64_condition_and_run_slot_once(self):
        for opcode,equal,taken in ((4,True,True),(4,False,False),(5,False,True)):
            p=PlanGuest((opcode<<26)|(4<<21)|(5<<16)|15,0x24020007)
            p.reg('4',1<<40);p.reg('5',1<<40 if equal else 0)
            ext=self.extension();plan=ext.plan(p,0x1000,p.uint(0x1000),set());self.assertEqual(p.reg('2'),0)
            ext.commit(p,plan);self.assertEqual(p.reg('2'),7);self.assertEqual(plan['taken'],taken)
            self.assertEqual(p.reg('PC'),0x1040 if taken else 0x1008);self.assertEqual(plan['addresses'],[0x1000,0x1004])
    def test_likely_false_annuls_even_an_invalid_slot(self):
        for opcode,a,b in ((0x14,1,0),(0x15,1,1)):
            p=PlanGuest((opcode<<26)|(4<<21)|(5<<16)|15,0xac82ffff);p.reg('4',a);p.reg('5',b)
            ext=self.extension();before=bytes(p.memory);plan=ext.plan(p,0x1000,p.uint(0x1000),{0x1004});ext.commit(p,plan)
            self.assertEqual(plan['addresses'],[0x1000]);self.assertTrue(plan['annulled']);self.assertEqual(bytes(p.memory),before)
            self.assertEqual(p.reg('PC'),0x1008)
    def test_jalr_alias_uses_old_target_and_slot_observes_new_link(self):
        p=PlanGuest(0x03e0f809,0x03e02025);p.reg('31',0x1040)
        ext=self.extension();plan=ext.plan(p,0x1000,p.uint(0x1000),set());self.assertEqual(p.reg('31'),0x1040)
        ext.commit(p,plan);self.assertEqual(p.reg('31'),0x1008);self.assertEqual(p.reg('4'),0x1008);self.assertEqual(p.reg('PC'),0x1040)
        self.assertEqual(p.stack_extension.upper64,list(UPPER))
    def test_jalr_sp_link_is_visible_through_delegated_alias_before_commit(self):
        # The new SP link is unaligned; the old SP would incorrectly pass.
        p=PlanGuest(0x0080e809,0x7fb00000);p.reg('4',0x1040);ext=self.extension()
        before=list(p.regs),bytes(p.memory)
        with self.assertRaisesRegex(ValueError,'Unaligned'):ext.plan(p,0x1000,p.uint(0x1000),set())
        self.assertEqual((p.regs,bytes(p.memory)),before)
    def test_bltz_bgez_are_signed64_not_signed32(self):
        for rt,value,taken in ((0,0x80000000,False),(1,0x80000000,True),(0,1<<63,True),(1,1<<63,False)):
            p=PlanGuest((1<<26)|(4<<21)|(rt<<16)|15);p.reg('4',value);ext=self.extension()
            plan=ext.plan(p,0x1000,p.uint(0x1000),set());self.assertEqual(plan['taken'],taken)
    def test_j_and_jr_target_and_zero_destination(self):
        for word,value in ((0x08000410,0),(0x00800008,0x10000001040)):
            p=PlanGuest(word,0x24000007);p.reg('4',value);ext=self.extension()
            plan=ext.plan(p,0x1000,word,set());ext.commit(p,plan)
            self.assertEqual(p.reg('PC'),0x1040);self.assertEqual(p.reg('0'),0)
    def test_likely_true_runs_validated_slot_and_respects_zero_link_destination(self):
        for word,a,b in ((0x5085000f,7,7),(0x5485000f,7,8)):
            p=PlanGuest(word,0x24020007);p.reg('4',a);p.reg('5',b);ext=self.extension()
            plan=ext.plan(p,0x1000,word,set());ext.commit(p,plan)
            self.assertEqual(p.reg('2'),7);self.assertEqual(p.reg('PC'),0x1040)
        p=PlanGuest(0x00800009,0);p.reg('4',0x1040);p.reg('31',99);ext=self.extension()
        plan=ext.plan(p,0x1000,p.uint(0x1000),set());ext.commit(p,plan)
        self.assertEqual(p.reg('31'),99);self.assertEqual(p.reg('0'),0)
    def test_stack_and_raw_slots_reuse_explicit_upper_and_fpr_state(self):
        p=PlanGuest(0x0c000410,0x7fbf0000);p.reg('31',99);ext=self.extension()
        plan=ext.plan(p,0x1000,p.uint(0x1000),set());ext.commit(p,plan)
        self.assertEqual(p.read(0x22000800,16),(0x1008).to_bytes(8,'little')+UPPER[31].to_bytes(8,'little'))
        p=PlanGuest(0x1000000f,0xe4800000);p.reg('4',0x22000800)
        p.raw_transfer_extension=RawTransfers([0x80000000]+[0]*31,[(0x22000000,4096,'rw')])
        ext=self.extension();plan=ext.plan(p,0x1000,p.uint(0x1000),set());ext.commit(p,plan)
        self.assertEqual(p.read(0x22000800,4),b'\0\0\0\x80')
    def test_slot_word_arithmetic_signextension_and_load_store_width(self):
        p=PlanGuest();p.reg('4',0x7fffffff);p.reg('5',1);ext=self.extension()
        plan=ext.scalar_slot(p,0x1004,(4<<21)|(5<<16)|(2<<11)|0x21)
        self.assertEqual(plan['writes'],{2:0xffffffff80000000})
        p.reg('4',0x22000800);p.write(0x22000800,b'\0\0\0\x80')
        plan=ext.scalar_slot(p,0x1004,(0x23<<26)|(4<<21)|(2<<16));self.assertEqual(plan['writes'],{2:0xffffffff80000000})
    def test_undefined_noncanonical_word_operands_refuse_before_commit(self):
        for slot,regs in [(0x00028103,{2:0x80000000}),
                ((4<<21)|(5<<16)|(2<<11)|0x21,{4:1<<40,5:1}),
                ((4<<21)|(5<<16)|(2<<11)|0x23,{4:1,5:0x80000000}),
                (0x24820001,{4:0x80000000})]:
            p=PlanGuest(0x1000000f,slot)
            for register,value in regs.items():p.reg(str(register),value)
            ext=self.extension();before=list(p.regs),bytes(p.memory)
            with self.assertRaisesRegex(ValueError,'non-sign-extended32'):ext.plan(p,0x1000,p.uint(0x1000),set())
            self.assertEqual((p.regs,bytes(p.memory)),before);self.assertFalse(ext.events)
    def test_dsra32_slot_uses_signed64_and_preserves_upper_with_preflight(self):
        for value in (0,0x7fffffff00000000,0x8000000000000000,0xffffffffffffffff):
            for shift in (0,31):
                p=PlanGuest(0x0c000410,(5<<16)|(5<<11)|(shift<<6)|0x3f);p.reg('5',value)
                ext=self.extension();plan=ext.plan(p,0x1000,p.uint(0x1000),set())
                self.assertEqual(p.reg('5'),value);ext.commit(p,plan)
                signed_value=value-(1<<64) if value>>63 else value
                self.assertEqual(p.reg('5'),(signed_value>>(32+shift))&MASK64)
                self.assertEqual(p.reg('31'),0x1008);self.assertEqual(p.stack_extension.upper64,list(UPPER))
        p=PlanGuest(0x0c000410,0x0025283f);ext=self.extension();before=list(p.regs),bytes(p.memory)
        with self.assertRaisesRegex(ValueError,'Reserved DSRA32'):ext.plan(p,0x1000,p.uint(0x1000),set())
        self.assertEqual((p.regs,bytes(p.memory)),before);self.assertFalse(ext.events)
    def test_sltiu_uses_unsigned64_signextended_immediate_and_preflight(self):
        for value,immediate,expected in ((0,1,1),(1,1,0),(1<<40,1,0),
                (0,-1,1),(0xffffffff,-1,1),(MASK64,-1,0),
                (MASK64-32767,-32768,0),(0,0,0)):
            p=PlanGuest(0x0c000410,(0x0b<<26)|(2<<21)|(16<<16)|(immediate&65535));p.reg('2',value)
            ext=self.extension();plan=ext.plan(p,0x1000,p.uint(0x1000),set())
            self.assertEqual(p.reg('16'),0);ext.commit(p,plan)
            self.assertEqual(p.reg('16'),expected);self.assertEqual(p.reg('31'),0x1008)
            self.assertEqual(p.stack_extension.upper64,list(UPPER))
        p=PlanGuest(0x0c000410,(0x0b<<26)|(31<<21)|0x1009);ext=self.extension()
        plan=ext.plan(p,0x1000,p.uint(0x1000),set());ext.commit(p,plan)
        self.assertEqual(p.reg('0'),0)
        p=PlanGuest(0x0c000410,0x2c500001);p.reg('2',0);ext=self.extension();before=list(p.regs),bytes(p.memory)
        with self.assertRaises(ValueError):ext.plan(p,0x1000,p.uint(0x1000),{0x1004})
        self.assertEqual((p.regs,bytes(p.memory)),before);self.assertFalse(ext.events)
    def test_unknown_control_slot_target_and_memory_refuse_before_mutation(self):
        for word,slot in ((0x1c80000f,0),(0x1080000f,0x03e00008),(0x1080000f,0xac820001),(0x10008000,0)):
            p=PlanGuest(word,slot);p.reg('4',0x22000800);ext=self.extension();before=list(p.regs),bytes(p.memory)
            with self.assertRaises(ValueError):ext.plan(p,0x1000,word,set())
            self.assertEqual((p.regs,bytes(p.memory)),before);self.assertFalse(ext.events)
        p=PlanGuest(0x1080000f,0xac820000);p.reg('4',0x22000800);ext=ScalarControlPairs([(0x22000000,4096,'r')])
        with self.assertRaises(ValueError):ext.plan(p,0x1000,p.uint(0x1000),set())

if __name__=='__main__':unittest.main()

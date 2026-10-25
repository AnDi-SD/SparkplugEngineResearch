"""Pure ABI/provenance/memory planning tests; no original guest execution."""
import unittest
from ps2_packet_calls import PacketCallBoundaries


class PlanGuest:
    def __init__(self,extension,site,args):
        self.regs={'RA':site+8,**{'A'+str(i):v for i,v in enumerate(args)}}
        self.memory={};self.trace=[site,site+4]
        for a,row in extension.sites.items():self.write(a,bytes.fromhex(row['originalWords']))
        declared=extension.sites[site]
        if declared['kind'] in extension.WRAPPERS:
            target=declared['target'];tail=extension.TAILS[target]
            self.trace+=list(range(target,tail+8,4));self.write(tail,(0x00400008).to_bytes(4,'little')+bytes(4))
    def reg(self,name,value=None):
        if value is not None:self.regs[name]=value
        return self.regs.get(name,0)
    def read(self,address,size):return bytes(self.memory.get(address+i,0) for i in range(size))
    def uint(self,address):return int.from_bytes(self.read(address,4),'little')
    def write(self,address,data):self.memory.update({address+i:v for i,v in enumerate(data)})


class PacketPrimitivePlanningTests(unittest.TestCase):
    def extension(self,**kwargs):
        ext=PacketCallBoundaries((0x21100000,4096),[(0x21000000,4096,'r'),(0x21200000,4096,'rw')],
            {0x15fc84:'aligned_allocate',0x15fbe4:'free',0x15fbd0:'memcpy',0x15fcb0:'memset',0x15fc08:'diagnostic'},**kwargs)
        ext.attached=True
        return ext

    def test_heap_ledger_allocate_and_free_only_exact_live_pointer(self):
        ext=self.extension();p=PlanGuest(ext,0x15fc84,[48,16,0x444468,45]);before=dict(p.memory)
        plan=ext.plan(p,ext.ALIGNED);self.assertFalse(ext.allocations);self.assertEqual(p.memory,before)
        ext.commit(p,plan);self.assertEqual(ext.allocations,{0x21100000:48});self.assertEqual(p.reg('V0'),0x21100000)
        ext.validate_transfer(dict(operation='SWC1',effectiveAddress='2110002C'))
        with self.assertRaises(ValueError):ext.validate_transfer(dict(operation='SWC1',effectiveAddress='21100030'))
        p=PlanGuest(ext,0x15fbe4,[0x21100004,0,0xffffffffffffffff,0xffffffffffffffff])
        with self.assertRaisesRegex(ValueError,'live exact'):ext.plan(p,ext.FREE)
        p.reg('A0',0x21100000);ext.commit(p,ext.plan(p,ext.FREE));self.assertFalse(ext.allocations)
        with self.assertRaises(ValueError):ext.validate_transfer(dict(operation='LWC1',effectiveAddress='21100000'))

    def test_libc_memory_effects_and_permissions_are_planned_first(self):
        ext=self.extension();p=PlanGuest(ext,0x15fbd0,[0x21200000,0x21000000,4,0xffffffffffffffff]);p.write(0x21000000,b'abcd')
        plan=ext.plan(p,0x4074f0);self.assertEqual(p.read(0x21200000,4),bytes(4))
        ext.commit(p,plan);self.assertEqual(p.read(0x21200000,4),b'abcd');self.assertEqual(p.reg('V0'),0x21200000)
        p=PlanGuest(ext,0x15fcb0,[0x21000000,0xff,4,0])
        with self.assertRaisesRegex(ValueError,'writable'):ext.plan(p,0x4076a8)
        p.reg('A0',0x21200000);ext.commit(p,ext.plan(p,0x4076a8));self.assertEqual(p.read(0x21200000,4),b'\xff'*4)

    def test_call_gate_faults_and_call_cap_do_not_mutate(self):
        ext=self.extension(maximum_calls=1);p=PlanGuest(ext,0x15fc84,[48,16,0,0]);before=dict(p.memory)
        p.trace=p.trace[2:]
        with self.assertRaisesRegex(ValueError,'trace'):ext.plan(p,ext.ALIGNED)
        self.assertEqual(p.memory,before);self.assertFalse(ext.events)
        p=PlanGuest(ext,0x15fc84,[48,16,0,0]);ext.commit(p,ext.plan(p,ext.ALIGNED));snapshot=dict(ext.allocations)
        with self.assertRaisesRegex(ValueError,'cap'):ext.plan(p,ext.ALIGNED)
        self.assertEqual(ext.allocations,snapshot)
        with self.assertRaisesRegex(ValueError,'target'):
            PacketCallBoundaries((0x21100000,4096),[(0x21000000,4096,'r')],{0x15f108:'memset'})
        with self.assertRaises(ValueError):
            PacketCallBoundaries((0x21100000,4096),[(0x21100000,4096,'r')],{0x15fc84:'aligned_allocate'})


if __name__=='__main__':unittest.main()

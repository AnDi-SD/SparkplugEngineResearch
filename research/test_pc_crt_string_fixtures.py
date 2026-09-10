"""Regression: a bounded CRT source need not contain a terminating zero."""
import unittest
from pc_instruction_emulator import PcInstructions
from pc_crt_string_fixtures import install_crt_string


class BoundedStrings(unittest.TestCase):
    def setUp(self):
        self.p=PcInstructions()
        self.source=0x34500ffc
        self.p.mu.mem_map(0x34500000,4096)
        self.p.mu.mem_write(self.source,b'ABCD')
        self.destination=self.p.allocate(32)
        self.p.mu.mem_write(self.destination,b'!'*32)
        install_crt_string(self.p)

    def call(self,iat,n,source=None):
        p=self.p;target=p.uint(iat)
        p.run(target,args=(self.destination,self.source if source is None else source,n),callee_pop=False)
        self.assertEqual(p.reg('EAX'),self.destination)

    def test_copy_unterminated_limit(self):
        self.call(0x6d9328,4)
        self.assertEqual(bytes(self.p.mu.mem_read(self.destination,6)),b'ABCD!!')

    def test_copy_zero_ignores_invalid_source(self):
        self.call(0x6d9328,0,0)
        self.assertEqual(bytes(self.p.mu.mem_read(self.destination,6)),b'!!!!!!')

    def test_copy_pads_only_requested_bytes(self):
        self.p.mu.mem_write(self.source,b'A\0')
        self.call(0x6d9328,7)
        self.assertEqual(bytes(self.p.mu.mem_read(self.destination,8)),b'A'+bytes(6)+b'!')

    def test_append_unterminated_limit(self):
        self.p.mu.mem_write(self.destination,b'X\0')
        self.call(0x6d9320,4)
        self.assertEqual(bytes(self.p.mu.mem_read(self.destination,7)),b'XABCD\0!')

    def test_append_zero_ignores_invalid_source(self):
        self.p.mu.mem_write(self.destination,b'X\0')
        self.call(0x6d9320,0,0)
        self.assertEqual(bytes(self.p.mu.mem_read(self.destination,3)),b'X\0!')


if __name__=='__main__':unittest.main()

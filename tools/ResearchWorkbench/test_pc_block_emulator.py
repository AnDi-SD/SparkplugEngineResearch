#!/usr/bin/env python3
"""Confinement and equivalence checks for block tracing; synthetic bytes only."""
from pathlib import Path
import struct,sys,unittest
from unittest.mock import patch
import pc_instruction_emulator as emu
from pc_block_emulator import PcBlocks


class BlockTests(unittest.TestCase):
    def test_extended_profile_requires_block_tracer(self):
        p=emu.PcInstructions(execution_profile='protected-block')
        with self.assertRaisesRegex(ValueError,'requires the validated block tracer'):p.run(0x401000)
        p=PcBlocks(execution_profile='protected-block');p.mu.mem_write(0x401000,b'\xc3');p.run(0x401000)
        self.assertEqual(p.last_execution_limits,dict(profile='protected-block',instructionLimit=60000000,timeoutUs=24000000))

    def test_loop_registers_match_instruction_tracer(self):
        values=[]
        for cls in (emu.PcInstructions,PcBlocks):
            p=cls();p.mu.mem_write(0x401000,bytes.fromhex('b90a00000031c001c8e2fcc3'));p.run(0x401000)
            values.append([p.reg(n) for n in ('EAX','ECX','ESP','EFLAGS')])
        self.assertEqual(values[0],values[1]);self.assertEqual(values[0][0],55)

    def test_boundary_inside_block_is_exact(self):
        p=PcBlocks();p.mu.mem_write(0x401000,bytes.fromhex('b801000000b802000000c3'))
        p.run(0x401000,stop_at=0x401005)
        self.assertEqual(p.reg('EAX'),1);self.assertEqual(p.reg('EIP'),0x401005)

    def test_unexecuted_denied_suffix_does_not_preempt_boundary(self):
        p=PcBlocks();p.mu.mem_write(0x401000,bytes.fromhex('b801000000f4c3'))
        p.run(0x401000,stop_at=0x401005)
        self.assertEqual(p.reg('EAX'),1);self.assertEqual(p.reg('EIP'),0x401005)

    def test_unexecuted_fixture_body_does_not_preempt_prefix(self):
        p=PcBlocks();p.mu.mem_write(0x401000,bytes.fromhex('b801000000f4c3'))
        p.seams[0x401005]=lambda q:q.fixture_return(eax=42);p.run(0x401000)
        self.assertEqual(p.reg('EAX'),42);self.assertEqual(p.reg('EIP'),emu.RETURN)

    def test_seam_membership_frozen_inside_call(self):
        p=PcBlocks();p.mu.mem_write(0x401000,b'\xc3')
        def callback(q):
            with self.assertRaises(TypeError):q.seams[0x401001]=callback
            q.fixture_return(eax=7)
        p.seams[0x401000]=callback;p.run(0x401000);self.assertEqual(p.reg('EAX'),7)
        p.seams[0x401001]=callback # Writable again after the call.

    def test_explicit_seam_and_resynchronization(self):
        p=PcBlocks();p.mu.mem_write(0x401000,b'\xe8'+struct.pack('<i',0x401080-0x401005)+b'\x40\xc3')
        p.mu.mem_write(0x401080,b'\xf4') # Unexecuted explicit fixture body.
        p.seams[0x401080]=lambda q:q.fixture_return(eax=40);p.run(0x401000);self.assertEqual(p.reg('EAX'),41)
        p.seams.pop(0x401080)
        with self.assertRaisesRegex(AssertionError,'OS/privileged'):p.run(0x401000)

    def test_native_instruction_cap_matches_exact_registers(self):
        values=[]
        for cls in (emu.PcInstructions,PcBlocks):
            p=cls();p.mu.mem_write(0x401000,b'\x40\xeb\xfd')
            with patch.object(emu,'INSTRUCTION_LIMIT',33):
                with self.assertRaisesRegex(AssertionError,'instruction/time cap'):p.run(0x401000)
            values.append([p.reg(n) for n in ('EIP','EAX','ESP','EFLAGS')])
        self.assertEqual(values[0],values[1])

    def test_privileged_and_prefixed_io_denied(self):
        for cls in (emu.PcInstructions,PcBlocks):
            for code in ('0f05','f36c','f36e','fa','fb','f4'):
                with self.subTest(cls=cls.__name__,code=code):
                    p=cls();p.mu.mem_write(0x401000,bytes.fromhex(code+'c3'))
                    with self.assertRaisesRegex(AssertionError,'OS/privileged'):p.run(0x401000)

    def test_null_and_external_execution_denied(self):
        p=PcBlocks();p.mu.mem_write(0x401000,bytes.fromhex('a100000000c3'))
        with self.assertRaisesRegex(AssertionError,'invalid memory'):p.run(0x401000)
        p=PcBlocks();p.mu.mem_map(0x33000000,4096);p.mu.mem_write(0x33000000,b'\xc3')
        with self.assertRaisesRegex(AssertionError,'external execution denied'):p.run(0x33000000)

    def test_host_patch_and_cross_page_invalidation(self):
        p=PcBlocks();p.mu.mem_write(0x401fff,b'\x66\x90\xc3');p.run(0x401fff)
        self.assertTrue(any(k[0]==0x401fff for k in p.block_cache))
        p.mu.mem_write(0x402000,b'\xf4');self.assertFalse(any(k[0]==0x401fff for k in p.block_cache))
        with self.assertRaisesRegex(AssertionError,'OS/privileged'):p.run(0x401fff)

    def test_guest_modification_invalidates_cached_target(self):
        p=PcBlocks();p.mu.mem_write(0x401020,b'\x90\xc3');p.run(0x401020)
        p.mu.mem_write(0x401000,b'\xc6\x05'+struct.pack('<I',0x401020)+b'\xf4'+b'\xe9'+struct.pack('<i',0x401020-0x40100c))
        with self.assertRaisesRegex(AssertionError,'OS/privileged'):p.run(0x401000)

    def test_guest_modification_inside_current_block_is_checked(self):
        p=PcBlocks();p.mu.mem_write(0x401000,b'\xc6\x05'+struct.pack('<I',0x401007)+b'\xf4\x90\xc3')
        with self.assertRaisesRegex(AssertionError,'OS/privileged'):p.run(0x401000)


if __name__=='__main__':
    if sys.argv[1:]==['--guest']:unittest.main(argv=[sys.argv[0]])
    else:raise SystemExit(emu.run_bounded(Path(__file__)))

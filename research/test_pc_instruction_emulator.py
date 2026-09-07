#!/usr/bin/env python3
"""Safety/regression checks for the guest-only harness, not game semantics."""
from pathlib import Path
import io
import subprocess
import sys
import unittest
from unittest.mock import patch
import pc_instruction_emulator as emu


class GuardTests(unittest.TestCase):
    def test_hash_before_dependency_loading(self):
        with patch.object(Path,'read_bytes',return_value=b'not the original'):
            with self.assertRaisesRegex(ValueError,'non-pristine'):
                emu.PcInstructions(Path('not-read.exe'))

    def test_outer_process_deadline(self):
        with patch.object(emu.subprocess,'run',side_effect=subprocess.TimeoutExpired('guest',30)) as call, patch('sys.stderr',new_callable=io.StringIO) as captured:
            self.assertEqual(emu.run_bounded(Path('fixture.py')),2)
            self.assertEqual(call.call_args.kwargs['timeout'],30)
            self.assertEqual(call.call_args.args[0][0],sys.executable)
            self.assertIn('exceeded 30 seconds',captured.getvalue())

    def test_child_failure_propagates(self):
        with patch.object(emu.subprocess,'run',return_value=subprocess.CompletedProcess([],7)):
            self.assertEqual(emu.run_bounded(Path('fixture.py')),7)

    def test_arena_bound(self):
        p=emu.PcInstructions()
        with self.assertRaisesRegex(ValueError,'arena exhausted'):
            p.allocate(emu.ARENA_SIZE+1)

    def test_unmapped_memory_denied(self):
        p=emu.PcInstructions()
        # Synthetic guard-only instruction, not used by an evidence fixture.
        p.mu.mem_write(0x401000,b'\xa1\x00\x00\x00\x00\xc3')
        with self.assertRaisesRegex(AssertionError,'invalid memory'):
            p.run(0x401000)

    def test_syscall_denied(self):
        p=emu.PcInstructions()
        p.mu.mem_write(0x401000,b'\x0f\x05\xc3')
        with self.assertRaisesRegex(AssertionError,'OS/privileged'):
            p.run(0x401000)

    def test_explicit_seh_segment_keeps_null_unmapped(self):
        p=emu.PcInstructions()
        teb=p.fixture_seh_chain()
        # mov eax,fs:[0]; ret -- synthetic guard-only instructions.
        p.mu.mem_write(0x401000,b'\x64\xa1\x00\x00\x00\x00\xc3')
        p.run(0x401000)
        self.assertEqual(p.reg('EAX'),0xffffffff)
        self.assertEqual(p.uint(teb),0xffffffff)
        p.mu.mem_write(0x401010,b'\xa1\x00\x00\x00\x00\xc3')
        with self.assertRaisesRegex(AssertionError,'invalid memory'):
            p.run(0x401010)

    def test_instruction_cap(self):
        p=emu.PcInstructions()
        p.mu.mem_write(0x401000,b'\xeb\xfe')
        with patch.object(emu,'INSTRUCTION_LIMIT',32):
            with self.assertRaisesRegex(AssertionError,'instruction/time cap'):
                p.run(0x401000)
        self.assertEqual(sum(p.visits.values()),32)


if __name__=='__main__':
    if sys.argv[1:]==['--guest']:
        unittest.main(argv=[sys.argv[0]])
    else:
        raise SystemExit(emu.run_bounded(Path(__file__)))

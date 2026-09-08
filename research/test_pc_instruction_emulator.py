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

    def test_execution_profile_rejected_before_image_read(self):
        with patch.object(Path, 'read_bytes') as read:
            with self.assertRaisesRegex(ValueError, 'execution profile'):
                emu.PcInstructions(execution_profile='unlimited')
            read.assert_not_called()

    def test_explicit_file_profile_still_bounds_instructions(self):
        p = emu.PcInstructions(execution_profile='file')
        p.mu.mem_write(0x401000, b'\xeb\xfe')
        with patch.object(emu, 'FILE_INSTRUCTION_LIMIT', 48):
            with self.assertRaisesRegex(AssertionError, 'instruction/time cap'):
                p.run(0x401000)
        self.assertEqual(sum(p.visits.values()), 48)
        self.assertEqual(p.last_execution_limits,
                         {'profile': 'file', 'instructionLimit': 48, 'timeoutUs': 8_000_000})

    def test_file_profile_keeps_unmapped_and_privileged_guards(self):
        for code, reason in [(b'\xa1\x00\x00\x00\x00\xc3', 'invalid memory'),
                             (b'\x0f\x05\xc3', 'OS/privileged')]:
            p = emu.PcInstructions(execution_profile='file')
            p.mu.mem_write(0x401000, code)
            with self.assertRaisesRegex(AssertionError, reason): p.run(0x401000)

    def test_cache_mode_rejected_before_image_read(self):
        with patch.object(Path, 'read_bytes') as read:
            with self.assertRaisesRegex(ValueError, 'instruction cache'):
                emu.PcInstructions(code_cache_mode='unchecked')
            read.assert_not_called()

    def test_cached_host_patch_still_denies_privileged_opcode(self):
        for mode in ('bytes', 'page'):
            p = emu.PcInstructions(code_cache_mode=mode)
            p.mu.mem_write(0x401000, b'\x90\xc3')
            p.run(0x401000)
            p.mu.mem_write(0x401000, b'\xf4')
            with self.assertRaisesRegex(AssertionError, 'OS/privileged'):
                p.run(0x401000)

    def test_cached_guest_self_modification_still_checked(self):
        import struct
        for mode in ('bytes', 'page'):
            p = emu.PcInstructions(code_cache_mode=mode)
            p.mu.mem_write(0x401020, b'\x90\xc3')
            p.run(0x401020)
            # mov byte ptr [401020],hlt; jmp 401020 (synthetic guard only).
            p.mu.mem_write(0x401000, b'\xc6\x05'+struct.pack('<I',0x401020)+b'\xf4'
                           +b'\xe9'+struct.pack('<i',0x401020-0x40100c))
            with self.assertRaisesRegex(AssertionError, 'OS/privileged'):
                p.run(0x401000)

    def test_cross_page_instruction_invalidated_from_second_page(self):
        p = emu.PcInstructions()
        p.mu.mem_write(0x401fff, b'\x66\x90\xc3')
        p.run(0x401fff)
        p.mu.mem_write(0x402000, b'\xf4')
        self.assertNotIn(0x401fff, p.code_cache)
        with self.assertRaisesRegex(AssertionError, 'OS/privileged'):
            p.run(0x401fff)

    def test_page_cache_keeps_other_pages_and_invalidates_unmap(self):
        p = emu.PcInstructions()
        for address in (0x401000, 0x402000):
            p.mu.mem_write(address, b'\x90\xc3')
            p.run(address)
        p.mu.mem_write(0x401100, b'\x01')
        self.assertNotIn(0x401000, p.code_cache)
        self.assertIn(0x402000, p.code_cache)
        p.mu.mem_unmap(0x402000, 4096)
        self.assertNotIn(0x402000, p.code_cache)

    def test_cached_host_patch_can_change_instruction_length(self):
        p = emu.PcInstructions()
        p.mu.mem_write(0x401000, b'\x90\xc3')
        p.run(0x401000)
        p.mu.mem_write(0x401000, b'\xb8\x2a\x00\x00\x00\xc3')
        p.run(0x401000)
        self.assertEqual(p.reg('EAX'), 42)


if __name__=='__main__':
    if sys.argv[1:]==['--guest']:
        unittest.main(argv=[sys.argv[0]])
    else:
        raise SystemExit(emu.run_bounded(Path(__file__)))

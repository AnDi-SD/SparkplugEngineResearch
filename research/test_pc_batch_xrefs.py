"""Byte scanner correctness before its results can guide binary analysis."""
import struct
import unittest
from pc_batch_xrefs import cpu_scan, validate, HITS_PER_TARGET


class ByteXrefs(unittest.TestCase):
    def test_unaligned_abs_and_relative_call(self):
        base, target = 0x400000, 0x400010
        data = b'X' + struct.pack('<I', target) + b'\xe8' + struct.pack('<I', (target - base - 10) & 0xffffffff)
        for method in ('find', 'words', 'auto'):
            self.assertEqual(cpu_scan(data, base, [target, target], method=method),
                             {target: [base + 1, (base + 5) | 0x80000000]})

    def test_signed_displacement_and_incomplete_tail(self):
        base = 0x400000
        data = b'\xe8' + struct.pack('<i', -8) + b'\xe8\x00\x00\x00'
        self.assertEqual(cpu_scan(data, base, [base - 3]), {base - 3: [base | 0x80000000]})

    def test_overlapping_literal_windows(self):
        for method in ('find', 'words'):
            self.assertEqual(cpu_scan(b'\xaa' * 6, 10, [0xaaaaaaaa], method=method),
                             {0xaaaaaaaa: [10, 11, 12]})

    def test_explicit_output_limit_before_large_result_allocation(self):
        for method in ('find', 'words'):
            with self.assertRaisesRegex(ValueError, 'candidate limit'):
                cpu_scan(bytes(HITS_PER_TARGET + 4), 0x400000, [0], method=method)

    def test_image_and_target_bounds(self):
        for data, base, targets in [(b'', 0, [1]), (b'a', -1, [1]), (b'ab', 0x7fffffff, [1]),
                                    (b'a', 0, []), (b'a', 0, [-1]), (b'a', 0, [True]),
                                    (b'a', 0, [1 << 32]), (b'a', 0, list(range(8193)))]:
            with self.assertRaises(ValueError): validate(data, base, targets)


if __name__ == '__main__': unittest.main()

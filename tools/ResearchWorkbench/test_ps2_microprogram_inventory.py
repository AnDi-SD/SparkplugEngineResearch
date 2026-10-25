"""Synthetic malformed-input/provenance and byte round-trip checks; no original assets."""
from pathlib import Path
import struct
import tempfile
import unittest

from inspect_executable_architecture import ElfSection
from ps2_microprogram_inventory import (constant_stores, digest, direct_calls,
    extract, parse_container, scan_containers, sections_for, verify_binding)


def packet(parts):
    body = bytearray()
    for destination, payload in parts:
        count = len(payload) // 8
        assert 0 < count <= 256 and len(payload) % 8 == 0
        body += struct.pack('<II', 0, 0x4a000000 | ((count & 255) << 16) | destination)
        body += payload
    body += bytes((-len(body)) % 16)
    return struct.pack('<4I', 0x60000000 | (len(body) // 16), 0, 0, 0) + body


def section(size, *, address=0x1000, offset=0, flags=7):
    return ElfSection('synthetic', 1, address, offset, size, flags)


class ContainerTests(unittest.TestCase):
    def test_count_zero_means_256_and_noncontiguous_source_round_trip(self):
        first = bytes(range(256)) * 8
        second = b'\xff\xff\xff\xff\x00\x00\x00\x00'
        raw = packet([(0, first), (256, second)])
        row = parse_container(raw)
        self.assertEqual(row['instructions'], 257)
        self.assertEqual([c['instructions'] for c in row['chunks']], [256, 1])
        row.update(address=0x1000, file_offset=0)
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            result = extract(raw, row, folder)
            self.assertEqual((folder / result['program']).read_bytes(), first + second)
            self.assertEqual((folder / result['container']).read_bytes(), raw)
            self.assertTrue(result['packet_round_trip_equal'])
            self.assertTrue(result['program_reread_equal'])

    def test_unknown_vif_command_is_rejected_after_valid_upload(self):
        raw = bytearray(packet([(0, bytes(16))]))  # Leaves trailing NOP words after the upload.
        struct.pack_into('<I', raw, len(raw) - 4, 0x14000000)
        with self.assertRaisesRegex(ValueError, 'unsupported VIF'):
            parse_container(raw)

    def test_truncated_dma_body(self):
        with self.assertRaises(ValueError):
            parse_container(packet([(0, bytes(8))])[:-16])

    def test_declared_dma_body_cannot_cross_section(self):
        raw = packet([(0, bytes(8))])
        accepted, rejected = scan_containers(raw[:-4], [section(len(raw) - 4)])
        self.assertFalse(accepted)
        self.assertEqual(len(rejected), 1)
        self.assertIn('file-backed', rejected[0]['reason'])

    def test_gap_overlap_and_nonzero_start_are_not_reassembled(self):
        for parts in [[(1, bytes(8))], [(0, bytes(16)), (1, bytes(8))],
                      [(0, bytes(8)), (2, bytes(8))]]:
            with self.subTest(parts=[p[0] for p in parts]):
                with self.assertRaisesRegex(ValueError, 'gap, overlap'):
                    parse_container(packet(parts))

    def test_vu_address_limit(self):
        parts = [(index * 256, bytes(2048)) for index in range(9)]
        with self.assertRaisesRegex(ValueError, 'maximum VU'):
            parse_container(packet(parts))

    def test_no_mpg_and_nonzero_auxiliary_tag_are_rejected(self):
        for words in [(0x60000001, 0, 0, 0), (0x60000001, 0, 1, 0)]:
            with self.assertRaises(ValueError):
                parse_container(struct.pack('<4I', *words) + bytes(16))

    def test_alignment_and_section_filter_do_not_invent_headers(self):
        raw = b'x' * 4 + packet([(0, bytes(8))])
        accepted, rejected = scan_containers(raw, [section(len(raw))])
        self.assertEqual((accepted, rejected), ([], []))


class ProvenanceTests(unittest.TestCase):
    def test_literal_store_has_value_and_base_instruction_provenance(self):
        # lui a0,0x44; addiu a0,a0,-0x5a00; lui v0,0x4c; sw a0,-0x7d04(v0)
        words = [0x3c040044, 0x2484a600, 0x3c02004c, 0xac4482fc]
        raw = struct.pack('<4I', *words)
        stores = constant_stores(raw, [section(len(raw))])
        self.assertEqual(len(stores), 1)
        self.assertEqual(stores[0]['value'], 0x43a600)
        self.assertEqual(stores[0]['target_address'], 0x4b82fc)
        self.assertEqual(stores[0]['value_provenance'], [0x1000, 0x1004])
        self.assertEqual(stores[0]['base_provenance'], [0x1008])

    def test_unknown_instruction_and_call_invalidate_literal_value(self):
        for barrier in (0x00000001, 0x0c000800):  # Unknown SPECIAL, then JAL.
            raw = struct.pack('<5I', 0x3c040044, 0x2484a600, barrier, 0x3c02004c, 0xac4482fc)
            stores = constant_stores(raw, [section(len(raw))])
            self.assertEqual(stores[0]['value'], None)

    def test_image_pinned_binding_and_literal_reference_both_required(self):
        raw = struct.pack('<8I', 0x3c040044, 0x2484a600, 0x3c02004c, 0xac4482fc,
                          0x3c035000, 0x3c051400, 0xac65000c, 0)
        sections = [section(len(raw))]
        stores = constant_stores(raw, sections)
        proof = dict(address=0x1000, bytes=16, sha256=digest(raw[:16]), role='initializer')
        upload = dict(address=0x1010, bytes=16, sha256=digest(raw[16:]), role='upload')
        binding = dict(pointer_store_address=0x100c, code_evidence=[proof, upload])
        self.assertEqual(verify_binding(raw, sections, dict(address=0x43a600), stores, binding)['pointer_store']['value'], 0x43a600)
        with self.assertRaisesRegex(ValueError, 'literal MIPS'):
            verify_binding(raw, sections, dict(address=0x43a610), stores, binding)
        wrong = dict(upload, sha256='0' * 64)
        with self.assertRaisesRegex(ValueError, 'hash changed'):
            verify_binding(raw, sections, dict(address=0x43a600), stores,
                           dict(binding, code_evidence=[proof, wrong]))
        with self.assertRaisesRegex(ValueError, 'distinct'):
            verify_binding(raw, sections, dict(address=0x43a600), stores,
                           dict(binding, code_evidence=[proof, dict(proof, role='upload')]))
        with self.assertRaisesRegex(ValueError, 'roles'):
            verify_binding(raw, sections, dict(address=0x43a600), stores,
                           dict(binding, code_evidence=[proof, dict(upload, role='initializer')]))
        with self.assertRaisesRegex(ValueError, 'executable'):
            verify_binding(raw, [section(len(raw), flags=3)], dict(address=0x43a600), stores, binding)

    def test_direct_call_scan_keeps_j_and_jal_distinct(self):
        target = 0x2000
        raw = struct.pack('<3I', (3 << 26) | target // 4, 0, (2 << 26) | target // 4)
        self.assertEqual(direct_calls(raw, [section(len(raw))], target),
                         [dict(address=0x1000, kind='JAL'), dict(address=0x1008, kind='J')])

    def test_malformed_elf_is_rejected_before_shared_parser(self):
        raw = bytearray(52)
        raw[:6] = b'\x7fELF\x01\x01'
        struct.pack_into('<H', raw, 18, 8)
        struct.pack_into('<I', raw, 32, 52)
        struct.pack_into('<HHH', raw, 46, 40, 1, 0)
        with self.assertRaisesRegex(ValueError, 'section table'):
            sections_for(raw)


if __name__ == '__main__':
    unittest.main()

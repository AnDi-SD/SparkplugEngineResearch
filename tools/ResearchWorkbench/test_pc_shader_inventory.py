"""Synthetic DXSO framing and PE boundary checks; no original files or SDK."""
import struct
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import pc_shader_inventory as inventory
from pc_shader_inventory import parse_dxso, pe_layout, scan_image, source_kind


def words(*values):
    return struct.pack('<' + 'I' * len(values), *values)


class ShaderInventoryTests(unittest.TestCase):
    def test_duplicate_content_reuses_same_run_inventory_without_overwrite(self):
        raw = bytearray(0x400)
        raw[:2] = b'MZ'
        struct.pack_into('<I', raw, 0x3C, 0x80)
        raw[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<H', raw, 0x86, 1)
        struct.pack_into('<H', raw, 0x94, 0xE0)
        struct.pack_into('<H', raw, 0x98, 0x10B)
        struct.pack_into('<I', raw, 0x98 + 28, 0x400000)
        raw[0x178:0x180] = b'.rdata\0\0'
        struct.pack_into('<IIII', raw, 0x180, 0x100, 0x1000, 0x100, 0x200)
        shader = words(0xFFFE0101, 1, 0xC00F0000, 0x90E40000, 0xFFFF)
        raw[0x200:0x200 + len(shader)] = shader
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first, second, output = root / 'one.exe', root / 'alias.exe', root / 'out'
            first.write_bytes(raw)
            second.write_bytes(raw)
            output.mkdir()
            seen = {}
            canonical = scan_image(first, output, seen=seen)
            extracted = output / canonical['dxsoCandidates'][0]['extracted']
            before = extracted.read_bytes()
            alias = scan_image(second, output, seen=seen)
            self.assertEqual(alias['duplicateOf'], str(first))
            self.assertEqual(alias['sha256'], canonical['sha256'])
            self.assertEqual(len(list(output.iterdir())), 1)
            self.assertEqual(extracted.read_bytes(), before)
            # A new run/API invocation must not adopt an old output silently.
            with self.assertRaises(FileExistsError):
                scan_image(first, output, seen={})
            cli_output = root / 'cli-output'
            arguments = ['pc_shader_inventory.py', str(first), str(second), '--output', str(cli_output)]
            with patch.object(inventory, 'LOCAL', root), patch.object(sys, 'argv', arguments), contextlib.redirect_stdout(io.StringIO()):
                inventory.main()
                manifest = json.loads((cli_output / 'manifest.json').read_text(encoding='utf-8'))
                self.assertEqual(manifest['status'], 'STATIC_SCAN_COMPLETE')
                self.assertEqual(len(manifest['images']), 1)
                self.assertEqual(len(manifest['aliases']), 1)
                self.assertEqual(manifest['aliases'][0]['duplicateOf'], str(first))
                with self.assertRaises(FileExistsError):
                    inventory.main()

    def test_sm1_framing_and_immediate_values(self):
        raw = words(0xFFFE0101, 81, 0xA00F0000, 0, 0x0000FFFF, 0x3F800000, 0,
                    1, 0xC00F0000, 0x90E40000, 0xFFFF)
        result = parse_dxso(raw)
        self.assertEqual(result['validation'], 'framing-valid')
        self.assertEqual(result['length'], len(raw))
        self.assertEqual(result['instructions'], 2)

    def test_sm1_texture_arity_depends_on_minor(self):
        self.assertEqual(parse_dxso(words(0xFFFF0101, 66, 0xB00F0000, 0xFFFF))['validation'], 'framing-valid')
        self.assertEqual(parse_dxso(words(0xFFFF0104, 66, 0x800F0000, 0xB0E40000, 0xFFFF))['validation'], 'framing-valid')
        self.assertEqual(parse_dxso(words(0xFFFF0104, 66, 0xB00F0000, 0xFFFF))['validation'], 'rejected')

    def test_comment_embedded_end_not_terminator_and_truncation(self):
        raw = words(0xFFFE0101, 0x0001FFFE, 0xFFFF, 1, 0xC00F0000, 0x90E40000, 0xFFFF)
        self.assertEqual(parse_dxso(raw)['length'], len(raw))
        self.assertEqual(parse_dxso(words(0xFFFE0101, 0x0010FFFE, 0xFFFF))['reason'], 'truncated-comment')

    def test_unknown_opcode_bad_operands_empty_and_bound(self):
        for raw in (words(0xFFFE0101, 12345, 0xFFFF), words(0xFFFE0101, 1, 3, 4, 0xFFFF), words(0xFFFE0101, 0xFFFF)):
            self.assertEqual(parse_dxso(raw)['validation'], 'rejected')
        raw = words(0xFFFE0101, 1, 0xC00F0000, 0x90E40000, 0xFFFF)
        self.assertEqual(parse_dxso(raw, end=len(raw) - 4)['validation'], 'rejected')

    def test_sm3_lengths_relative_predicate_and_sincos(self):
        for raw in (words(0xFFFE0300, 0x02000001, 0xE00F0000, 0x90E40000, 0xFFFF),
                    words(0xFFFE0300, 0x03000001, 0xE00F0000, 0xA0E42000, 0xB0000000, 0xFFFF),
                    words(0xFFFE0300, 0x13000001, 0xB0001000, 0xE00F0000, 0x90E40000, 0xFFFF),
                    words(0xFFFE0300, 0x02000025, 0x800F0000, 0x90E40000, 0xFFFF)):
            self.assertEqual(parse_dxso(raw)['validation'], 'framing-valid')
        self.assertEqual(parse_dxso(words(0xFFFE0300, 0x03000001, 0xE00F0000, 0x90E40000, 0x90E40000, 0xFFFF))['validation'], 'rejected')

    def test_text_program_vs_fragments_and_messages(self):
        self.assertEqual(source_kind(b'vs.1.1\nmov oPos, v0\n'), 'assembly-program-text-candidate')
        self.assertEqual(source_kind(b'float4 Main(float4 p:POSITION):POSITION { return p; }'), 'HLSL-program-text-candidate')
        self.assertEqual(source_kind(b'BlendWeightCount = %d;\n'), 'source-generation-fragment')
        self.assertEqual(source_kind(b'only vs_1_1 is supported'), 'compiler-language-or-shader-related-string')
        self.assertEqual(source_kind(b'%sShaders\\*.vsh'), 'external-resource-reference')

    def test_malformed_pe_rejected(self):
        for raw in (b'', b'MZ' + bytes(62), b'MZ' + bytes(58) + struct.pack('<I', 0x1000)):
            with self.assertRaises(ValueError):
                pe_layout(raw)


if __name__ == '__main__':
    unittest.main()

import hashlib
import json
import os
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
import unittest

import ps2_vu_disassemble as vu


def row(index=0, lower=0x8000033C, upper=0x000002FF,
        lower_name='MOVE', lower_text='NOP.', upper_name='NOP', upper_text='NOP'):
    return dict(pc=index * 8, lowerWord=lower, upperWord=upper,
                upperSymbol='disExternalVUMI_' + upper_name, upperText=upper_text,
                lowerSymbol='disExternalVUMI_' + lower_name, lowerText=lower_text)


class VUStaticContractTests(unittest.TestCase):
    def test_payload_and_row_bounds_and_endianness(self):
        self.assertEqual(vu.input_words(bytes.fromhex('78563412efcdab90')), [(0x12345678, 0x90ABCDEF)])
        for data in [b'', b'1234567', bytes(16392)]:
            with self.assertRaises(ValueError): vu.input_words(data)
        data = struct.pack('<II', 0x8000033C, 0x000002FF)
        vu.validate_rows(data, [row()])
        for malformed in [[], [dict(row(), pc=8)], [dict(row(), upperWord=False)], [row(), row(1)]]:
            with self.assertRaises(ValueError): vu.validate_rows(data, malformed)

    def test_i_flag_is_immediate_data_not_lower_opcode(self):
        current = row(lower=0xFFFFFFFF, upper=0xF80002FF, lower_name='unused')
        current.update(lowerSymbol='LOI', lowerText='LOI I, 0xffffffff')
        data = struct.pack('<II', current['lowerWord'], current['upperWord'])
        vu.validate_rows(data, [current])
        report = vu.annotate([current])
        self.assertEqual(report['lowerImmediateIndices'], [0])
        self.assertEqual(report['lowerInstructionRecognized'], 0)
        self.assertEqual(report['lowerUnknownIndices'], [])
        self.assertEqual(report['flags'], dict.fromkeys('IEMDT', 1))
        self.assertEqual(vu.reconstruct_encodings([current]), data)
        current['lowerSymbol'] = 'disExternalVUMI_B'
        with self.assertRaises(ValueError): vu.validate_rows(data, [current])

    def test_direct_labels_use_signed_relative_pair_indices(self):
        rows = [row(i) for i in range(4)]
        rows[0] = row(0, lower=(0x20 << 25) | 1, lower_name='B', lower_text='B 1')
        rows[2] = row(2, lower=(0x29 << 25) | 0x7FD, lower_name='IBNE', lower_text='IBNE -3, vi01, vi02')
        report = vu.annotate(rows)
        self.assertEqual(rows[0]['controlFlow']['targetIndexUnwrapped'], 2)
        self.assertEqual(rows[2]['controlFlow']['targetLabel'], 'L_0000')
        self.assertEqual(rows[0]['controlFlow']['delaySlotIndex'], 1)
        self.assertIsNone(rows[0]['controlFlow']['continuationIndex'])
        self.assertEqual(rows[2]['registerOperands']['lower'], ['vi01', 'vi02'])
        self.assertEqual(report['directBranchIndices'], [0, 2])
        self.assertIn('L_0002:', vu.listing(rows))
        self.assertFalse(rows[0]['controlFlow']['hardwareWrapApplied'])

    def test_outside_target_and_indirect_jump_are_not_fabricated_edges(self):
        rows = [row(0, lower=0x40000400, lower_name='B', lower_text='B -1024'),
                row(1, lower=0x48001800, lower_name='JR', lower_text='JR vi03')]
        vu.annotate(rows)
        self.assertEqual(rows[0]['controlFlow']['targetIndexUnwrapped'], -1023)
        self.assertFalse(rows[0]['controlFlow']['targetInExtractedImage'])
        self.assertIsNone(rows[0]['controlFlow']['targetLabel'])
        self.assertIsNone(rows[1]['controlFlow']['target'])
        self.assertIsNone(rows[1]['controlFlow']['delaySlotIndex'])
        self.assertEqual([vu.sign11(x) for x in [0, 0x3FF, 0x400, 0x7FF]], [0, 1023, -1024, -1])

    def test_upstream_typo_retains_symbol_and_text(self):
        current = row(lower=0x5E000000, lower_name='IBGEZ', lower_text='IBEZ 0, vi01')
        report = vu.annotate([current])
        self.assertEqual(current['lowerText'], 'IBEZ 0, vi01')
        self.assertEqual(current['lowerOpcode'], 'IBGEZ')
        self.assertEqual(report['formatterAnomalyIndices'], [0])

    def test_unknown_and_word_display_never_count_as_decoded(self):
        current = row()
        current.update(upperSymbol='disNULL', upperText='*** Bad OP ***', lowerText='.word 8000033c')
        report = vu.annotate([current])
        self.assertEqual(report['upperRecognized'], 0)
        self.assertEqual(report['upperUnknownIndices'], [0])
        self.assertEqual(report['lowerUnknownIndices'], [0])
        self.assertEqual(vu.reconstruct_encodings([current]), struct.pack('<II', 0x8000033C, 0x000002FF))

    def test_exact_dependency_hash_and_git_blob_are_both_required(self):
        with TemporaryDirectory() as temp:
            directory = Path(temp)
            data = b'// synthetic upstream fixture\n'
            (directory / 'test.h').write_bytes(data)
            blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            contract = dict(commit='fixture', spdx='fixture', files=[
                dict(name='test.h', sha256=vu.digest(data), gitBlobSha1=blob)])
            self.assertEqual(vu.verify_sources(directory, contract)['commit'], 'fixture')
            (directory / 'test.h').write_bytes(data + b'\n')
            with self.assertRaises(ValueError): vu.verify_sources(directory, contract)

    def test_fresh_output_stays_inside_designated_root(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            self.assertEqual(vu.bounded_destination(root / 'new', root), root / 'new')
            for bad in [root, root.parent / 'escape']:
                with self.assertRaises(ValueError): vu.bounded_destination(bad, root)
            (root / 'used').mkdir()
            with self.assertRaises(ValueError): vu.bounded_destination(root / 'used', root)


@unittest.skipUnless(os.environ.get('SPARKPLUG_VU_DECODER'), 'explicit compiled standalone adapter not supplied')
class CompiledDecoderTests(unittest.TestCase):
    def test_four_accumulator_cells_use_interpreter_dispatch(self):
        decoder = Path(os.environ['SPARKPLUG_VU_DECODER']).resolve()
        data = b''.join(struct.pack('<II', 0x8000033C, value) for value in [0x7C, 0x7D, 0x7E, 0x7F])
        rows = vu.decode_program(decoder, data)
        report = vu.annotate(rows)
        self.assertEqual([r['upperOpcode'] for r in rows], ['SUBAx', 'SUBAy', 'SUBAz', 'SUBAw'])
        self.assertTrue(all('ACC' in r['upperText'] for r in rows))
        self.assertEqual(report['repairedUpperIndices'], [0, 1, 2, 3])

    def test_upstream_real_decode_immediate_and_unknown(self):
        decoder = Path(os.environ['SPARKPLUG_VU_DECODER']).resolve()
        commit = json.loads(vu.CONTRACT.read_text(encoding='utf-8'))['commit']
        vu.verify_decoder(decoder, vu.digest(vu.read_bounded(decoder, vu.MAX_DECODER_BYTES)), commit)
        data = struct.pack('<IIIIII', 0x8000033C, 0x000002FF,
                           0x3F800000, 0x800002FF, 0xFFFFFFFF, 0x00000030)
        rows = vu.decode_program(decoder, data)
        report = vu.annotate(rows)
        self.assertEqual(rows[0]['upperOpcode'], 'NOP')
        self.assertEqual(rows[0]['lowerOpcode'], 'MOVE')
        self.assertEqual(rows[1]['lowerKind'], 'immediate-I')
        self.assertEqual(report['upperUnknownIndices'], [2])
        self.assertEqual(report['lowerUnknownIndices'], [2])
        self.assertEqual(vu.reconstruct_encodings(rows), data)


if __name__ == '__main__':
    unittest.main()

"""Specimen verification safeguards with synthetic bytes and no file writes."""
import copy
import hashlib
import io
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from native_specimens import verify, MAX_FILE


class SpecimenTests(unittest.TestCase):
    def setUp(self):
        self.raw = bytes.fromhex('7856341253424f4f01020304')
        self.data = dict(kind='pc-smo-variant-specimens', absent=[], witnesses=[dict(
            sourcePath='local-data/pc-pristine/Media/__test__.smo', byte_size=len(self.raw),
            sha256=hashlib.sha256(self.raw).hexdigest(), physical_offset=0, type_hash=0x12345678,
            fields=[dict(absolute_payload_offset=8, payload_size=4,
                         payload_sha256=hashlib.sha256(self.raw[8:]).hexdigest())])])

    def run_fixture(self, data):
        with patch.object(Path, 'open', side_effect=lambda *args, **kwargs: io.BytesIO(self.raw)), patch.object(Path, 'stat', return_value=SimpleNamespace(st_size=len(self.raw))):
            return verify(data)

    def test_valid_structural_fixture(self):
        self.assertEqual(self.run_fixture(self.data), 0)

    def test_platform_boundary(self):
        self.data['witnesses'][0]['sourcePath'] = 'local-data/Winx Club the game PS2/fake.smo'
        with self.assertRaisesRegex(ValueError, 'pristine PC'):
            self.run_fixture(self.data)

    def test_changed_fingerprint(self):
        self.data['witnesses'][0]['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'changed since'):
            self.run_fixture(self.data)

    def test_identity_and_field_range(self):
        data = copy.deepcopy(self.data)
        data['witnesses'][0]['type_hash'] = 1
        with self.assertRaisesRegex(ValueError, 'identity'):
            self.run_fixture(data)
        self.data['witnesses'][0]['fields'][0]['payload_size'] = 100
        with self.assertRaisesRegex(ValueError, 'outside file'):
            self.run_fixture(self.data)

    def test_payload_hash(self):
        self.data['witnesses'][0]['fields'][0]['payload_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'payload mismatch'):
            self.run_fixture(self.data)

    def test_count_cap(self):
        self.data['witnesses'] *= 129
        with self.assertRaisesRegex(ValueError, 'witness count'):
            self.run_fixture(self.data)

    def test_oversized_file_is_not_opened(self):
        with patch.object(Path, 'stat', return_value=SimpleNamespace(st_size=MAX_FILE + 1)), patch.object(Path, 'open') as opened:
            with self.assertRaisesRegex(ValueError, 'File cap'):
                verify(self.data)
            opened.assert_not_called()

    def test_actual_read_is_bounded_if_file_grows_after_stat(self):
        with patch.object(Path, 'stat', return_value=SimpleNamespace(st_size=12)), patch.object(Path, 'open') as opened:
            stream = opened.return_value.__enter__.return_value
            stream.read.return_value = b'\0' * (MAX_FILE + 1)
            with self.assertRaisesRegex(ValueError, 'File cap exceeded during read'):
                verify(self.data)
            stream.read.assert_called_once_with(MAX_FILE + 1)

    def test_field_sample_cap(self):
        self.data['witnesses'][0]['fields'] *= 33
        with self.assertRaisesRegex(ValueError, 'Field count cap'):
            self.run_fixture(self.data)

    def test_aggregate_budget_checked_before_open(self):
        with patch('native_specimens.MAX_TOTAL', 10), patch.object(Path, 'stat', return_value=SimpleNamespace(st_size=12)), patch.object(Path, 'open') as opened:
            with self.assertRaisesRegex(ValueError, 'Aggregate witness memory cap'):
                verify(self.data)
            opened.assert_not_called()

    def test_growing_file_read_uses_remaining_aggregate_budget(self):
        with patch('native_specimens.MAX_TOTAL', 10), patch.object(Path, 'stat', return_value=SimpleNamespace(st_size=4)), patch.object(Path, 'open') as opened:
            stream = opened.return_value.__enter__.return_value
            stream.read.return_value = b'\0' * 11
            with self.assertRaisesRegex(ValueError, 'Aggregate witness memory cap exceeded during read'):
                verify(self.data)
            stream.read.assert_called_once_with(11)


if __name__ == '__main__':
    unittest.main()

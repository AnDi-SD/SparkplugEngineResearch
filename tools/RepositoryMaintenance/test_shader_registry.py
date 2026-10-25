"""Keep shader source references and the finite corpus honest without game files."""
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from shader_registry import REGISTRY, validate


class ShaderRegistry(unittest.TestCase):
    def fixture(self, root):
        source = 'Sparkplug/Shaders/PC/Example.hlsl'
        card = 'docs/reference/shaders/example.md'
        row = {'id': 'pc.example.vs', 'platform': 'pc', 'stage': 'vertex',
               'original': {'file': 'Example.vsh', 'profile': 'vs_1_1'},
               'reconstruction': {'source': source, 'entryPoint': 'ExampleVS',
                                  'profile': 'vs_3_0', 'status': 'semantic-reconstruction', 'defines': {}},
               'documentation': card}
        data = {'schemaVersion': 1, 'scope': {'originalFileCount': 1}, 'programs': [row]}
        for path, text in [(source, 'float4 ExampleVS(float4 p : POSITION) : POSITION { return p; }'),
                           (card, '# Example\n\nA documented source-stage contract.')]:
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding='utf-8')
        self.write(root, data)
        return data, {source, card, REGISTRY}

    def write(self, root, data):
        path = root / REGISTRY
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data), encoding='utf-8')

    def with_ps2(self, data):
        data['schemaVersion'] = 2
        data['pcExternalScope'] = data.pop('scope')
        data['scope'] = {'platforms': ['pc', 'ps2'], 'completeness': 'open'}
        card = data['programs'][0]['documentation']
        data['binaryPrograms'] = [{
            'id': 'ps2.vu.00004000', 'platform': 'ps2', 'kind': 'vu-upload-image',
            'original': {'file': 'GAME.ELF', 'containerAddress': '0x00004000',
                         'containerByteLength': 32, 'payloadBytes': 8, 'instructionPairs': 1, 'uploadChunks': 1},
            'extractionStatus': 'structure-validated', 'semanticStatus': 'unrecovered',
            'unit': None, 'runtimeQualified': False, 'documentation': card}]
        data['completeness'] = {'status': 'open', 'documentation': card, 'requirements': [
            {'id': name, 'status': 'open'} for name in (
                'input-cohort', 'external-resources', 'pc-embedded-and-generated',
                'captured-program-source-identity', 'ps2-upload-images',
                'producer-dispatch-closure', 'material-constant-packet-contracts', 'behavior-and-scenes')]}
        return data

    def test_source_archive_requires_no_original_game_or_sdk(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            _, public = self.fixture(root)
            self.assertEqual(validate(root, public), [])

    def test_same_original_stage_cannot_be_counted_twice(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            data, public = self.fixture(root)
            duplicate = json.loads(json.dumps(data['programs'][0]))
            duplicate['id'] = 'pc.second.vs'
            data['programs'].append(duplicate)
            self.write(root, data)
            self.assertTrue(any('duplicate original stage' in e for e in validate(root, public)))

    def test_wrong_profile_private_target_and_unlisted_shader_are_rejected(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            data, public = self.fixture(root)
            data['programs'][0]['reconstruction']['profile'] = 'ps_3_0'
            data['programs'][0]['documentation'] = '../../outside.md'
            orphan = 'Sparkplug/Shaders/PC/Unlisted.hlsl'
            (root / orphan).write_text('float4 Unlisted() { return 0; }', encoding='utf-8')
            public.add(orphan)
            self.write(root, data)
            errors = validate(root, public)
            self.assertTrue(any('profile/status mismatch' in e for e in errors))
            self.assertTrue(any('missing/nonpublic' in e for e in errors))
            self.assertTrue(any('unlisted reconstruction' in e for e in errors))

    def test_comment_is_not_a_source_entry_definition(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            data, public = self.fixture(root)
            (root / data['programs'][0]['reconstruction']['source']).write_text('// ExampleVS(p)\n', encoding='utf-8')
            self.assertTrue(any('missing source entry' in e for e in validate(root, public)))

    def test_ps2_upload_image_is_separate_from_pc_source_count(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            data, public = self.fixture(root)
            self.with_ps2(data)
            self.write(root, data)
            self.assertEqual(validate(root, public), [])

    def test_upload_bytes_and_containers_cannot_be_double_counted(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            data, public = self.fixture(root)
            self.with_ps2(data)
            other = json.loads(json.dumps(data['binaryPrograms'][0]))
            other['id'] = 'ps2.vu.00004010'
            other['original']['containerAddress'] = '0x00004010'
            other['original']['payloadBytes'] = 16
            data['binaryPrograms'].append(other)
            self.write(root, data)
            errors = validate(root, public)
            self.assertTrue(any('inconsistent upload sizes' in e for e in errors))
            self.assertTrue(any('overlapping binary' in e for e in errors))

    def test_open_requirements_cannot_be_called_complete(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            data, public = self.fixture(root)
            self.with_ps2(data)
            data['scope']['completeness'] = data['completeness']['status'] = 'complete'
            self.write(root, data)
            self.assertTrue(any('claim exceeds' in e for e in validate(root, public)))

    def test_extraction_is_not_hardware_qualification(self):
        with TemporaryDirectory() as temp:
            root = Path(temp)
            data, public = self.fixture(root)
            self.with_ps2(data)
            data['binaryPrograms'][0]['unit'] = 'vu1'
            data['binaryPrograms'][0]['runtimeQualified'] = True
            self.write(root, data)
            self.assertTrue(any('hardware qualification' in e for e in validate(root, public)))


if __name__ == '__main__':
    unittest.main()

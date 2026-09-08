"""Readiness arithmetic, evidence integrity and scope-change regressions."""
import copy
import tempfile
from pathlib import Path
import unittest

from tool_readiness import build_report, compare_baseline, sha256


class ReadinessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'proof.md').write_bytes(b'checked operation\n')
        self.scope = {'kind': 'tool-readiness-scope', 'schemaVersion': 1,
                      'scopeId': 'operations-v1', 'features': [
                          {'id': identity, 'title': identity, 'group': 'test',
                           'tools': ['Tool'], 'acceptance': 'Operation produces the checked output'}
                          for identity in ('one', 'two')]}
        ref = {'path': 'proof.md', 'sha256': sha256(b'checked operation\n'),
               'observation': 'Recorded representative output'}
        self.entry = {'featureId': 'one', 'blockers': [], 'stages': {
            stage: {'status': 'passed', 'evidence': [copy.deepcopy(ref)]}
            for stage in ('contract', 'integration', 'validation')}}
        self.assessment = {'kind': 'tool-readiness-assessment', 'schemaVersion': 1,
                           'assessmentId': 'baseline', 'scopeId': 'operations-v1',
                           'scopeSha256': 'scope-hash', 'assessedUtc': '2026-09-08T06:00:00Z',
                           'assessments': [self.entry]}

    def report(self):
        return build_report(self.scope, self.assessment, 'scope-hash', root=self.root)

    def test_unassessed_operations_remain_in_denominator(self):
        report = self.report()
        self.assertEqual((report['readyCount'], report['totalCount'], report['readyPercent']), (1, 2, 50))
        self.assertEqual(report['features'][1]['state'], 'unassessed')
        self.assertEqual(report['evidenceFilesRead'], 1)

    def test_partial_or_blocked_operation_does_not_receive_fractional_credit(self):
        self.entry['stages']['validation']['status'] = 'partial'
        self.assertEqual(self.report()['readyCount'], 0)
        self.entry['stages']['validation']['status'] = 'passed'
        self.entry['blockers'] = ['Command is disabled']
        self.assertEqual(self.report()['readyCount'], 0)

    def test_changed_evidence_requires_review_without_rewriting_baseline(self):
        baseline = self.report()
        (self.root / 'proof.md').write_bytes(b'changed code or result\n')
        current = self.report()
        self.assertEqual(current['features'][0]['state'], 'needs_review')
        self.assertEqual(current['features'][0]['staleEvidence'], ['proof.md'])
        delta = compare_baseline(current, baseline)
        self.assertEqual(delta['noLongerReady'], ['one'])
        self.assertEqual(delta['percentagePointDelta'], -50)
        self.assertEqual(baseline['readyCount'], 1)

    def test_all_passed_stages_need_evidence(self):
        self.entry['stages']['integration']['evidence'] = []
        with self.assertRaisesRegex(ValueError, 'explicit evidence'):
            self.report()

    def test_scope_change_cannot_look_like_cycle_progress(self):
        baseline = self.report()
        baseline['scopeSha256'] = 'different-scope'
        with self.assertRaisesRegex(ValueError, 'different readiness scopes'):
            compare_baseline(self.report(), baseline)
        self.assessment['scopeSha256'] = 'different-scope'
        with self.assertRaisesRegex(ValueError, 'different scope'):
            self.report()

    def test_unknown_and_duplicate_assessment_ids_rejected(self):
        self.assessment['assessments'].append(copy.deepcopy(self.entry))
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            self.report()
        self.assessment['assessments'][-1]['featureId'] = 'outside-scope'
        with self.assertRaisesRegex(ValueError, 'Unknown'):
            self.report()

    def test_local_artifacts_cannot_be_hidden_by_relative_path_components(self):
        (self.root / 'local-data').mkdir()
        (self.root / 'local-data/proof.md').write_bytes(b'checked operation\n')
        self.entry['stages']['contract']['evidence'][0]['path'] = 'sub/../local-data/proof.md'
        with self.assertRaisesRegex(ValueError, 'repository text artifact'):
            self.report()


if __name__ == '__main__':
    unittest.main()

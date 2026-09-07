"""In-memory goal-accounting tests: no executable, game or production DB writes."""
import copy
import json
import sqlite3
import unittest

from native_knowledge import DDL as CATALOG_DDL
from native_platform_knowledge import ensure_schema
from native_goal_coverage import DDL, REQUIRED_PC_GATES, aggregate, import_scope, record, report, validate_scope


class Manifest:
    def __init__(self, value):
        self.value = value

    def read_bytes(self):
        return json.dumps(self.value).encode()

    def __str__(self):
        return 'memory-goal-test.json'


class GoalCoverageTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.row_factory = sqlite3.Row
        self.db.executescript(CATALOG_DDL)
        ensure_schema(self.db)
        self.db.executescript(DDL)
        for identity, name, pc, ps2 in ((1, 'spShared', 1, 1), (2, 'spPC', 1, 0), (3, 'spPS2', 0, 1)):
            self.db.execute('INSERT INTO native_types(id,type_hash,class_name,owner_scope,on_pc,on_ps2,updated_utc) VALUES(?,?,?,\'engine\',?,?,\'fixture\')', (identity, identity, name, pc, ps2))
        path = 'research/test_native_goal_coverage.py'
        self.manifest = dict(kind='native-goal-scope', schemaVersion=1, scopeId='fixture-v1', createdUtc='2026-09-06T20:00:00Z', notes='Fixture only',
            groups=[dict(id='base', title='Base', classes=['spShared','spPC','spPS2'], basis='Test', evidencePaths=[path])],
            gates=[dict(id='read', title='Read', platform='pc', status='partial', acceptance='All read paths', current='Partial', evidencePaths=[path])],
            unregisteredObligations=[dict(label='Unnamed helper', originalName=None, gates=['read'], evidencePaths=[path])])
        self.manifest['gates'].extend(dict(id=key,title=key,platform='pc',status='passed',acceptance='Fixture',current='Fixture only',evidencePaths=[path]) for key in sorted(REQUIRED_PC_GATES-{'read'}))
        self.db.execute("INSERT INTO native_platform_imports VALUES('fixture','fixture','hash','2026-09-06T20:00:00Z','test')")
        self.add_score(1, 'pc', 80)
        self.add_score(1, 'ps2', 20)

    def tearDown(self):
        self.db.close()

    def add_score(self, identity, platform, score, status='partial'):
        self.db.execute('INSERT OR REPLACE INTO native_platform_assessments VALUES(?,?,?,?,?,?,?,?,?,?,?,?)',
                        (identity, platform, score, max(0,score-10), min(100,score+10), status,
                         'reviewed_existing','Test','[]','[]','fixture','2026-09-06T20:00:00Z'))

    def imported(self):
        import_scope(self.db, Manifest(self.manifest))
        return report(self.db, details=True)

    def test_independent_platforms_and_unrated_denominator(self):
        value = self.imported()['platforms']
        self.assertEqual(value['pc']['workflow']['creditedPercent'], 40)
        self.assertEqual(value['ps2']['workflow']['creditedPercent'], 10)
        self.assertEqual(value['pc']['workflow']['unratedCount'], 1)
        self.assertEqual(value['pc']['workflow']['denominator'], 2)
        self.assertEqual(value['ps2']['workflow']['denominator'], 2)

    def test_live_recalculation_not_stale_snapshot(self):
        old = self.imported()
        record(self.db)
        self.add_score(2, 'pc', 60)
        new = report(self.db)
        self.assertEqual(new['platforms']['pc']['workflow']['creditedPercent'], 70)
        self.assertNotEqual(old['assessmentSha256'], new['assessmentSha256'])
        self.assertEqual(new['platforms']['ps2']['workflow']['creditedPercent'], 10)

    def test_gate_pass_alone_cannot_finish(self):
        self.manifest['gates'][0]['status'] = 'passed'
        value = self.imported()['platforms']['pc']
        self.assertFalse(value['completion']['ready'])
        self.assertEqual(value['completion']['passed'], 7)

    def test_all_class_scores_alone_cannot_finish(self):
        self.add_score(1,'pc',100,'closed')
        self.add_score(2,'pc',100,'closed')
        self.assertFalse(self.imported()['platforms']['pc']['completion']['ready'])

    def test_both_conditions_required_for_finish(self):
        self.manifest['gates'][0]['status'] = 'passed'
        self.add_score(1,'pc',100,'closed')
        self.add_score(2,'pc',100,'closed')
        value = self.imported()['platforms']
        self.assertTrue(value['pc']['completion']['ready'])
        self.assertFalse(value['ps2']['completion']['ready'])

    def test_duplicate_and_unknown_members_rejected(self):
        for name in ('spPC', 'invented'):
            with self.subTest(name=name):
                value = copy.deepcopy(self.manifest)
                value['groups'][0]['classes'].append(name)
                with self.assertRaises(ValueError):
                    validate_scope(self.db,value)

    def test_observed_asset_class_cannot_be_dropped(self):
        self.db.execute("INSERT INTO native_type_scopes(native_type_id,scope_key,evidence_status,provenance,updated_utc) VALUES(2,'smo_san','confirmed','fixture','fixture')")
        self.manifest['groups'][0]['classes'].remove('spPC')
        with self.assertRaisesRegex(ValueError,'Missing observed'):
            validate_scope(self.db,self.manifest)

    def test_scope_immutable_and_import_idempotent(self):
        self.assertTrue(import_scope(self.db,Manifest(self.manifest)))
        self.assertFalse(import_scope(self.db,Manifest(self.manifest)))
        self.manifest['notes'] = 'Changed'
        with self.assertRaisesRegex(ValueError,'immutable'):
            import_scope(self.db,Manifest(self.manifest))

    def test_record_idempotence_and_auditable_change(self):
        self.imported()
        self.assertEqual(record(self.db),(1,True))
        self.assertEqual(record(self.db),(1,False))
        self.add_score(2,'pc',40)
        self.assertEqual(record(self.db),(2,True))
        self.assertEqual(self.db.execute('PRAGMA foreign_key_check').fetchall(),[])

    def test_catalog_change_also_invalidates_snapshot(self):
        old = self.imported()
        record(self.db)
        self.db.execute("INSERT INTO native_types(id,type_hash,class_name,owner_scope,on_pc,on_ps2,updated_utc) VALUES(4,4,'spNew','engine',1,0,'fixture')")
        new = report(self.db)
        self.assertEqual(old['assessmentLedgerSha256'],new['assessmentLedgerSha256'])
        self.assertNotEqual(old['assessmentSha256'],new['assessmentSha256'])
        self.assertEqual(record(self.db),(2,True))

    def test_helper_obligation_and_evidence_are_required(self):
        value = copy.deepcopy(self.manifest)
        value['unregisteredObligations'] = []
        with self.assertRaisesRegex(ValueError,'Non-RTTI'):
            validate_scope(self.db,value)
        value = copy.deepcopy(self.manifest)
        value['unregisteredObligations'][0]['gates'] = ['missing']
        with self.assertRaisesRegex(ValueError,'Unknown helper'):
            validate_scope(self.db,value)
        value = copy.deepcopy(self.manifest)
        value['groups'][0]['evidencePaths'] = ['missing-file']
        with self.assertRaisesRegex(ValueError,'Invalid evidence'):
            validate_scope(self.db,value)

    def test_empty_aggregate_is_not_one_hundred_percent(self):
        self.assertIsNone(aggregate([])['creditedPercent'])

    def test_cannot_drop_a_completion_criterion(self):
        self.manifest['gates'].pop()
        with self.assertRaisesRegex(ValueError,'seven PC'):
            validate_scope(self.db,self.manifest)

    def test_scope_change_requires_reason(self):
        self.imported()
        self.manifest.update(scopeId='fixture-v2',createdUtc='2026-09-06T20:01:00Z')
        self.manifest['groups'][0]['classes'].remove('spPC')
        with self.assertRaisesRegex(ValueError,'explicit reason'):
            import_scope(self.db,Manifest(self.manifest))
        self.manifest['scopeChangeReason']='Fixture-only evidence: excluded path'
        self.assertTrue(import_scope(self.db,Manifest(self.manifest)))


if __name__ == '__main__':
    unittest.main()

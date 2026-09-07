"""Platform ledger tests use an in-memory catalog, never the corpus database."""
import copy
import json
import sqlite3
import unittest
from native_knowledge import DDL as LEGACY_DDL
from native_platform_knowledge import apply_manifest, coverage, ensure_schema


class Manifest:
    def __init__(self, data):
        self.data = data

    def read_bytes(self):
        return json.dumps(self.data, sort_keys=True).encode()

    def __str__(self):
        return 'in-memory-platform-test.json'


class PlatformTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.row_factory = sqlite3.Row
        self.db.execute('PRAGMA foreign_keys=ON')
        self.db.executescript(LEGACY_DDL)
        ensure_schema(self.db)
        for identity, name, owner, pc, ps2 in ((1, 'spShared', 'engine', 1, 1),
                                              (2, 'spPC', 'engine', 1, 0),
                                              (3, 'spGame', 'game', 1, 1)):
            self.db.execute('INSERT INTO native_types(id,type_hash,class_name,owner_scope,on_pc,on_ps2,updated_utc) VALUES(?,?,?,?,?,?,?)',
                            (identity, identity, name, owner, pc, ps2, 'fixture'))
        self.db.commit()

    def tearDown(self):
        self.db.close()

    def manifest(self, platform='pc', name='spShared', stamp='2026-09-06T07:40:00Z'):
        item = dict(className=name, platformKey=platform, coverageScore=70,
                    lowerBound=60, upperBound=80, researchStatus='partial',
                    assessmentOrigin='reviewed_existing', summary='Explicit test evidence.',
                    evidenceRefs=[dict(platformKey=platform,
                        sourcePath='research/test_native_platform_knowledge.py',
                        locator='fixture', observation='Test only, not engine evidence.')])
        return Manifest(dict(kind='native-platform-research', schemaVersion=1, manifestId=stamp,
                             createdUtc=stamp, assessments=[item]))

    def test_pc_does_not_confer_ps2_credit(self):
        apply_manifest(self.db, self.manifest())
        self.assertAlmostEqual(coverage(self.db, 'pc', 'all')['creditedPercent'], 70 / 3)
        ps2 = coverage(self.db, 'ps2', 'all')
        self.assertEqual((ps2['assessedCount'], ps2['unratedCount']), (0, 2))
        self.assertEqual(ps2['possibleUpperPercent'], 100)
        self.assertEqual(coverage(self.db, 'pc', 'game')['assessedCount'], 0)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM native_coverage_snapshots').fetchone()[0], 0)

    def test_independent_ps2_assessment_preserves_pc(self):
        apply_manifest(self.db, self.manifest())
        item = self.manifest('ps2', stamp='2026-09-06T07:41:00Z')
        item.data['assessments'][0].update(coverageScore=30, lowerBound=20, upperBound=40)
        apply_manifest(self.db, item)
        self.assertAlmostEqual(coverage(self.db, 'pc', 'engine')['creditedPercent'], 35)
        self.assertAlmostEqual(coverage(self.db, 'ps2', 'engine')['creditedPercent'], 30)
        self.assertEqual(self.db.execute('PRAGMA foreign_key_check').fetchall(), [])

    def test_common_and_cross_platform_evidence_rejected(self):
        with self.assertRaisesRegex(ValueError, 'explicit pc or ps2'):
            apply_manifest(self.db, self.manifest('common'))
        item = self.manifest()
        item.data['assessments'][0]['evidenceRefs'][0]['platformKey'] = 'ps2'
        with self.assertRaisesRegex(ValueError, 'Cross-platform'):
            apply_manifest(self.db, item)

    def test_absent_platform_class_rejected(self):
        with self.assertRaisesRegex(ValueError, 'absent from ps2'):
            apply_manifest(self.db, self.manifest('ps2', 'spPC'))

    def test_repeat_and_immutable(self):
        item = self.manifest()
        self.assertEqual(apply_manifest(self.db, item), (1, True))
        self.assertEqual(apply_manifest(self.db, item), (0, False))
        item.data['assessments'][0]['summary'] = 'Changed'
        with self.assertRaisesRegex(ValueError, 'immutable'):
            apply_manifest(self.db, item)

    def test_out_of_order_even_for_different_class(self):
        apply_manifest(self.db, self.manifest())
        with self.assertRaisesRegex(ValueError, 'Out-of-order'):
            apply_manifest(self.db, self.manifest(name='spPC', stamp='2026-09-06T07:39:00Z'))

    def test_invalid_batch_writes_nothing(self):
        item = self.manifest()
        invalid = copy.deepcopy(item.data['assessments'][0])
        invalid.update(className='spPC', coverageScore=float('nan'))
        item.data['assessments'].append(invalid)
        with self.assertRaisesRegex(ValueError, 'Invalid score'):
            with self.db:
                apply_manifest(self.db, item)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM native_platform_imports').fetchone()[0], 0)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM native_platform_assessments').fetchone()[0], 0)

    def test_missing_evidence_and_hash_mismatch(self):
        item = self.manifest()
        item.data['assessments'][0]['evidenceRefs'][0]['sourceSha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            apply_manifest(self.db, item)
        item.data['assessments'][0]['evidenceRefs'] = []
        with self.assertRaisesRegex(ValueError, 'needs evidence'):
            apply_manifest(self.db, item)

    def test_empty_scope_has_no_percentage(self):
        self.assertIsNone(coverage(self.db, 'pc', 'smo_san'))

    def test_empty_and_invalid_timestamp_rejected(self):
        item = self.manifest()
        item.data['assessments'] = []
        with self.assertRaisesRegex(ValueError, 'Empty'):
            apply_manifest(self.db, item)
        item.data['createdUtc'] = 'not a timestamp'
        with self.assertRaises(ValueError):
            apply_manifest(self.db, item)

    def test_unknown_or_missing_schema_writes_nothing(self):
        before = self.db.total_changes
        for version in (None, 0, 2, True, '1'):
            with self.subTest(version=version):
                item = self.manifest()
                if version is None:
                    del item.data['schemaVersion']
                else:
                    item.data['schemaVersion'] = version
                with self.assertRaisesRegex(ValueError, 'manifest schema'):
                    apply_manifest(self.db, item)
        self.assertEqual(self.db.total_changes, before)

    def test_invalid_manifest_identity_writes_nothing(self):
        before = self.db.total_changes
        for identity in (None, '', '   ', 123):
            with self.subTest(identity=identity):
                item = self.manifest()
                item.data['manifestId'] = identity
                with self.assertRaisesRegex(ValueError, 'manifest ID'):
                    apply_manifest(self.db, item)
        self.assertEqual(self.db.total_changes, before)


if __name__ == '__main__':
    unittest.main()

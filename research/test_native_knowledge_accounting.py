"""Small in-memory regressions; never opens the game-resource database."""
import json
import sqlite3
import unittest
from native_knowledge import DDL, apply_manifest


class Manifest:
    def __init__(self, data):
        self.data = data
    def read_bytes(self):
        return json.dumps(self.data, sort_keys=True).encode()
    def __str__(self):
        return 'in-memory-test-manifest.json'


class AccountingTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.row_factory = sqlite3.Row
        self.db.executescript(DDL)
        self.db.execute("INSERT INTO native_types(id,type_hash,class_name,owner_scope,on_pc,on_ps2,updated_utc) VALUES(1,1,'spTest','engine',1,0,'test')")
        for scope, numerator, denominator in (('all', 10., 100.), ('engine', 8., 80.),
                                              ('game', 2., 20.), ('smo_san', 1., 10.)):
            self.db.execute("INSERT INTO native_coverage_snapshots(scope_key,numerator,denominator,coverage_percent,lower_bound,upper_bound,calculation_method,notes,created_utc) VALUES(?,?,?,?,?,?,?,?,?)",
                            (scope, numerator, denominator, 10., 8., 12., 'baseline', '', 'test'))

    def tearDown(self):
        self.db.close()

    def manifest(self, score=80, accounting=None, reason=None, manifest_id='test1'):
        record = dict(className='spTest', researchStatus='substantial', pcStatus='substantial',
                      ps2Status='deferred', coverageScore=score, lowerBound=70, upperBound=90,
                      priorityTier=1, summary='test')
        if accounting is not None:
            record['coverageAccounting'] = accounting
        if reason is not None:
            record['baselineBackfillReason'] = reason
        return Manifest(dict(manifestId=manifest_id, createdUtc=manifest_id, mode='incremental', classes=[record]))

    def numerator(self, scope='all'):
        return self.db.execute('SELECT numerator FROM latest_native_coverage WHERE scope_key=?', (scope,)).fetchone()[0]

    def test_new_unstudied_class_counts_normally(self):
        self.assertEqual(apply_manifest(self.db, self.manifest()), (1, True))
        self.assertAlmostEqual(self.numerator(), 10.8)
        self.assertAlmostEqual(self.numerator('engine'), 8.8)
        self.assertEqual(self.numerator('game'), 2.)
        self.assertEqual(self.numerator('smo_san'), 1.)

    def test_backfill_does_not_recount_old_baseline(self):
        data = self.manifest(accounting='baseline_backfill', reason='Existing pre-baseline card, already in aggregate audit.')
        self.assertEqual(apply_manifest(self.db, data), (1, True))
        self.assertEqual(self.numerator(), 10.)
        self.assertEqual(self.db.execute('SELECT coverage_score FROM native_research_progress').fetchone()[0], 80.)
        self.assertEqual(apply_manifest(self.db, data), (0, False))
        self.assertEqual(self.db.execute('PRAGMA foreign_key_check').fetchall(), [])

    def test_next_research_counts_only_delta(self):
        apply_manifest(self.db, self.manifest(accounting='baseline_backfill', reason='Existing audited card.'))
        apply_manifest(self.db, self.manifest(score=85, manifest_id='test2'))
        self.assertAlmostEqual(self.numerator(), 10.05)

    def test_backfill_cannot_hide_existing_progress_change(self):
        apply_manifest(self.db, self.manifest())
        with self.assertRaisesRegex(RuntimeError, 'missing progress row'):
            apply_manifest(self.db, self.manifest(accounting='baseline_backfill', reason='Not allowed.', manifest_id='test2'))

    def test_backfill_requires_audit_reason(self):
        with self.assertRaisesRegex(RuntimeError, 'explicit reason'):
            apply_manifest(self.db, self.manifest(accounting='baseline_backfill'))

    def test_unknown_policy_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Unknown coverage accounting'):
            apply_manifest(self.db, self.manifest(accounting='guess'))


if __name__ == '__main__':
    unittest.main()

"""Read-only dossier, platform separation and bounded runner regressions."""
import copy
import hashlib
from datetime import datetime, timezone
import json
import sqlite3
import subprocess
from pathlib import Path
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from native_knowledge import DDL
from native_workbench import WORK_ITEMS, dossier, queue, run_profile, validate_config


class WorkbenchTests(unittest.TestCase):
    def setUp(self):
        self.config = json.loads(WORK_ITEMS.read_text(encoding='utf-8'))
        self.db = sqlite3.connect(':memory:')
        self.db.row_factory = sqlite3.Row
        self.db.executescript(DDL)
        self.db.execute("INSERT INTO native_types(id,type_hash,class_name,owner_scope,on_pc,on_ps2,updated_utc) VALUES(1,1,'spOctreeNode','engine',1,0,'fixture')")
        self.db.commit()

    def tearDown(self):
        self.db.close()

    def test_real_configuration(self):
        self.assertIs(validate_config(self.config), self.config)

    def test_cycle_rejected(self):
        self.config['items'][0]['planningStatus'] = 'active'
        self.config['items'][1]['planningStatus'] = 'active'
        self.config['items'][0]['dependsOn'] = [self.config['items'][1]['id']]
        self.config['items'][1]['dependsOn'] = [self.config['items'][0]['id']]
        with self.assertRaisesRegex(ValueError, 'cycle'):
            validate_config(self.config)

    def test_cross_platform_dependency_rejected(self):
        self.config['items'][0]['platform'] = 'ps2'
        with self.assertRaisesRegex(ValueError, '[Cc]ross-platform'):
            validate_config(self.config)

    def test_outside_script_rejected(self):
        self.config['testProfiles']['spatial-static'][0]['script'] = 'README.md'
        with self.assertRaisesRegex(ValueError, 'research Python'):
            validate_config(self.config)

    def test_dossier_does_not_write_or_infer_score(self):
        before = self.db.total_changes
        result = dossier(self.db, self.config, 'spOctreeNode', 'pc')
        self.assertIsNone(result['platformAssessment'])
        self.assertEqual(result['scopeCountMode'], 'shared_project_inventory_not_platform_filtered')
        self.assertEqual(len(result['workItems']), 2)
        self.assertEqual(self.db.total_changes, before)
        with self.assertRaisesRegex(ValueError, 'absent'):
            dossier(self.db, self.config, 'spOctreeNode', 'ps2')

    def test_ps2_queue_not_copied(self):
        ps2 = queue(self.db, self.config, 'ps2')
        self.assertEqual({item['id'] for item in ps2},
                         {item['id'] for item in self.config['items']
                          if item['platform'] == 'ps2' and item.get('planningStatus', 'active') == 'active'})
        self.assertTrue(all(item['platform'] == 'ps2' for item in ps2))
        pc = queue(self.db, self.config, 'pc')
        self.assertTrue(pc)
        self.assertEqual({item['id'] for item in pc},
                         {item['id'] for item in self.config['items']
                          if item['platform'] == 'pc' and item.get('planningStatus', 'active') == 'active'})
        self.assertTrue(all(item['platform'] == 'pc' for item in pc))

    def test_deferred_work_retained_but_not_scheduled(self):
        active = queue(self.db, self.config, 'pc')
        complete = queue(self.db, self.config, 'pc', include_deferred=True)
        active_ids = {item['id'] for item in active}
        self.assertNotIn('pc-shader-template-generation', active_ids)
        self.assertIn('pc-texture-codec-backend', active_ids)
        self.assertEqual({item['id'] for item in complete},
                         {item['id'] for item in self.config['items'] if item['platform'] == 'pc'})
        self.assertGreater(len(complete), len(active))

    def test_active_dependency_cannot_disappear_into_deferred_backlog(self):
        items = {item['id']: item for item in self.config['items']}
        items['pc-node-resource-graph']['dependsOn'] = ['pc-smo-san-loader-save']
        items['pc-smo-san-loader-save']['planningStatus'] = 'deferred'
        with self.assertRaisesRegex(ValueError, 'depends on deferred'):
            validate_config(self.config)

    def test_queue_respects_dependencies_before_priority(self):
        items={item['id']:item for item in self.config['items']}
        items['pc-mesh-native-submission']['priority']=-10
        ordered=queue(self.db,self.config,'pc',include_deferred=True);position={item['id']:i for i,item in enumerate(ordered)}
        for item in ordered:
            for dependency in item['dependsOn']:
                self.assertLess(position[dependency],position[item['id']])
        self.assertLess(position['pc-dx-mesh-materialization'],position['pc-visibility-plane-storage'])

    @patch('native_workbench.subprocess.run')
    def test_deadline_stops_before_start(self, run):
        self.assertEqual(run_profile(self.config, 'spatial-static', datetime(2020, 1, 1, tzinfo=timezone.utc)), 3)
        run.assert_not_called()

    @patch('native_workbench.subprocess.run')
    def test_fixed_cap_and_stop_on_failure(self, run):
        run.return_value.returncode = 2
        self.assertEqual(run_profile(self.config, 'spatial-static'), 2)
        self.assertEqual(run.call_count, 1)
        self.assertEqual(run.call_args.kwargs['timeout'], 30)
        self.assertNotIn('shell', run.call_args.kwargs)

    @patch('native_workbench.subprocess.run', side_effect=subprocess.TimeoutExpired('fixture', 30))
    def test_timeout_is_failure(self, run):
        self.assertEqual(run_profile(self.config, 'spatial-static'), 124)
        self.assertEqual(run.call_count, 1)

    @patch('native_workbench.subprocess.run')
    def test_persisted_report_counts_results_not_coverage(self, run):
        run.return_value.returncode=0
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'owned-generated-report.json'
            self.assertEqual(run_profile(self.config,'spatial-static',report_path=path),0)
            report=json.loads(path.read_text(encoding='utf-8'))
        self.assertEqual(report['status'],'passed')
        self.assertEqual(report['completedChildren'],report['expectedChildren'])
        self.assertTrue(all(len(row['scriptSha256'])==64 for row in report['results']))
        self.assertIn('NOT class coverage',report['notes'])
        self.assertEqual(report['schemaVersion'],2)
        self.assertEqual(report['configurationSha256'],hashlib.sha256(json.dumps(self.config,sort_keys=True,separators=(',',':')).encode('utf-8')).hexdigest())

    @patch('native_workbench.subprocess.run')
    def test_deadline_report_has_no_completed_children(self, run):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'owned-generated-report.json'
            self.assertEqual(run_profile(self.config,'spatial-static',datetime(2020,1,1,tzinfo=timezone.utc),path),3)
            report=json.loads(path.read_text(encoding='utf-8'))
        self.assertEqual(report['status'],'deadline');self.assertEqual(report['completedChildren'],0)
        run.assert_not_called()

    @patch('native_workbench.subprocess.run',side_effect=subprocess.TimeoutExpired('fixture',30))
    def test_failure_report_stops_after_timeout(self, run):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'owned-generated-report.json'
            self.assertEqual(run_profile(self.config,'spatial-static',report_path=path),124)
            report=json.loads(path.read_text(encoding='utf-8'))
        self.assertEqual(report['status'],'failed');self.assertEqual(report['completedChildren'],1)
        self.assertEqual(report['results'][0]['exitCode'],124);self.assertEqual(run.call_count,1)

    @patch('native_workbench.subprocess.run')
    def test_parallel_requires_declared_independence(self,run):
        with self.assertRaisesRegex(ValueError,'parallelSafe'):
            run_profile(self.config,'spatial-static',workers=2)
        for workers in (0,5,True):
            with self.assertRaisesRegex(ValueError,'worker count'):
                run_profile(self.config,'spatial-static',workers=workers)
        run.assert_not_called()

    @patch('native_workbench.subprocess.run')
    def test_parallel_failure_drains_wave_and_stops(self,run):
        tests=self.config['testProfiles']['spatial-static']
        for test in tests:test['parallelSafe']=True
        barrier=threading.Barrier(2,timeout=3)
        def execute(command,**kwargs):
            barrier.wait()
            self.assertEqual(kwargs['timeout'],30)
            return SimpleNamespace(returncode=7 if Path(command[2]).name==Path(tests[0]['script']).name else 0)
        run.side_effect=execute
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'parallel.json'
            self.assertEqual(run_profile(self.config,'spatial-static',report_path=path,workers=2),7)
            report=json.loads(path.read_text(encoding='utf-8'))
        self.assertEqual(run.call_count,2)
        self.assertEqual([r['id'] for r in report['results']],[t['id'] for t in tests[:2]])
        self.assertEqual(report['status'],'failed')
        self.assertEqual(report['workers'],2)
        self.assertFalse(report['memoryAdmissionIsOSLimit'])

    @patch('native_workbench.subprocess.run')
    def test_parallel_deadline_launches_no_children(self,run):
        for test in self.config['testProfiles']['spatial-static']:test['parallelSafe']=True
        self.assertEqual(run_profile(self.config,'spatial-static',datetime(2020,1,1,tzinfo=timezone.utc),workers=4),3)
        run.assert_not_called()


if __name__ == '__main__':
    unittest.main()

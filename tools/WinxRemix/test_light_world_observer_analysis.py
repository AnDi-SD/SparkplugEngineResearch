"""Golden origin/join and file-boundary diagnostics; no game, GPU or native instructions."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
import io
from contextlib import redirect_stdout
from unittest.mock import patch
import analyze_light_world_observer as analyzer
from analyze_light_world_observer import (matrix_packet, origin_classification,
                                         parse_log_bytes, summarize, SOURCES)

def bits(values):
    return list(struct.unpack('<' + 'I' * len(values), struct.pack('<' + 'f' * len(values), *values)))


def self_test():
    import copy
    # Known native row matrix: camera translated to world z=10, so world z=5
    # has view z=-5. An accidental column convention would fail this fixture.
    view = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., -10., 1.)
    projection = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 10 / 9, 1., 0., 0., -10 / 9, 0.)
    packet = {'event': 'camera', 'frame': 300, 'scene': 123, 'camera': 789,
              'native': False, 'accepted': False, 'result': 0,
              'viewBits': bits(view), 'projectionBits': bits(projection)}
    diag = {'event': 'frame', 'frame': 300, 'matched': 1, 'rejected': 0, 'failures': 0,
            'stateMismatches': 0, 'lateUpdates': 0, 'missedFirstDraws': 0}
    light = {'event': 'observation', 'frame': 300, 'scene': 123, 'light': 456,
             'status': 'Current', 'flags': 0, 'type': 1, 'enabled': 1,
             'worldPositionBits': bits((0., 0., 5.))}
    checks = 0
    def check(condition):
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError('Synthetic case ' + str(checks))
    def result(packets=None, lights=None):
        return summarize([packet, diag] if packets is None else packets, [light] if lights is None else lights)
    base = result()
    check(base['observedCurrentEnabledBehindOriginEvents'] == 1)
    check(base['examples'][0]['cameraPosition'] == (0., 0., -5., 1.))
    check(base['examples'][0]['clipPosition'][3] == -5.)
    check(base['examples'][0]['cameraNativeSubmit'] is False)
    for field in ('frame', 'scene'):
        changed = copy.deepcopy(light); changed[field] += 1
        check(result(lights=[changed])['qualifyingSampledFrameScenePairs'] == 0)
    check(result(packets=[packet])['qualifyingSampledFrameScenePairs'] == 0)
    check(result(packets=[diag])['qualifyingSampledFrameScenePairs'] == 0)
    second = copy.deepcopy(packet); second['camera'] += 1
    check(result(packets=[packet, second, diag])['qualifyingSampledFrameScenePairs'] == 0)
    check(result(packets=[packet, packet, diag])['qualifyingSampledFrameScenePairs'] == 0)
    for field in ('matched', 'rejected', 'failures', 'stateMismatches', 'lateUpdates', 'missedFirstDraws'):
        changed = copy.deepcopy(diag); changed[field] = 0 if field == 'matched' else 1
        check(result(packets=[packet, changed])['qualifyingSampledFrameScenePairs'] == 0)
    for bad in (0x7f800000, 0xff800000, 0x7fc00000):
        changed = copy.deepcopy(light); changed['worldPositionBits'][0] = bad
        check(result(lights=[changed])['qualifyingSampledFrameScenePairs'] == 0)
        changed = copy.deepcopy(packet); changed['viewBits'][0] = bad
        check(result(packets=[changed, diag])['qualifyingSampledFrameScenePairs'] == 0)
    for type_value in (0, 3):
        changed = copy.deepcopy(light); changed['type'] = type_value
        check(result(lights=[changed])['originClassifications'] == {'positional_origin_undefined': 1})
    for status in ('Changed', 'NotObserved', 'Unqualified'):
        changed = copy.deepcopy(light); changed['status'] = status
        value = result(lights=[changed])
        check(value['originClassifications'] == {'behind_camera_origin': 1})
        check(value['observedCurrentEnabledBehindOriginEvents'] == 0)
    changed = copy.deepcopy(light); changed['enabled'] = 0
    check(result(lights=[changed])['observedCurrentEnabledBehindOriginEvents'] == 0)
    changed = copy.deepcopy(light); changed['type'] = 2
    check(result(lights=[changed])['observedCurrentEnabledBehindOriginEvents'] == 1)
    view_values, projection_values = matrix_packet(packet)
    for pos, expected in (((0., 0., 10.), 'camera_plane_origin'),
                          ((0., 0., 11.), 'clip_boundary_origin'),
                          ((0., 0., 15.), 'inside_clip_volume_origin'),
                          ((9., 0., 15.), 'outside_clip_volume_origin'),
                          ((0., 0., 21.), 'outside_clip_volume_origin')):
        check(origin_classification(pos, view_values, projection_values)[0] == expected)
    changed = copy.deepcopy(packet); changed['projectionBits'][11] = bits((0.,))[0]
    check(result(packets=[changed, diag])['qualifyingSampledFrameScenePairs'] == 0)
    changed = copy.deepcopy(packet); changed['viewBits'][3] = bits((1.,))[0]
    check(result(packets=[changed, diag])['qualifyingSampledFrameScenePairs'] == 0)
    twice = copy.deepcopy(light); twice['frame'] = 600
    packet2 = copy.deepcopy(packet); packet2['frame'] = 600
    diag2 = copy.deepcopy(diag); diag2['frame'] = 600
    value = result(packets=[packet, diag, packet2, diag2], lights=[light, twice])
    check(value['observedCurrentEnabledBehindOriginEvents'] == 2)
    check(value['uniqueObservedCurrentEnabledBehindSceneAddressKeys'] == 1)
    check(value['observedCurrentEnabledBehindFrameScenePairs'] == 2)
    try:
        result(lights=[light, light])
    except ValueError:
        check(True)
    else:
        check(False)
    rows, tail = parse_log_bytes(b'{"event":"frame"}\n')
    check(len(rows) == 1 and not tail['incompleteFinalRecord'])
    rows, tail = parse_log_bytes(b'{"event":"frame"}\n{"event":"obser', True)
    check(len(rows) == 1 and tail['incompleteFinalRecord'] and tail['incompleteFinalRecordBytes'] == 15)
    check(tail['parsedCompleteBytes'] == len(b'{"event":"frame"}\n'))
    try:
        parse_log_bytes(b'{"event":"frame"}\n{"event":"obser', False)
    except ValueError:
        check(True)
    else:
        check(False)
    try:
        parse_log_bytes(b'{"event":"frame"}\nBAD COMPLETE LINE\n{"event":"obser', True)
    except ValueError:
        check(True)
    else:
        check(False)
    return {'status': 'PASS', 'syntheticChecks': checks, 'gameOrGpuRun': False, 'sources': SOURCES}


class ObserverAnalysisTests(unittest.TestCase):
    def setUp(self):
        artifact_root = analyzer.ROOT / 'artifacts/WinxRemix'
        self.temporary = tempfile.TemporaryDirectory(prefix='light-observer-analysis-', dir=artifact_root)
        self.directory = Path(self.temporary.name).resolve()
        if not self.directory.is_relative_to(artifact_root.resolve()):
            raise ValueError('Fixture cleanup target escaped artifact directory')
        self.addCleanup(self.temporary.cleanup)

    def write_fixture(self, pid=123):
        launch = dict(pid=pid, proxySha256='0' * 64)
        camera = [dict(event='init', schema=1, enabled=True, submit=False),
                  dict(event='camera', frame=300, scene=123, camera=789,
                       native=False, accepted=False, result=0,
                       viewBits=bits((1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)),
                       projectionBits=bits((1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 10 / 9, 1., 0., 0., -10 / 9, 0.))),
                  dict(event='frame', frame=300, matched=1, rejected=0, failures=0,
                       stateMismatches=0, lateUpdates=0, missedFirstDraws=0)]
        light = [dict(event='init', schema=1, enabled=True, installed=True),
                 dict(event='observation', frame=300, scene=123, light=456, status='Current',
                      flags=0, type=1, enabled=1, worldPositionBits=bits((0., 0., -5.)))]
        self.launch = self.directory / 'launch.json'
        self.camera = self.directory / 'native-camera-source.jsonl'
        self.light = self.directory / 'light-world-observer.jsonl'
        self.launch.write_text(json.dumps(launch), encoding='utf-8')
        for path, rows in ((self.camera, camera), (self.light, light)):
            path.write_text(''.join(json.dumps(row) + '\n' for row in rows), encoding='utf-8')
        return launch, camera, light

    def test_semantic_golden_fixtures(self):
        result = self_test()
        self.assertEqual(result['syntheticChecks'], 48)
        self.assertEqual(result['status'], 'PASS')

    def test_complete_final_record_without_newline_counts(self):
        rows, tail = parse_log_bytes(b'{"event":"frame"}\n{"event":"observation"}')
        self.assertEqual(len(rows), 2)
        self.assertFalse(tail['incompleteFinalRecord'])
        self.assertEqual(tail['parsedCompleteBytes'], len(b'{"event":"frame"}\n{"event":"observation"}'))

    def test_automatic_tail_guard_rejects_malformed_records(self):
        for raw in (b'{}', b'[]', b'1', b'null', b'{"event":1}', b'{"event":[]}',
                    b'{"event":"unknown"}', b'{"event":"UNKNOWN',
                    b'{"event":"observation","frame":!}',
                    b'{"event":"observation","frame":!',
                    b'{"event":"observation"} garbage',
                    b'{"event":"observation","status":"bad\\q',
                    b'{"event":"frame","event":"observation"}',
                    b'{"event":"observation",}',
                    b'{"event":"frame"}\nBAD\n{"event":"obser'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                parse_log_bytes(raw)
        with self.assertRaises(ValueError):
            parse_log_bytes(b'{"event":"observation","x":"' + b'a' * 65537)

    def test_recognized_prefixes_report_only_incomplete_final_row(self):
        for tail in (b'{"event":"obser', b'{"event":"observation",',
                     b'{"event":"call","frame":', b'{"event":"call","frame":1e',
                     b'{"event":"call","frame":-1.', b'{"event":"call","ok":tru',
                     b'{"event":"call","x":[1,', b'{"event":"call","x":"\\u12'):
            with self.subTest(tail=tail):
                rows, metadata = parse_log_bytes(b'{"event":"frame"}\n' + tail)
                self.assertEqual(len(rows), 1)
                self.assertTrue(metadata['incompleteFinalRecord'])
                self.assertEqual(metadata['incompleteFinalRecordBytes'], len(tail))

    def test_file_read_and_tail_metadata_preserve_source(self):
        self.write_fixture()
        raw = self.light.read_bytes() + b'{"event":"obser'
        self.light.write_bytes(raw)
        with patch.object(analyzer, 'closed') as process_check:
            result = analyzer.analyze(self.directory)
            self.assertEqual(process_check.call_count, 2)
        self.assertTrue(result['immutableRead'])
        self.assertEqual(result['observedCurrentEnabledBehindOriginEvents'], 1)
        self.assertEqual(self.light.read_bytes(), raw)
        self.assertTrue(result['sourceLogs'][1]['incompleteFinalRecord'])
        self.assertEqual(result['sourceLogs'][1]['bytes'], len(raw))
        with patch.object(analyzer, 'closed'), self.assertRaises(ValueError):
            analyzer.analyze(self.directory, allow_final_partial=False)

    def test_launch_and_log_mutations_after_final_exit_probe_rejected(self):
        for mutate_launch in (False, True):
            with self.subTest(mutate_launch=mutate_launch):
                self.write_fixture()
                checks = 0
                def mutate(pid):
                    nonlocal checks
                    checks += 1
                    if checks == 2:
                        with (self.launch if mutate_launch else self.light).open('a') as output:
                            output.write('\n')
                with patch.object(analyzer, 'closed', mutate), self.assertRaises(ValueError):
                    analyzer.analyze(self.directory)

    def test_identity_and_schema_input_types_rejected(self):
        for bad_pid in (True, 0, -1, 0x100000000, '123'):
            with self.subTest(bad_pid=bad_pid):
                launch, _, _ = self.write_fixture()
                launch['pid'] = bad_pid
                self.launch.write_text(json.dumps(launch), encoding='utf-8')
                with patch.object(analyzer, 'closed'), self.assertRaises(ValueError):
                    analyzer.analyze(self.directory)
        for field, value in (('schema', True), ('enabled', 1), ('installed', False)):
            with self.subTest(field=field, value=value):
                _, _, light = self.write_fixture()
                light[0][field] = value
                self.light.write_text(''.join(json.dumps(row) + '\n' for row in light), encoding='utf-8')
                with patch.object(analyzer, 'closed'), self.assertRaises(ValueError):
                    analyzer.analyze(self.directory)
        for field, value in (('frame', True), ('scene', 0), ('light', -1), ('flags', '0'), ('enabled', True)):
            with self.subTest(field=field, value=value):
                _, _, light = self.write_fixture()
                light[1][field] = value
                self.light.write_text(''.join(json.dumps(row) + '\n' for row in light), encoding='utf-8')
                with patch.object(analyzer, 'closed'), self.assertRaises(ValueError):
                    analyzer.analyze(self.directory)
        launch, _, _ = self.write_fixture()
        launch['gameProcessClosed'] = False
        self.launch.write_text(json.dumps(launch), encoding='utf-8')
        with patch.object(analyzer, 'closed'), self.assertRaises(ValueError):
            analyzer.analyze(self.directory)

    @unittest.skipUnless(os.name == 'nt', 'actual process check requires Windows')
    def test_actual_live_pid_cannot_be_waived_by_launch_metadata(self):
        launch, _, _ = self.write_fixture(pid=os.getpid())
        launch['gameProcessClosed'] = True
        self.launch.write_text(json.dumps(launch), encoding='utf-8')
        with self.assertRaises(ValueError):
            analyzer.analyze(self.directory)

    def test_read_budget_before_unbounded_allocation(self):
        target = self.directory / 'oversized.bin'
        target.write_bytes(b'x' * 17)
        with self.assertRaises(ValueError):
            analyzer.read_closed(target, 16)

    def test_exclusive_output_and_dry_run(self):
        target = self.directory / 'light-world-observer-analysis.json'
        with patch.object(analyzer, 'analyze', return_value={'schema': 1}), redirect_stdout(io.StringIO()):
            analyzer.main([str(self.directory), '--dry-run'])
            self.assertFalse(target.exists())
            analyzer.main([str(self.directory)])
            self.assertEqual(json.loads(target.read_text()), {'schema': 1})
            original = target.read_bytes()
            with self.assertRaises(FileExistsError):
                analyzer.main([str(self.directory)])
            self.assertEqual(target.read_bytes(), original)

    def test_optional_cached_sphere_bound_scaled_affine_depth_axis(self):
        row = dict(type=1, status='Current', enabled=1, cachedRangeBits=bits((1.,))[0])
        view = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 2., 0., 0., 0., 0., 1.)
        bound = analyzer.point_sphere_diagnostic(row, (0., 0., -1.5, 1.), view)
        self.assertEqual(bound['depthAxisLength'], 2.)
        self.assertEqual(bound['viewSpaceRadiusBound'], 2.)
        self.assertEqual(bound['status'], 'IntersectsCameraPlane')
        negative_axis = list(view); negative_axis[10] = -2.
        position = analyzer.row_times_matrix((0., 0., 1.5, 1.), negative_axis)
        bound = analyzer.point_sphere_diagnostic(row, position, negative_axis)
        self.assertEqual(bound['status'], 'WhollyBehindCameraPlane')
        self.assertEqual(bound['farthestCameraZ'], -1.)
        self.assertEqual(bound['nearestCameraZ'], -5.)
        self.assertEqual(analyzer.point_sphere_diagnostic(row, (0., 0., 3., 1.), view)['status'],
                         'WhollyInFrontOfCameraPlane')
        for center in (-2., 2., -2.000001):
            self.assertEqual(analyzer.point_sphere_diagnostic(row, (0., 0., center, 1.), view)['status'],
                             'CameraPlaneBoundary')

    def test_optional_range_never_turns_bad_missing_or_other_types_into_extent_proof(self):
        row = dict(type=1, status='Current', enabled=1)
        view = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)
        position = (0., 0., -500., 1.)
        self.assertEqual(analyzer.point_sphere_diagnostic(row, position, view)['status'], 'MissingCachedRange')
        for word in (True, -1, 0x100000000, '0', 0x7f800000, 0xff800000, 0x7fc00000):
            self.assertEqual(analyzer.point_sphere_diagnostic(dict(row, cachedRangeBits=word), position, view)['status'],
                             'InvalidCachedRange')
        for radius in (0., -0., -1.):
            self.assertEqual(analyzer.point_sphere_diagnostic(dict(row, cachedRangeBits=bits((radius,))[0]),
                                                             position, view)['status'], 'NonpositiveCachedRange')
        self.assertEqual(analyzer.point_sphere_diagnostic(dict(row, type=2, cachedRangeBits=bits((1.,))[0]),
                                                         position, view)['status'], 'UnsupportedSpotExtent')
        for status in ('Changed', 'NotObserved', 'Unqualified'):
            self.assertEqual(analyzer.point_sphere_diagnostic(dict(row, status=status, cachedRangeBits=bits((1.,))[0]),
                                                             position, view)['status'], 'UnqualifiedObserverStatus')
        self.assertEqual(analyzer.point_sphere_diagnostic(dict(row, enabled=0, cachedRangeBits=bits((1.,))[0]),
                                                         position, view)['status'], 'DisabledPoint')
        zero_axis = list(view); zero_axis[2] = zero_axis[6] = zero_axis[10] = 0.
        self.assertEqual(analyzer.point_sphere_diagnostic(dict(row, cachedRangeBits=bits((1.,))[0]),
                                                         position, zero_axis)['status'], 'UnqualifiedDepthAxis')

    def test_origin_accounting_survives_optional_range_and_extent_credit_has_own_counts(self):
        _, camera, light = self.write_fixture()
        old = summarize(camera, light)
        self.assertEqual(old['observedCurrentEnabledBehindOriginEvents'], 1)
        self.assertEqual(old['cachedPointSphereDiagnostics'], {'MissingCachedRange': 1})
        self.assertEqual(old['cachedPointSpheresWhollyBehindCameraPlaneEvents'], 0)
        light[1]['cachedRangeBits'] = bits((1.,))[0]
        new = summarize(camera, light)
        self.assertEqual(new['observedCurrentEnabledBehindOriginEvents'], 1)
        self.assertEqual(new['examples'], old['examples'])
        self.assertEqual(new['cachedPointSphereDiagnostics'], {'WhollyBehindCameraPlane': 1})
        self.assertEqual(new['cachedPointSpheresWhollyBehindCameraPlaneEvents'], 1)
        self.assertEqual(new['cachedPointSpheresWhollyBehindCameraPlaneFrameScenePairs'], 1)
        self.assertEqual(new['uniqueCachedPointSpheresWhollyBehindSceneAddressKeys'], 1)

    def test_malformed_unused_record_fields_do_not_silently_pass(self):
        for row in (dict(event='call', correlated=1), dict(event='batch', observed=True),
                    dict(event='frame', deviceEpoch=-1), dict(event='scene', snapshotValid=1),
                    dict(event='observation', status=1), dict(event='init', submit=1)):
            with self.subTest(row=row), self.assertRaises(ValueError):
                parse_log_bytes((json.dumps(row) + '\n').encode())


if __name__ == '__main__':
    unittest.main()

"""Own closed-log diagnostic: cached light origins relative to sampled main camera.

Original matrix/type facts are linked in SOURCES. This does not reproduce a
Light producer, visibility traversal, influence test, raster clipping or API
submission. Camera packets are sampled; never carry one across frames/scenes.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import re
from analyze_native_mesh_source import exited as closed


ROOT = Path(__file__).resolve().parents[4]
MAX_LOG = 16 * 1024 * 1024 + 65536
MAX_EXAMPLES = 5
SOURCES = {
    'types': 'docs/reference/classes/sp-light-data.md:83',
    'worldCachePosition': 'docs/engine/scene/node-world.md:25',
    'rowVectorAndTranslation': 'docs/engine/scene/node-world.md:27',
    'perspective': 'docs/reference/classes/sp-camera.md:53',
    'cameraSourceQualification': 'tools/WinxRemix/src/native/winx_native_camera_source.h:22',
    'cameraMatchingAndSampling': 'tools/WinxRemix/src/native/winx_native_camera_source.h:51',
    'lightObservation': 'tools/WinxRemix/src/native/winx_native_light_world_source.h:208',
}
TYPES = {0: 'directional', 1: 'point', 2: 'spot', 3: 'ambient'}
STATUSES = {'Current', 'Changed', 'NotObserved', 'Unqualified'}
CAMERA_EVENTS = {'init', 'camera', 'frame', 'first_window_missed'}
LIGHT_EVENTS = {'init', 'batch', 'call', 'observation', 'scene', 'frame'}
KNOWN_EVENTS = CAMERA_EVENTS | LIGHT_EVENTS


def floats(words, length):
    if not isinstance(words, list) or len(words) != length:
        raise ValueError('Wrong float-bit array length')
    if any(type(word) is not int or not 0 <= word <= 0xffffffff for word in words):
        raise ValueError('Invalid uint32 float word')
    result = struct.unpack('<' + 'f' * length, struct.pack('<' + 'I' * length, *words))
    if not all(math.isfinite(value) for value in result):
        raise ValueError('Nonfinite float input')
    return result


def uint(value, label, positive=False):
    if type(value) is not int or not (1 if positive else 0) <= value <= 0xffffffff:
        raise ValueError('Invalid uint32 ' + label)
    return value


def row_times_matrix(vector, matrix):
    return tuple(sum(vector[i] * matrix[i * 4 + j] for i in range(4)) for j in range(4))


def matrix_packet(packet):
    view = floats(packet['viewBits'], 16)
    projection = floats(packet['projectionBits'], 16)
    if tuple(view[i] for i in (3, 7, 11, 15)) != (0.0, 0.0, 0.0, 1.0):
        raise ValueError('View is not the observed affine row-matrix form')
    # Native ordinary perspective has clip.w = view.z. Reject other branches,
    # off-axis forms and invalid near/far input; no projection interpretation
    # is extrapolated from _34 alone.
    zero_indices = (1, 2, 3, 4, 6, 7, 8, 9, 12, 13)
    if any(projection[i] != 0.0 for i in zero_indices) or \
            projection[11] != 1.0 or projection[15] != 0.0 or \
            projection[0] <= 0.0 or projection[5] <= 0.0 or \
            projection[10] < 1.0 or projection[14] >= 0.0:
        raise ValueError('Projection is not ordinary original perspective')
    return view, projection


def origin_classification(position, view, projection):
    camera = row_times_matrix((*position, 1.0), view)
    clip = row_times_matrix(camera, projection)
    if not all(math.isfinite(value) for value in (*camera, *clip)):
        raise ValueError('Nonfinite transformed position')
    # Python uses double intermediates, not original x87/device math. This
    # explicit diagnostic tolerance keeps near-plane/equality cases separate.
    tolerance = 1e-5 * max(1.0, *(abs(value) for value in clip))
    if clip[3] < -tolerance:
        location = 'behind_camera_origin'
    elif clip[3] <= tolerance:
        location = 'camera_plane_origin'
    else:
        margins = (clip[3] - clip[0], clip[3] + clip[0],
                   clip[3] - clip[1], clip[3] + clip[1], clip[2], clip[3] - clip[2])
        if min(margins) < -tolerance:
            location = 'outside_clip_volume_origin'
        elif min(margins) <= tolerance:
            location = 'clip_boundary_origin'
        else:
            location = 'inside_clip_volume_origin'
    return location, camera, clip


def point_sphere_diagnostic(row, camera_position, view):
    """Own sphere bound from optional cached DX range, never an influence test."""
    if row['type'] != 1:
        return {'status': 'UnsupportedSpotExtent'}
    if row['status'] != 'Current':
        return {'status': 'UnqualifiedObserverStatus'}
    if not row['enabled']:
        return {'status': 'DisabledPoint'}
    if 'cachedRangeBits' not in row:
        return {'status': 'MissingCachedRange'}
    try:
        radius = floats([row['cachedRangeBits']], 1)[0]
    except ValueError:
        return {'status': 'InvalidCachedRange'}
    if radius <= 0.0:
        return {'status': 'NonpositiveCachedRange'}
    depth_axis = math.hypot(view[2], view[6], view[10])
    if not math.isfinite(depth_axis) or depth_axis <= 0.0:
        return {'status': 'UnqualifiedDepthAxis'}
    center = camera_position[2]
    extent = radius * depth_axis
    farthest, nearest = center + extent, center - extent
    tolerance = 1e-5 * max(1.0, abs(center), abs(extent))
    if farthest < -tolerance:
        status = 'WhollyBehindCameraPlane'
    elif nearest > tolerance:
        status = 'WhollyInFrontOfCameraPlane'
    elif nearest < -tolerance and farthest > tolerance:
        status = 'IntersectsCameraPlane'
    else:
        status = 'CameraPlaneBoundary'
    return {'status': status, 'cachedRadius': radius, 'depthAxisLength': depth_axis,
            'cameraCenterZ': center, 'viewSpaceRadiusBound': extent,
            'farthestCameraZ': farthest, 'nearestCameraZ': nearest,
            'diagnosticTolerance': tolerance}


def cameras(rows):
    packets, diagnostics = {}, {}
    for row in rows:
        if row.get('event') == 'camera':
            uint(row.get('frame'), 'camera frame')
            uint(row.get('scene'), 'camera scene', positive=True)
            uint(row.get('camera'), 'camera address', positive=True)
            if type(row.get('native')) is not bool or type(row.get('result')) is not int:
                raise ValueError('Invalid camera result/native marker')
            packets.setdefault((row['frame'], row['scene']), []).append(row)
        elif row.get('event') == 'frame':
            uint(row.get('frame'), 'camera diagnostic frame')
            diagnostics.setdefault(row['frame'], []).append(row)
    return packets, diagnostics


def qualify_camera(key, packets, diagnostics):
    choices = packets.get(key, [])
    if not choices:
        return None, 'missing_same_frame_scene_camera'
    if len(choices) != 1:
        return None, 'multiple_camera_packets'
    frames = diagnostics.get(key[0], [])
    if len(frames) != 1:
        return None, 'missing_or_multiple_camera_frame_diagnostics'
    frame = frames[0]
    required = ('matched', 'rejected', 'failures', 'stateMismatches', 'lateUpdates', 'missedFirstDraws')
    if any(type(frame.get(field)) is not int or not 0 <= frame[field] <= 0xffffffff for field in required):
        return None, 'incomplete_camera_frame_diagnostics'
    if frame['matched'] != 1 or any(frame[field] != 0 for field in required[1:]):
        return None, 'camera_frame_not_stable_matched'
    packet = choices[0]
    if not packet.get('camera') or packet.get('result') != 0:
        return None, 'invalid_camera_packet'
    try:
        return (packet, *matrix_packet(packet)), None
    except (KeyError, ValueError) as error:
        return None, 'invalid_camera_matrices: ' + str(error)


def summarize(camera_rows, light_rows):
    packets, diagnostics = cameras(camera_rows)
    classification, status_counts, reasons, type_counts = (Counter() for _ in range(4))
    joined_frames, current_frames, behind_frames = set(), set(), set()
    observed_ids, joined_ids, current_ids, behind_ids = set(), set(), set(), set()
    seen, examples, observations = set(), [], 0
    current_enabled_behind_events = 0
    range_counts = Counter()
    range_first_examples, range_behind_examples = [], []
    sphere_events, sphere_frames, sphere_ids = 0, set(), set()
    for row in light_rows:
        if row.get('event') != 'observation':
            continue
        observations += 1
        for field in ('frame', 'scene', 'light', 'flags', 'type'):
            uint(row.get(field), 'observation ' + field, positive=field in ('scene', 'light'))
        if type(row.get('enabled')) is not int or not 0 <= row['enabled'] <= 255:
            raise ValueError('Invalid raw enabled byte')
        key = row['frame'], row['scene']
        identity = row['scene'], row['light']
        full_key = (*key, row['light'])
        if full_key in seen:
            raise ValueError('Duplicate same-frame scene/light observation')
        seen.add(full_key)
        observed_ids.add(identity)
        status = row['status']
        if status not in STATUSES:
            raise ValueError('Unknown light observation status')
        status_counts[status] += 1
        light_type = TYPES.get(row['type'], 'unknown')
        type_counts[light_type] += 1
        if light_type in ('directional', 'ambient'):
            classification['positional_origin_undefined'] += 1
            continue
        if light_type == 'unknown':
            reasons['unknown_light_type'] += 1
            classification['Unqualified'] += 1
            continue
        camera, reason = qualify_camera(key, packets, diagnostics)
        if reason:
            reasons[reason] += 1
            classification['Unqualified'] += 1
            continue
        packet, view, projection = camera
        try:
            position = floats(row['worldPositionBits'], 3)
            location, camera_position, clip = origin_classification(position, view, projection)
        except (KeyError, ValueError) as error:
            reasons['invalid_world_cache_position: ' + str(error)] += 1
            classification['Unqualified'] += 1
            continue
        joined_frames.add(key)
        joined_ids.add(identity)
        classification[location] += 1
        if status == 'Current':
            current_frames.add(key)
            current_ids.add(identity)
        credited = status == 'Current' and row['enabled'] != 0 and location == 'behind_camera_origin'
        if credited:
            current_enabled_behind_events += 1
            behind_frames.add(key)
            behind_ids.add(identity)
        range_result = point_sphere_diagnostic(row, camera_position, view)
        range_counts[range_result['status']] += 1
        range_example = {'frame': key[0], 'scene': key[1], 'light': identity[1],
                         'type': light_type, 'observerStatus': status,
                         'enabledRaw': row['enabled'], 'originClassification': location,
                         **range_result}
        if len(range_first_examples) < MAX_EXAMPLES:
            range_first_examples.append(range_example)
        if range_result['status'] == 'WhollyBehindCameraPlane':
            sphere_events += 1
            sphere_frames.add(key)
            sphere_ids.add(identity)
            if len(range_behind_examples) < MAX_EXAMPLES:
                range_behind_examples.append(range_example)
        if len(examples) < MAX_EXAMPLES and (credited or status == 'Current'):
            examples.append({'frame': key[0], 'scene': key[1], 'light': identity[1],
                             'type': light_type, 'enabledRaw': row['enabled'],
                             'observerStatus': status, 'flags': row['flags'],
                             'worldCachePosition': position, 'camera': packet['camera'],
                             'cameraNativeSubmit': packet['native'],
                             'cameraPosition': camera_position, 'clipPosition': clip,
                             'originClassification': location,
                             'observedCurrentEnabledOriginBehind': credited})
    return {'schema': 1, 'observations': observations,
            'observerStatuses': dict(status_counts), 'lightTypes': dict(type_counts),
            'originClassifications': dict(classification), 'unqualifiedReasons': dict(reasons),
            'uniqueObservedSceneAddressKeys': len(observed_ids),
            'qualifyingSampledFrameScenePairs': len(joined_frames),
            'uniqueJoinedSceneAddressKeys': len(joined_ids),
            'currentJoinedFrameScenePairs': len(current_frames),
            'uniqueCurrentJoinedSceneAddressKeys': len(current_ids),
            'observedCurrentEnabledBehindOriginEvents': current_enabled_behind_events,
            'observedCurrentEnabledBehindFrameScenePairs': len(behind_frames),
            'uniqueObservedCurrentEnabledBehindSceneAddressKeys': len(behind_ids),
            'cachedPointSphereDiagnostics': dict(range_counts),
            'cachedPointSpheresWhollyBehindCameraPlaneEvents': sphere_events,
            'cachedPointSpheresWhollyBehindCameraPlaneFrameScenePairs': len(sphere_frames),
            'uniqueCachedPointSpheresWhollyBehindSceneAddressKeys': len(sphere_ids),
            'rangeExamples': range_behind_examples or range_first_examples,
            'rangeSource': 'docs/reference/classes/sp-dx-light.md:21; cached DX range +0x13C',
            'rangeLimits': [
                'Optional cachedRangeBits is an observed cached DX word, never a recomputed or freshness-qualified radius.',
                'Only same-frame camera-joined Current enabled point with finite positive cached range receives a sphere bound.',
                'Missing/invalid/nonpositive range retains origin accounting but receives no whole-sphere credit.',
                'Spot, directional and ambient extent are not inferred from a point sphere.',
                'Farthest camera z = center z + radius * length(view depth column [2,6,10]); scaled affine views are supported.',
                'Wholly behind the camera plane describes this diagnostic cached sphere, never receiver visibility, influence, shadows or RT shape.',
                'Relative 1e-5 tolerance separates tangency/boundary; original x87/GPU arithmetic equivalence is not claimed.',
            ],
            'examples': examples,
            'limits': [
                'Cached point/spot origins relative to sampled matching original main-camera matrices only.',
                'Only Current credits observed-current origin; Changed/NotObserved/Unqualified keep independent observer status.',
                'Current means unchanged completed producer observation, never FreshRt or complete producer coverage.',
                'Behind/outside origin does not establish receiver visibility, light influence, shadow need or missing native updates.',
                'Camera native=false is original matrix observation without Remix API submit; accepted=false is not rejection of this diagnostic.',
                'Packets are sampled (normally frame%300/trace boundary); missing same-frame scene camera is Unqualified, never interpolated.',
                'Scene/address keys are observed pointer keys, not persistent identities, instances or an exact distinct-object count.',
                'Double diagnostic math with relative 1e-5 tolerance; no claim of original x87/GPU rounding at boundaries.',
            ], 'sources': SOURCES}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON object key: ' + key)
        result[key] = value
    return result


def validate_record_types(row):
    unsigned32 = ('frame', 'scene', 'light', 'flags', 'type', 'manager', 'observed',
                  'beforeDraw', 'applyAfterDraw', 'beforeFlags', 'afterFlags', 'inheritedFlags',
                  'payloadDifferenceMask', 'calls', 'correlated', 'rejected', 'batches', 'sealed',
                  'loaded', 'current', 'changed', 'notObserved', 'unqualified', 'worldDirty', 'lightDirty',
                  'applies', 'foreignApplies', 'matched', 'submitted', 'failures', 'stateMismatches',
                  'lateUpdates', 'missedFirstDraws', 'schema', 'capacity', 'maxLogBytes')
    for field in unsigned32:
        if field in row and not (field == 'correlated' and row['event'] == 'call'):
            uint(row[field], field)
    for field in ('sequence', 'observerEpoch', 'deviceEpoch'):
        if field in row and (type(row[field]) is not int or not 0 <= row[field] <= 0xffffffffffffffff):
            raise ValueError('Invalid uint64 ' + field)
    for field in ('installed', 'startupIdentityVerified', 'imageVerified', 'submit', 'native', 'accepted',
                  'published', 'snapshotValid', 'allocationFailure'):
        if field in row and type(row[field]) is not bool:
            raise ValueError('Invalid Boolean ' + field)
    if row['event'] == 'init' and 'enabled' in row and type(row['enabled']) is not bool:
        raise ValueError('Invalid initialization enabled marker')
    if row['event'] == 'call' and type(row.get('correlated')) is not bool:
        raise ValueError('Invalid call correlated marker')
    for field in ('source', 'scope', 'transport', 'status', 'reason'):
        if field in row and type(row[field]) is not str:
            raise ValueError('Invalid text field ' + field)


def incomplete_json_prefix(text):
    """Recognize valid incomplete syntax with a known leading event marker.

    This guard never repairs a record or evaluates partial values. Only the
    native writer's event-first object form may be excluded from accounting.
    """
    marker = re.match(r'^\s*\{\s*"event"\s*:\s*"([^"\\]*)', text)
    if not marker or not marker[1]:
        return False
    event_prefix = marker[1]
    event_closed = marker.end() < len(text) and text[marker.end()] == '"'
    if (event_closed and event_prefix not in KNOWN_EVENTS) or \
            (not event_closed and not any(event.startswith(event_prefix) for event in KNOWN_EVENTS)):
        return False

    class Incomplete(Exception):
        pass

    pos = 0

    def space():
        nonlocal pos
        while pos < len(text) and text[pos] in ' \t\r\n':
            pos += 1

    def char(expected):
        nonlocal pos
        space()
        if pos == len(text):
            raise Incomplete()
        if text[pos] != expected:
            raise ValueError('Malformed JSON prefix')
        pos += 1

    def string():
        nonlocal pos
        char('"')
        while pos < len(text):
            value = text[pos]; pos += 1
            if value == '"':
                return
            if ord(value) < 0x20:
                raise ValueError('Control character in JSON string')
            if value == '\\':
                if pos == len(text):
                    raise Incomplete()
                escape = text[pos]; pos += 1
                if escape == 'u':
                    for _ in range(4):
                        if pos == len(text):
                            raise Incomplete()
                        if text[pos] not in '0123456789abcdefABCDEF':
                            raise ValueError('Invalid Unicode escape')
                        pos += 1
                elif escape not in '"\\/bfnrt':
                    raise ValueError('Invalid JSON escape')
        raise Incomplete()

    def number():
        nonlocal pos
        if text[pos] == '-':
            pos += 1
            if pos == len(text):
                raise Incomplete()
        if text[pos] == '0':
            pos += 1
        elif text[pos] in '123456789':
            while pos < len(text) and text[pos] in '0123456789':
                pos += 1
        else:
            raise ValueError('Invalid JSON number')
        if pos < len(text) and text[pos] == '.':
            pos += 1
            if pos == len(text):
                raise Incomplete()
            if text[pos] not in '0123456789':
                raise ValueError('Invalid JSON fraction')
            while pos < len(text) and text[pos] in '0123456789':
                pos += 1
        if pos < len(text) and text[pos] in 'eE':
            pos += 1
            if pos < len(text) and text[pos] in '+-':
                pos += 1
            if pos == len(text):
                raise Incomplete()
            if text[pos] not in '0123456789':
                raise ValueError('Invalid JSON exponent')
            while pos < len(text) and text[pos] in '0123456789':
                pos += 1

    def value(depth=0):
        nonlocal pos
        if depth > 64:
            raise ValueError('JSON prefix nesting exceeds budget')
        space()
        if pos == len(text):
            raise Incomplete()
        current = text[pos]
        if current == '"':
            string()
        elif current == '{':
            pos += 1; space()
            if pos == len(text):
                raise Incomplete()
            if text[pos] == '}':
                pos += 1; return
            while True:
                string(); char(':'); value(depth + 1); space()
                if pos == len(text):
                    raise Incomplete()
                if text[pos] == '}':
                    pos += 1; return
                char(',')
        elif current == '[':
            pos += 1; space()
            if pos == len(text):
                raise Incomplete()
            if text[pos] == ']':
                pos += 1; return
            while True:
                value(depth + 1); space()
                if pos == len(text):
                    raise Incomplete()
                if text[pos] == ']':
                    pos += 1; return
                char(',')
        elif current in '-0123456789':
            number()
        else:
            word = next((word for word in ('true', 'false', 'null') if word.startswith(current)), None)
            if word is None:
                raise ValueError('Unknown JSON value')
            remaining = text[pos:]
            if len(remaining) < len(word):
                if word.startswith(remaining):
                    raise Incomplete()
                raise ValueError('Malformed JSON literal')
            if not remaining.startswith(word):
                raise ValueError('Malformed JSON literal')
            pos += len(word)

    try:
        value()
        space()
        return False
    except Incomplete:
        return True
    except ValueError:
        return False


def parse_log_bytes(raw, allow_final_partial=True):
    partial = b''
    complete = raw
    if raw and not raw.endswith(b'\n'):
        boundary = raw.rfind(b'\n') + 1
        final = raw[boundary:]
        if len(final) > 65536:
            raise ValueError('Incomplete final record exceeds tail budget')
        text = final.decode('utf-8-sig')
        try:
            json.loads(text, object_pairs_hook=unique_object)
        except json.JSONDecodeError:
            if not allow_final_partial or not incomplete_json_prefix(text):
                raise ValueError('Malformed or unrecognized final record')
            complete, partial = raw[:boundary], final
    rows = [json.loads(line, object_pairs_hook=unique_object) for line in complete.decode('utf-8-sig').splitlines()]
    if any(type(row) is not dict for row in rows):
        raise ValueError('Observer record must be a JSON object')
    if any(type(row.get('event')) is not str or row['event'] not in KNOWN_EVENTS for row in rows):
        raise ValueError('Unknown observer event')
    for row in rows:
        validate_record_types(row)
    return rows, {'parsedCompleteBytes': len(complete),
                  'incompleteFinalRecord': bool(partial),
                  'incompleteFinalRecordBytes': len(partial),
                  'incompleteFinalRecordHex': partial[:128].hex()}


def identity(stat):
    return stat.st_size, stat.st_mtime_ns, stat.st_dev, stat.st_ino


def read_closed(path, limit):
    before = path.stat()
    if before.st_size > limit:
        raise ValueError('Evidence exceeds diagnostic budget')
    raw = path.read_bytes()
    after = path.stat()
    if identity(before) != identity(after) or len(raw) != after.st_size:
        raise ValueError('Evidence changed during read')
    return raw, identity(after)


def load_log(path, allow_final_partial=True):
    raw, stamp = read_closed(path, MAX_LOG)
    rows, tail = parse_log_bytes(raw, allow_final_partial)
    return rows, {'path': str(path), 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(),
                  'logLimitReached': len(raw) >= 16 * 1024 * 1024,
                  'mtimeNs': stamp[1], 'fileIdentity': stamp, **tail}


def analyze(run, allow_final_partial=True):
    launch_path = run / 'launch.json'
    launch_raw, launch_stamp = read_closed(launch_path, 1024 * 1024)
    launch = json.loads(launch_raw.decode('utf-8-sig'), object_pairs_hook=unique_object)
    pid = uint(launch.get('pid'), 'launch PID', positive=True)
    if launch.get('gameProcessClosed') is False:
        raise ValueError('Launch explicitly records an unclosed game process')
    closed(pid)
    camera_rows, camera_source = load_log(run / 'native-camera-source.jsonl', allow_final_partial)
    light_rows, light_source = load_log(run / 'light-world-observer.jsonl', allow_final_partial)
    for rows, label, known in ((camera_rows, 'camera', CAMERA_EVENTS), (light_rows, 'light observer', LIGHT_EVENTS)):
        if any(row['event'] not in known for row in rows):
            raise ValueError('Wrong event kind in ' + label + ' log')
        init = [row for row in rows if row.get('event') == 'init']
        if len(init) != 1 or type(init[0].get('schema')) is not int or init[0]['schema'] != 1 or init[0].get('enabled') is not True:
            raise ValueError(label + ' not enabled with known schema')
    if next(row for row in light_rows if row.get('event') == 'init').get('installed') is not True:
        raise ValueError('Native light entry was not installed')
    result = summarize(camera_rows, light_rows)
    closed(pid)
    for source in (camera_source, light_source):
        if identity(Path(source['path']).stat()) != source['fileIdentity']:
            raise ValueError('Log changed during analysis')
    if identity(launch_path.stat()) != launch_stamp:
        raise ValueError('Launch changed during analysis')
    result.update(run=str(run), pidExited=True, immutableRead=True, proxySha256=launch['proxySha256'],
                  launchSha256=hashlib.sha256(launch_raw).hexdigest(),
                  sourceLogs=[camera_source, light_source])
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path,
                        help='New output file; default RUN/light-world-observer-analysis.json')
    parser.add_argument('--dry-run', action='store_true', help='Print diagnostic without writing output')
    parser.add_argument('--strict-final-record', action='store_true',
                        help='Reject even recognized incomplete final record; raw logs are always preserved')
    args = parser.parse_args(argv)
    result = analyze(args.run, not args.strict_final_record)
    if not args.dry_run:
        target = args.output or args.run / 'light-world-observer-analysis.json'
        with target.open('x', encoding='utf-8') as output:
            json.dump(result, output, ensure_ascii=False, indent=2)
            output.write('\n')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Select small pristine PC SMO variant witnesses from existing SQLite knowledge.

No directory/EXE rescan, no viewer/game. Verification is limited to fingerprint,
record identity and exact cached field payload hashes, NOT native loader/render.
PS2 selection/execution is deliberately deferred in this PC-first pilot.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import time

ROOT = Path(__file__).resolve().parents[2]
DATABASE = ROOT / 'local-data/results/smo-corpus-v2.sqlite'
MAX_FILE = 4 * 1024 * 1024
MAX_TOTAL = 32 * 1024 * 1024
DEFAULT_CLASSES = ('spMeshData', 'spModel', 'spSkin', 'spPartitionNode',
                   'spOctreeNode', 'spBSPNode', 'spZonePortal')


def select(db, names=DEFAULT_CLASSES):
    if len(names) > 8:
        raise ValueError('At most8 selected classes per bounded query')
    corpus = db.execute("SELECT id,source_root FROM corpora WHERE corpus_key='pc-pristine'").fetchone()
    if corpus is None:
        raise ValueError('PC pristine corpus not indexed')
    root = Path(corpus['source_root']).resolve()
    if not root.is_relative_to(ROOT / 'local-data'):
        raise ValueError('Indexed source root is outside local research data')
    result = dict(kind='pc-smo-variant-specimens', limits=dict(fileBytes=MAX_FILE),
                  corpus='pc-pristine', evidenceMode='cached_variant_witness_selection',
                  witnesses=[], absent=[], warnings=['Field/hash verification is not a native loader or in-game render test.'])
    for name in names:
        native = db.execute('SELECT type_hash FROM native_types WHERE class_name=? AND on_pc=1', (name,)).fetchone()
        if native is None:
            raise ValueError(f'Unknown PC class {name}')
        variants = db.execute("SELECT id,variant_key,status FROM class_variants WHERE type_hash=? AND status='confirmed' AND (scope_kind='common' OR scope_key='pc') ORDER BY id", (native[0],)).fetchall()
        for variant in variants:
            # Drive from the small selected corpus file index into object PK;
            # avoid scanning every other corpus object through type_hash first.
            # Existing file/object/assignment indexes only; capped VDBE
            # work and wall time also bound not-found searches. No DDL/index build.
            budget = [0]; start = time.monotonic()
            def stop():
                budget[0] += 1000
                return budget[0] > 2_000_000 or time.monotonic() - start > 2
            db.set_progress_handler(stop, 1000)
            try:
                row = db.execute('''SELECT f.id AS file_id,f.relative_path,f.sha256,f.byte_size,
                    o.object_index,o.physical_offset,o.serialized_size,o.type_hash,o.name
                    FROM files f INDEXED BY ix_files_corpus_path
                    CROSS JOIN objects o INDEXED BY sqlite_autoindex_objects_1
                    JOIN object_variant_assignments a ON a.file_id=o.file_id AND a.object_index=o.object_index
                    WHERE o.file_id=f.id AND o.type_hash=? AND f.corpus_id=? AND f.byte_size<=? AND a.variant_id=?
                    AND f.parse_status='ok' AND o.field_parse_error IS NULL
                    LIMIT 1''', (native[0], corpus['id'], MAX_FILE, variant['id'])).fetchone()
            except sqlite3.OperationalError as error:
                if 'interrupted' not in str(error):
                    raise
                result['absent'].append(dict(className=name, variant=variant['variant_key'], reason='bounded query cap; not proof of absence'))
                continue
            finally:
                db.set_progress_handler(None, 0)
            if row is None:
                result['absent'].append(dict(className=name, variant=variant['variant_key'], reason='no witness under selected PC pristine/file-size constraints'))
                continue
            path = (root / row['relative_path']).resolve()
            if not path.is_relative_to(root):
                raise ValueError('Unsafe indexed relative path')
            fields = [dict(field) for field in db.execute('''SELECT field_index,absolute_payload_offset,payload_size,payload_sha256,
                semantic_key FROM direct_fields WHERE file_id=? AND object_index=?
                AND payload_sha256 IS NOT NULL ORDER BY field_index LIMIT 32''', (row['file_id'], row['object_index']))]
            result['witnesses'].append(dict(className=name, variantId=variant['id'], variant=variant['variant_key'],
                sourcePath=path.relative_to(ROOT).as_posix(), fields=fields, fieldSampleLimit=32, **dict(row)))
    return result


def verify(manifest):
    if manifest.get('kind') != 'pc-smo-variant-specimens':
        raise ValueError('Expected PC specimen manifest')
    if len(manifest['witnesses']) > 128:
        raise ValueError('Bounded witness count exceeded')
    cache = {}; checks = 0
    for item in manifest['witnesses']:
        path = (ROOT / item['sourcePath']).resolve()
        if not path.is_relative_to(ROOT / 'local-data/pc-pristine/Media') or path.suffix.lower() != '.smo':
            raise ValueError('Only pristine PC SMO witnesses allowed')
        if path not in cache:
            if len(cache) >= 16:
                raise ValueError('At most16 witness files per verification')
            expected_size = path.stat().st_size
            if expected_size > MAX_FILE:
                raise ValueError('File cap exceeded')
            remaining = MAX_TOTAL - sum(map(len, cache.values()))
            if expected_size > remaining:
                raise ValueError('Aggregate witness memory cap exceeded')
            # Bound the actual read too: a file can grow after stat(). Read one
            # sentinel byte beyond the smaller budget, never an unbounded blob.
            read_limit = min(MAX_FILE, remaining)
            with path.open('rb') as stream:
                raw = stream.read(read_limit + 1)
            if len(raw) > MAX_FILE:
                raise ValueError('File cap exceeded during read')
            if len(raw) > remaining:
                raise ValueError('Aggregate witness memory cap exceeded during read')
            cache[path] = raw
        raw = cache[path]
        if len(raw) != item['byte_size'] or hashlib.sha256(raw).hexdigest().upper() != item['sha256'].upper():
            raise ValueError('Specimen changed since indexing')
        checks += 1
        at = item['physical_offset']
        if at < 0 or at + 8 > len(raw) or struct.unpack_from('<I', raw, at)[0] != item['type_hash']:
            raise ValueError('Cached object identity offset does not match pristine data')
        checks += 1
        if len(item['fields']) > 32:
            raise ValueError('Field count cap exceeded')
        for field in item['fields']:
            offset, size = field['absolute_payload_offset'], field['payload_size']
            if offset < 0 or size < 0 or offset + size > len(raw):
                raise ValueError('Cached field range outside file')
            if hashlib.sha256(raw[offset:offset + size]).hexdigest().upper() != field['payload_sha256'].upper():
                raise ValueError('Cached field payload mismatch')
            checks += 1
    print(f'PASS {checks}/{checks}: {len(manifest["witnesses"])} SMO variant witnesses in {len(cache)} pristine PC files; structural hashes only, not native loading')
    print(f'UNSELECTED {len(manifest["absent"])} variants under explicit constraints; not treated as tested')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('select', 'verify'))
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--database', type=Path, default=DATABASE)
    parser.add_argument('--class', dest='classes', action='append')
    args = parser.parse_args()
    if args.command == 'verify':
        if not args.manifest:
            parser.error('--manifest is required')
        return verify(json.loads(args.manifest.read_text(encoding='utf-8')))
    db = sqlite3.connect(args.database.resolve().as_uri() + '?mode=ro', uri=True)
    db.row_factory = sqlite3.Row
    try:
        print(json.dumps(select(db, args.classes or DEFAULT_CLASSES), ensure_ascii=False, indent=2))
    finally:
        db.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

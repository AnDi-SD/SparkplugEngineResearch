#!/usr/bin/env python3
"""Report registered classes with existing source definitions, without writing.

Source presence does not establish complete reconstruction or game integration.
The public registry does not contain a verified complete-class status.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / 'docs/reference/classes.json'
NOTICE = ('Source definitions do not establish complete reconstruction or '
          'runtime integration; no complete-class count is inferred.')
NONCODE = re.compile(
    r'''//[^\r\n]*|/\*.*?\*/|R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*' ''',
    re.S | re.X)


def has_definition(text: str, name: str) -> bool:
    # Ignore comments/literals: an example or a forward declaration is not a
    # source definition. This checks direct declarations used by this registry,
    # not arbitrary macro-generated C++ or whether methods are implemented.
    code = NONCODE.sub(lambda match: ' ' * len(match[0]), text)
    return re.search(r'\b(?:class|struct)\s+' + re.escape(name)
                     + r'\b[^;{}]*\{', code) is not None


def coverage(registry: Path) -> dict:
    data = json.loads(registry.read_text(encoding='utf-8-sig'))
    if not isinstance(data, dict) or not isinstance(data.get('classes'), list):
        raise ValueError('registry must contain a classes array')

    rows = data['classes']
    counts = {owner: {'total': 0, 'withSources': 0, 'sourceLinked': 0}
              for owner in ('engine', 'game')}
    errors = []
    names = set()
    contents = {}
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            raise ValueError(f'classes[{index}] must be an object')
        name, owner, sources = row.get('name'), row.get('owner'), row.get('sources')
        if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z_]\w*', name):
            raise ValueError(f'classes[{index}] has an invalid name')
        if owner not in counts:
            raise ValueError(f'{name}: owner must be engine or game')
        if not isinstance(sources, list) or any(not isinstance(p, str) or not p for p in sources):
            raise ValueError(f'{name}: sources must be an array of nonempty paths')
        if name in names:
            errors.append(f'duplicate class name: {name}')
        names.add(name)
        counts[owner]['total'] += 1
        if not sources:
            continue
        counts[owner]['sourceLinked'] += 1
        valid = True
        defined = False
        for source in sources:
            path = (ROOT / source).resolve()
            if not path.is_relative_to(ROOT):
                errors.append(f'{name}: source path leaves repository: {source}')
                valid = False
                continue
            if not path.is_file():
                errors.append(f'{name}: missing source file: {source}')
                valid = False
                continue
            if path not in contents:
                contents[path] = path.read_text(encoding='utf-8-sig')
            defined |= has_definition(contents[path], name)
        if valid and not defined:
            errors.append(f'{name}: linked sources contain no direct class/struct '
                          'definition (forward declarations do not count)')
        if valid and defined:
            counts[owner]['withSources'] += 1

    for count in counts.values():
        count['percent'] = round(100 * count['withSources'] / count['total'], 2) if count['total'] else 0.0
    total = len(rows)
    with_sources = sum(count['withSources'] for count in counts.values())
    return {
        'schemaVersion': 1,
        'metric': 'registered_classes_with_source_definitions',
        'notice': NOTICE,
        'total': total,
        'withSources': with_sources,
        'sourceLinked': sum(count['sourceLinked'] for count in counts.values()),
        'percent': round(100 * with_sources / total, 2) if total else 0.0,
        'byOwner': counts,
        'validation': {'passed': not errors, 'errors': errors},
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true', help='emit machine-readable counts and validation')
    parser.add_argument('--registry', type=Path, default=REGISTRY,
                        help='registry to read; source paths are relative to the repository root')
    args = parser.parse_args()
    try:
        report = coverage(args.registry)
    except (OSError, UnicodeError, ValueError) as error:
        if args.json:
            print(json.dumps({'schemaVersion': 1, 'validation': {'passed': False, 'errors': [str(error)]}}, ensure_ascii=False, indent=2))
        else:
            print(f'Class coverage failed: {error}')
        return 1
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"Классы с исходниками: {report['withSources']} из {report['total']} ({report['percent']:.2f}%).")
        for owner, label in (('engine', 'Движок'), ('game', 'Игра')):
            count = report['byOwner'][owner]
            print(f"{label}: {count['withSources']} из {count['total']} ({count['percent']:.2f}%).")
        print('Наличие определения класса не означает полного восстановления или интеграции с игрой.')
        for error in report['validation']['errors']:
            print(f'ERROR: {error}')
    return 0 if report['validation']['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

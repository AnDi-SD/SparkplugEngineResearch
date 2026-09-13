#!/usr/bin/env python3
"""Run a named workbench module with explicit public/private import roots."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
PUBLIC = ROOT / 'tools/ResearchWorkbench'
PRIVATE = ROOT / '.private/research'


def resolve_script(name: str) -> Path:
    candidate = Path(name)
    if candidate.is_absolute() or len(candidate.parts) != 1 or candidate.name in ('.', '..'):
        raise ValueError('Use a module filename, without directory components.')
    if not candidate.suffix:
        candidate = candidate.with_suffix('.py')
    if candidate.suffix != '.py':
        raise ValueError('Only Python modules are supported.')
    for folder in (PUBLIC, PRIVATE):
        path = (folder / candidate).resolve()
        if path.is_relative_to(folder.resolve()) and path.is_file() and path != Path(__file__).resolve():
            return path
    raise ValueError(f'Module {candidate.name} is not available in the public workbench or local private research directory.')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='List available public and local modules without importing them.')
    parser.add_argument('script', nargs='?')
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.list:
        for folder in (PUBLIC, PRIVATE):
            for path in sorted(folder.glob('*.py')):
                if path.name != 'run.py':
                    print(path.relative_to(ROOT).as_posix())
        return 0
    if not args.script:
        parser.error('Specify a module, or --list.')
    try:
        path = resolve_script(args.script)
    except ValueError as error:
        parser.error(str(error))
    environment = os.environ.copy()
    roots = [str(PUBLIC), str(PRIVATE)]
    if environment.get('PYTHONPATH'):
        roots.append(environment['PYTHONPATH'])
    environment['PYTHONPATH'] = os.pathsep.join(roots)
    environment['PYTHONDONTWRITEBYTECODE'] = '1'
    arguments = args.arguments[1:] if args.arguments[:1] == ['--'] else args.arguments
    return subprocess.run([sys.executable, '-B', str(path), *arguments], cwd=ROOT, env=environment).returncode


if __name__ == '__main__':
    raise SystemExit(main())

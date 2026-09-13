#!/usr/bin/env python3
"""Check public Markdown links, catalogs and class-registry references."""
from __future__ import annotations
import json
import re
import subprocess
import unicodedata
from pathlib import Path
from urllib.parse import unquote, urlsplit
import catalog

ROOT = Path(__file__).resolve().parents[2]
LINK = re.compile(r'!?\[([^\]\n]*)\]\(\s*(<[^>\n]+>|[^\s)]+)(?:\s+["\'][^\n]*?["\'])?\s*\)')
PRIVATE_PARTS = {'.private', 'local-data', '.codex-tmp', '.scratch', '.tmp'}


def public_files(root: Path) -> set[str]:
    # In a source archive there may be no .git; only traverse the public roots.
    if (root / '.git').exists():
        result = subprocess.run(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=root, check=True, capture_output=True)
        return {p for p in result.stdout.decode('utf-8').split('\0') if p and (root / p).is_file()}
    found = {p.name for p in root.iterdir() if p.is_file()}
    for name in ('docs', 'Sparkplug', 'Winx', 'tools', 'release'):
        found.update(p.relative_to(root).as_posix() for p in (root / name).rglob('*') if p.is_file() and not {'bin', 'obj', '__pycache__'}.intersection(p.parts))
    return found


def strip_code(text: str) -> str:
    text = re.sub(r'^\s*(`{3,}|~{3,})[^\n]*\n.*?^\s*\1\s*$', '', text, flags=re.M | re.S)
    # Inline code can contain example Markdown that isn't a link.
    return re.sub(r'(?<!`)`[^`\n]+`(?!`)', lambda m: ' ' * len(m[0]), text)


def heading_ids(text: str) -> set[str]:
    seen = {}
    result = set(re.findall(r'<a\s+(?:id|name)=["\']([^"\']+)', text, flags=re.I))
    for heading in re.findall(r'^#{1,6}\s+(.+?)\s*#*\s*$', text, flags=re.M):
        heading = re.sub(r'<[^>]+>', '', heading).strip().lower()
        heading = ''.join(c for c in heading if c in '-_ ' or unicodedata.category(c)[0] in ('L', 'N', 'M'))
        base = heading.replace(' ', '-')
        number = seen.get(base, 0)
        seen[base] = number + 1
        result.add(base if number == 0 else f'{base}-{number}')
    return result


def check(root: Path, files: set[str]) -> list[str]:
    errors = []
    for relative in sorted(files):
        if relative.startswith(('research/', 'journal/', 'docs/research/')):
            errors.append(f'{relative}: research history belongs in the private tree')
    markdown = sorted(p for p in files if p.endswith('.md') and not PRIVATE_PARTS.intersection(Path(p).parts))
    anchors = {}
    for source in markdown:
        path = root / source
        text = path.read_text(encoding='utf-8-sig')
        if source.startswith('docs/') and re.search(r'\d{4}-\d{2}-\d{2}', path.name):
            errors.append(f'{source}: dated report filename in public knowledge base')
        for match in LINK.finditer(strip_code(text)):
            raw = match[2].strip('<>')
            url = urlsplit(raw)
            if url.scheme or url.netloc:
                continue
            target = (path.parent / unquote(url.path)).resolve() if url.path else path
            try:
                relative = target.relative_to(root.resolve()).as_posix()
            except ValueError:
                errors.append(f'{source}: link leaves repository: {raw}')
                continue
            if PRIVATE_PARTS.intersection(Path(relative).parts) or relative.startswith(('research/', 'journal/', 'docs/research/')):
                errors.append(f'{source}: public link points to private/retired material: {raw}')
                continue
            if not target.exists():
                errors.append(f'{source}: missing target: {raw}')
                continue
            if target.is_file() and relative not in files:
                errors.append(f'{source}: target is not included in public sources: {raw}')
            if target.is_file() and target.suffix == '.md' and url.fragment:
                if relative not in anchors:
                    anchors[relative] = heading_ids(target.read_text(encoding='utf-8-sig'))
                if unquote(url.fragment) not in anchors[relative]:
                    errors.append(f'{source}: missing heading: {raw}')
        if source.startswith('docs/'):
            body = re.sub(r'^#{1,6}.*$', '', text, flags=re.M)
            body = re.sub(r'<!--.*?-->', '', body, flags=re.S).strip()
            if len(body) < 80:
                errors.append(f'{source}: no substantive body')
    navigation = json.loads((root / 'docs/catalog.json').read_text(encoding='utf-8'))
    entries = navigation['documents']
    indexed = {entry['path'] for entry in entries}
    expected = {p for p in markdown if p.startswith('docs/') and p != 'docs/catalog.md'}
    if len(indexed) != len(entries):
        errors.append('docs/catalog.json: duplicate document')
    for p in sorted(expected - indexed):
        errors.append(f'{p}: absent from catalog')
    for p in sorted(indexed - expected):
        errors.append(f'{p}: stale catalog entry')
    classes = json.loads((root / 'docs/reference/classes.json').read_text(encoding='utf-8'))['classes']
    names = set()
    for item in classes:
        name = item['name']
        if name in names:
            errors.append(f'class registry: duplicate {name}')
        names.add(name)
        if not re.fullmatch(r'0x[0-9A-F]{8}', item['classId']):
            errors.append(f'class registry: invalid ID for {name}')
        if item['owner'] not in ('engine', 'game') or not item['platforms'] or set(item['platforms']) - {'pc', 'ps2'}:
            errors.append(f'class registry: invalid owner/platform for {name}')
        if set(item['registrationBaseIds']) != set(item['platforms']):
            errors.append(f'class registry: base IDs/platforms disagree for {name}')
        for target in item['documentation'] + item['sources']:
            if target not in files or PRIVATE_PARTS.intersection(Path(target).parts):
                errors.append(f'class registry: {name} links to missing/nonpublic {target}')
    return errors


def main() -> int:
    files = public_files(ROOT)
    errors = check(ROOT, files)
    for relative, content in catalog.generated_files().items():
        path = ROOT / relative
        if not path.exists() or path.read_text(encoding='utf-8-sig') != content:
            errors.append(f'{relative}: stale generated content; run catalog.py')
    if errors:
        print('\n'.join(errors))
        print(f'Documentation check failed: {len(errors)} error(s).')
        return 1
    pages = sum(p.startswith('docs/') and p.endswith('.md') for p in files)
    print(f'Documentation OK: {pages} knowledge-base pages; links, class registry and navigation are current.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

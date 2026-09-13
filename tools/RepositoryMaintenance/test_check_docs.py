"""Regression cases for links that previously disappeared during reorganizations."""
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from check_docs import check


class DocumentationLinks(unittest.TestCase):
    def fixture(self, root, body, extra=None):
        files = {'docs/README.md', 'docs/catalog.json', 'docs/reference/classes.json'}
        (root / 'docs/reference').mkdir(parents=True)
        (root / 'docs/README.md').write_text('# Entry\n\n' + 'Context for the documented behavior. ' * 3 + '\n\n' + body, encoding='utf-8')
        (root / 'docs/catalog.json').write_text(json.dumps({'documents': [{'path': 'docs/README.md'}]}), encoding='utf-8')
        (root / 'docs/reference/classes.json').write_text(json.dumps({'classes': []}), encoding='utf-8')
        for path, text in (extra or {}).items():
            target = root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding='utf-8')
        return files

    def test_missing_private_and_escaping_targets_are_errors(self):
        with TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            files = self.fixture(root, '[missing](absent.md) [private](../.private/note.md) [escape](../../outside.md)')
            errors = check(root, files)
            self.assertEqual(len(errors), 3)
            self.assertTrue(any('missing target' in e for e in errors))
            self.assertTrue(any('private/retired' in e for e in errors))
            self.assertTrue(any('leaves repository' in e for e in errors))

    def test_untracked_target_is_not_publication_ready(self):
        with TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            files = self.fixture(root, '[local](../local-only.txt)', {'local-only.txt': 'local data'})
            self.assertTrue(any('not included in public sources' in e for e in check(root, files)))

    def test_unicode_duplicate_headings_and_code_examples(self):
        with TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            body = '## Поля узла\n\nОписание.\n\n## Поля узла\n\nОписание.\n\n[second](#поля-узла-1)\n\n`[example](missing.md)`\n\n```text\n[example](also-missing.md)\n```\n'
            files = self.fixture(root, body)
            self.assertEqual(check(root, files), [])
            path = root / 'docs/README.md'
            path.write_text(path.read_text(encoding='utf-8') + '\n[wrong](#поля-узла-2)\n', encoding='utf-8')
            self.assertTrue(any('missing heading' in e for e in check(root, files)))

    def test_retired_report_is_rejected_even_when_it_exists(self):
        with TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            files = self.fixture(root, '[report](research/old.md)', {'docs/research/old.md': '# Report\n\nOld history.'})
            self.assertTrue(any('private/retired' in e for e in check(root, files)))


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3
"""Check publishable Markdown paths and JSON syntax without opening game data.

Uses tracked and untracked non-ignored files. Local evidence directories are
reported separately; remote URLs and heading anchors are not validated.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
LOCAL_ONLY = {"local-data", ".codex-tmp", ".scratch", ".tmp", "artifacts"}
LINK = re.compile(r"!?\[[^\]\n]*\]\(\s*(<[^>\n]+>|[^\s)]+)(?:\s+[^)\n]*)?\)")
REFERENCE = re.compile(r"^\s{0,3}\[[^\]\n]+\]:\s*(<[^>\n]+>|\S+)")
FENCE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")


def audit() -> dict:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=ROOT, capture_output=True, check=True,
    )
    paths = sorted({p.decode("utf-8") for p in result.stdout.split(b"\0") if p})
    counts = {"markdownFiles": 0, "jsonFiles": 0, "relativeLinks": 0}
    errors, local = [], []
    published = set(paths)
    # A gitlink publishes the pinned submodule tree, not its ignored build files.
    index = subprocess.run(["git", "ls-files", "--stage", "-z"], cwd=ROOT,
                           capture_output=True, check=True)
    for record in index.stdout.split(b"\0"):
        if not record.startswith(b"160000 "):
            continue
        metadata, module_bytes = record.split(b"\t", 1)
        module = module_bytes.decode("utf-8")
        revision = metadata.split()[1].decode("ascii")
        tree = subprocess.run(["git", "-C", module, "ls-tree", "-r", "--name-only", "-z", revision],
                              cwd=ROOT, capture_output=True, check=True)
        published.update(module + "/" + p.decode("utf-8") for p in tree.stdout.split(b"\0") if p)
    for name in paths:
        path = ROOT / name
        if path.suffix.lower() not in {".md", ".json"} or not path.is_file():
            continue
        try:
            content = path.read_text(encoding="utf-8-sig")
        except UnicodeError as exc:
            errors.append({"file": name, "error": str(exc)})
            continue
        if path.suffix.lower() == ".json":
            counts["jsonFiles"] += 1
            try:
                json.loads(content)
            except ValueError as exc:
                errors.append({"file": name, "error": str(exc)})
            continue
        counts["markdownFiles"] += 1
        fence = None
        for line_number, line in enumerate(content.splitlines(), 1):
            marker = FENCE.match(line)
            if marker:
                delimiter = marker.group(1)
                if fence is None:
                    fence = delimiter
                elif delimiter[0] == fence[0] and len(delimiter) >= len(fence):
                    fence = None
                continue
            if fence is not None:
                continue
            matches = list(LINK.finditer(line))
            reference = REFERENCE.match(line)
            if reference:
                matches.append(reference)
            for match in matches:
                target = match.group(1).strip("<>")
                parts = urlsplit(target)
                if parts.scheme or parts.netloc or not parts.path:
                    continue
                counts["relativeLinks"] += 1
                resolved = (path.parent / unquote(parts.path).replace("\\", "/")).resolve()
                entry = {"file": name, "line": line_number, "target": target}
                if not resolved.is_relative_to(ROOT):
                    errors.append({**entry, "error": "outside repository"})
                    continue
                relative = resolved.relative_to(ROOT).as_posix()
                if relative.split("/")[0] in LOCAL_ONLY:
                    local.append(entry)
                elif not (relative in published or any(p.startswith(relative.rstrip("/") + "/") for p in published)):
                    errors.append({**entry, "error": "not in publishable file set"})
                elif not resolved.exists():
                    errors.append({**entry, "error": "missing target"})
    return {"schemaVersion": 1, "status": "failed" if errors else "passed",
            **counts, "errors": errors, "localOnlyLinks": local,
            "limitations": ["Remote URLs and heading anchors are not checked.",
                            "Local evidence links are classified, not validated.",
                            "Checks inline and reference link destinations outside fenced code; not a full Markdown parser."]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Print the full audit as JSON")
    args = parser.parse_args()
    report = audit()
    if args.json:
        print(json.dumps(report, indent=2, ensure_ascii=True))
    else:
        print(f"{report['status'].upper()}: {report['markdownFiles']} Markdown, "
              f"{report['jsonFiles']} JSON, {report['relativeLinks']} relative links; "
              f"{len(report['errors'])} errors, {len(report['localOnlyLinks'])} local-only links")
        for error in report["errors"]:
            print(json.dumps(error, ensure_ascii=True))
    return int(bool(report["errors"]))


if __name__ == "__main__":
    raise SystemExit(main())

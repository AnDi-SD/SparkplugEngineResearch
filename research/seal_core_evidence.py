"""Small immutable snapshot helper for completed core implementation blocks.

Bindings name the selected source files; they do not claim a full compiler
dependency closure. Original captures and intermediate failures stay intact.
"""
from pathlib import Path
import hashlib,shutil

ROOT=Path(__file__).resolve().parents[1]
def fingerprint(path):
    path=Path(path).resolve()
    return dict(path=path.relative_to(ROOT).as_posix(),bytes=path.stat().st_size,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest().upper())
def seal(folder,key_sources,artifacts):
    destination=(ROOT/folder).resolve()
    if not destination.is_relative_to(ROOT/'local-data/results'):raise ValueError('Snapshot destination must stay in local results')
    sources=[(ROOT/path).resolve() for path in key_sources]
    files=[(ROOT/path).resolve() for path in artifacts]
    for path in sources+files:
        if not path.is_relative_to(ROOT) or not path.is_file():raise ValueError(f'Missing workspace source/artifact: {path}')
    destination.mkdir(parents=True,exist_ok=False)
    def copy(path,kind):
        output=destination/kind/path.relative_to(ROOT);output.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,output)
        original=fingerprint(path);saved=fingerprint(output)
        if original['sha256']!=saved['sha256']:raise RuntimeError('Source changed while sealing')
        return dict(**original,snapshot=saved['path'])
    return dict(keySourceBindings=[copy(path,'sources') for path in sources],artifacts=[copy(path,'artifacts') for path in files])

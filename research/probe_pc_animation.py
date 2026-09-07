#!/usr/bin/env python3
"""Compile/run bounded x86 PC instruction probes; never starts the game.

Only whitelisted bodies are copied into RX pages. Cross-body calls are
redirected to known bodies or explicitly documented synthetic seams. This is
instruction-level evidence, NOT an in-game integration or renderer test.
"""
from __future__ import annotations
import argparse
import subprocess
from pathlib import Path
from inspect_serializer_manager import PC_SHA256, sha256

def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pc', type=Path, default=root/'local-data/pc-pristine/WinxClub.exe')
    parser.add_argument('--build', type=Path, default=root/'.codex-tmp/pc-animation-probe')
    args = parser.parse_args()
    if sha256(args.pc.read_bytes()) != PC_SHA256:
        raise SystemExit('Refusing non-pristine PC executable')
    build = args.build.resolve()
    build.mkdir(parents=True, exist_ok=True)
    vsdev = Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat')
    source = root/'research/probe_pc_animation.cpp'
    binary = build/'probe_pc_animation.exe'
    command = (f'call "{vsdev}" -arch=x86 -host_arch=x64 >nul && '
               f'cl /nologo /std:c++17 /EHsc /W4 /O2 "{source}" '
               f'/Fe:"{binary}" /link /INCREMENTAL:NO')
    result = subprocess.run(command, shell=True, cwd=build, timeout=120)
    if result.returncode:
        return result.returncode
    result = subprocess.run([str(binary), str(args.pc.resolve())], cwd=build, timeout=10)
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())

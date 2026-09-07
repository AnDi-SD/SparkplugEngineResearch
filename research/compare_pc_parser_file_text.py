#!/usr/bin/env python3
"""Exact native/source complete file-text result, capacity and I/O sequence."""
from pathlib import Path
import json,subprocess,sys
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_parser_file_text import main as original
def main(mode):
    native=original(mode,True)
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugParserTests.exe'
    source=json.loads(subprocess.run([str(binary),'--file',mode],capture_output=True,text=True,check=True,timeout=10).stdout)
    if native!=source:raise AssertionError(f'{mode}: exact file text/capacity/I/O mismatch')
    print('PASS exact original PC file-to-owned-text',mode);return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

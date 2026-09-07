#!/usr/bin/env python3
"""Original full small-XML parse vs C++ engine callbacks fed decoded XML events.

The external XML decoder is Python ElementTree in the C++ side and the original
in-image XML library in the native side. No source XML/library identity is claimed.
"""
from pathlib import Path
import json,subprocess,sys,xml.etree.ElementTree as ET
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_effect_template import main as original,XML_CASES

def event_input(data):
    lines=[data.hex() or '-']
    def visit(node):
        if '}' in node.tag:raise ValueError('namespace conversion not part of this bounded fixture')
        lines.append(f'S {node.tag.encode().hex()} {len(node.attrib)}')
        for key,value in node.attrib.items():lines.append(f'{key.encode().hex()} {value.encode().hex() or "-"}')
        for child in node:visit(child)
        lines.append(f'E {node.tag.encode().hex()} 0')
    visit(ET.fromstring(data));return '\n'.join(lines)+'\n'

def main(mode):
    binary=ROOT/'.codex-tmp/Sparkplug-build-pc2100-utf8/SparkplugEffectTemplateTests.exe'
    xml=mode in XML_CASES
    source=json.loads(subprocess.run([str(binary),'--xml' if xml else '--case',mode],input=event_input(XML_CASES[mode]) if xml else None,capture_output=True,text=True,timeout=10,check=True).stdout)
    native=original(mode,True)
    if source!=native:raise AssertionError(f'{mode}: source={source!r}; native={native!r}')
    print('PASS exact PC effect template',mode);return 0

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

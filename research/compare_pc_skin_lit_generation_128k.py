#!/usr/bin/env python3
"""Explicit 128KiB integration experiment; historical64KiB profile unchanged."""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded,INTEGRATION_ARENA_SIZE
from compare_pc_skin_decoded_light_generated import main
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2],sys.argv[3] if len(sys.argv)>3 else 'normal',INTEGRATION_ARENA_SIZE))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

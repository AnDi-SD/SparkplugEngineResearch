#!/usr/bin/env python3
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_mesh_writer import main
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2],sys.argv[3],False,sys.argv[4]=='base',True))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

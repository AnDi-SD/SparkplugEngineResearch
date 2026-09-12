"""Bounded owned-process live camera test; no installed game changes.

Uses the repository's existing Windows job/process wrappers. The helper is
created suspended, identity-checked, assigned to a private job, then resumed.
Only that job can be terminated. Each invocation uses a fresh runtime copy.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'research'))
import smoke_release_packages as owned

K = owned.K
snapshot = owned.api(K, 'CreateToolhelp32Snapshot', W.HANDLE, W.DWORD, W.DWORD)
class ProcessEntry(C.Structure):
    _fields_ = [('size',W.DWORD),('usage',W.DWORD),('pid',W.DWORD),('heap',C.c_size_t),('module',W.DWORD),('threads',W.DWORD),('parent',W.DWORD),('priority',W.LONG),('flags',W.DWORD),('name',W.WCHAR*260)]
class ThreadEntry(C.Structure):
    _fields_ = [('size',W.DWORD),('usage',W.DWORD),('tid',W.DWORD),('pid',W.DWORD),('priority',W.LONG),('delta',W.LONG),('flags',W.DWORD)]
process_first = owned.api(K,'Process32FirstW',W.BOOL,W.HANDLE,C.POINTER(ProcessEntry))
process_next = owned.api(K,'Process32NextW',W.BOOL,W.HANDLE,C.POINTER(ProcessEntry))
thread_first = owned.api(K,'Thread32First',W.BOOL,W.HANDLE,C.POINTER(ThreadEntry))
thread_next = owned.api(K,'Thread32Next',W.BOOL,W.HANDLE,C.POINTER(ThreadEntry))
open_thread = owned.api(K,'OpenThread',W.HANDLE,W.DWORD,W.BOOL,W.DWORD)
thread_pid = owned.api(K,'GetProcessIdOfThread',W.DWORD,W.HANDLE)
suspend = owned.api(K,'SuspendThread',W.DWORD,W.HANDLE)
wait = owned.api(K,'WaitForSingleObject',W.DWORD,W.HANDLE,W.DWORD)

def job_processes(job):
    # NVIDIA may create short-lived compiler children. Bound the list by the
    # buffer capacity, not the unrelated package-smoke limit of 16 processes.
    buffer=C.create_string_buffer(32768)
    owned.check(owned.query_job(job,3,buffer,len(buffer),None))
    count=C.cast(buffer,C.POINTER(W.DWORD))[1]
    if count>(len(buffer)-8)//C.sizeof(C.c_size_t):raise RuntimeError('Job PID list overflow')
    result=[]
    for pid in (C.c_size_t*count).from_buffer(buffer,8):
        handle=owned.open_process(0x1000,False,pid)
        if not handle:
            result.append({'pid':pid,'path':'','queryFailed':True});continue
        try:
            path=C.create_unicode_buffer(32768);length=W.DWORD(32768)
            if owned.image_name(handle,0,path,C.byref(length)):result.append({'pid':pid,'path':path.value})
            else:result.append({'pid':pid,'path':'','queryFailed':True})
        finally:owned.close_handle(handle)
    return result

def blockers():
    handle=snapshot(2,0)
    if handle == C.c_void_p(-1).value: raise C.WinError(C.get_last_error())
    result=[]
    try:
        entry=ProcessEntry();entry.size=C.sizeof(entry);more=process_first(handle,C.byref(entry))
        while more:
            if entry.name.lower() in {'winxclub.exe','winxclubdebug.exe','nvremixbridge.exe','live_camera.exe','test_remix_material.exe'}:
                result.append({'pid':entry.pid,'name':entry.name})
            more=process_next(handle,C.byref(entry))
    finally: owned.close_handle(handle)
    return result

def suspend_owned_server(job,expected):
    matches=[p for p in job_processes(job) if Path(p['path']).resolve()==expected.resolve()]
    if len(matches)!=1: raise RuntimeError('Expected exactly one owned bridge in the job')
    pid=matches[0]['pid'];handles=[];threads=[];snap=snapshot(4,0)
    if snap == C.c_void_p(-1).value: raise C.WinError(C.get_last_error())
    try:
        entry=ThreadEntry();entry.size=C.sizeof(entry);more=thread_first(snap,C.byref(entry))
        while more:
            if entry.pid==pid:
                h=open_thread(0x0002|0x0800,False,entry.tid)
                owned.check(h)
                if thread_pid(h)!=pid:
                    owned.close_handle(h);raise RuntimeError('Thread owner changed')
                if suspend(h)==0xffffffff:
                    owned.close_handle(h);raise C.WinError(C.get_last_error())
                handles.append(h);threads.append(entry.tid)
            more=thread_next(snap,C.byref(entry))
        if not handles: raise RuntimeError('No owned server threads suspended')
        return {'pid':pid,'path':str(expected),'threads':threads},handles
    except BaseException:
        for h in handles:owned.close_handle(h)
        raise
    finally:owned.close_handle(snap)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--template',default='runtime-v1');parser.add_argument('--name',required=True)
    parser.add_argument('--visible',action='store_true');parser.add_argument('--fault',action='store_true');args=parser.parse_args()
    if not all(re.fullmatch(r'[A-Za-z0-9_-]+',v) for v in [args.template,args.name]):raise RuntimeError('Single path segments required')
    base=ROOT/'local-data/rtx-remix/direct-camera-live';template=base/args.template;run=base/args.name
    if run.exists():raise RuntimeError('Use a fresh name')
    if pending:=blockers():raise RuntimeError(f'Another game/bridge/helper is running: {pending}')
    prepared=json.loads((template/'prepared.json').read_text(encoding='utf-8-sig'))
    for entry in prepared['files']:
        if hashlib.sha256((template/entry['path']).read_bytes()).hexdigest().upper()!=entry['sha256']:raise RuntimeError(f'Prepared input changed: {entry["path"]}')
    shutil.copytree(template,run)
    job=owned.create_job(None,None);owned.check(job)
    limits=owned.ExtendedLimit();limits.basic.flags=0x2000|0x200;limits.jobMemory=4*1024*1024*1024
    owned.check(owned.set_job(job,9,C.byref(limits),C.sizeof(limits)))
    pi=owned.ProcessInfo();startup=owned.Startup();startup.cb=C.sizeof(startup);startup.flags=1;startup.show=0
    executable=run/'live_camera.exe';cmd=[str(executable)]
    if args.visible:cmd.append('--visible')
    if args.fault:cmd.append('--fault')
    os.environ['DXVK_RTX_CONFIG_FILE']=str(run/'rtx.conf')
    report={'run':str(run),'mode':'fault' if args.fault else 'positive','limitSeconds':30,'jobCommitLimit':limits.jobMemory,'helperHash':hashlib.sha256(executable.read_bytes()).hexdigest().upper(),'preparedFiles':prepared['files']}
    report['expectedExitCode']=0xE052CA01 if args.fault else 0
    thread_handles=[];start=None
    try:
        owned.check(owned.create_process(str(executable),C.create_unicode_buffer(subprocess.list2cmdline(cmd)),None,None,False,0x4|0x08000000,None,str(run),C.byref(startup),C.byref(pi)))
        image=C.create_unicode_buffer(32768);length=W.DWORD(32768)
        owned.check(owned.image_name(pi.process,0,image,C.byref(length)))
        if Path(image.value).resolve()!=executable.resolve():raise RuntimeError('Created process image mismatch')
        report['pid']=pi.pid;report['verifiedImage']=image.value
        owned.check(owned.assign_job(job,pi.process));report['jobAssignedBeforeResume']=True
        (run/'launch.json').write_text(json.dumps(report,indent=2)+'\n')
        start=time.monotonic()
        if owned.resume(pi.thread)==0xffffffff:raise C.WinError(C.get_last_error())
        suspended=False
        while wait(pi.process,10)==0x102:
            if args.fault and not suspended and (run/'fault.ready').exists():
                details,thread_handles=suspend_owned_server(job,run/'.trex/NvRemixBridge.exe')
                report['suspendedOwnedServer']=details;suspended=True
                (run/'fault.proceed').write_text('owned server suspended\n')
            if time.monotonic()-start>=30:
                report['timeout']=True;break
        code=W.DWORD();owned.check(owned.exit_code(pi.process,C.byref(code)));report['exitCode']=code.value
        report['elapsedSeconds']=time.monotonic()-start
        report['jobBeforeCleanup']=job_processes(job)
    except BaseException as e:
        report['error']=str(e);report['passed']=False
    finally:
        # Only descendants assigned before first user instruction are in this
        # job. No PID-name based kill and no termination of the installed game.
        if pi.process and wait(pi.process,0)==0x102 and not report.get('jobAssignedBeforeResume'):
            owned.terminate_process(pi.process,2)
        usage=owned.ExtendedLimit()
        if owned.query_job(job,9,C.byref(usage),C.sizeof(usage),None):report['peakJobCommitBytes']=usage.peakJobMemory
        owned.terminate_job(job,2)
        for h in thread_handles:owned.close_handle(h)
        deadline=time.monotonic()+2
        while job_processes(job) and time.monotonic()<deadline:time.sleep(.01)
        report['jobAfterCleanup']=job_processes(job)
        if report['jobAfterCleanup']:report['passed']=False
        for h in [pi.thread,pi.process,job]:
            if h:owned.close_handle(h)
    # The suspended server can inherit the CRT trace handle. Read only after
    # owned-job cleanup has closed every copy, including on terminal faults.
    try:
        journal=run/'helper.jsonl';read_deadline=time.monotonic()+2
        while True:
            try:
                records=[json.loads(line) for line in journal.read_text().splitlines()] if journal.exists() else []
                break
            except PermissionError:
                # Windows process termination/CRT-handle cleanup is asynchronous.
                if time.monotonic()>=read_deadline:raise
                time.sleep(.02)
        report['helperRecords']=len(records)
        report['complete']=records[-1] if records and records[-1]['event']=='complete' else None
        reached=bool(report['complete']) if not args.fault else any(r['event']=='fault-camera-call' for r in records)
        report['passed']=not report.get('error') and not report.get('timeout') and not report['jobAfterCleanup'] and report.get('exitCode')==report['expectedExitCode'] and reached
    except BaseException as e:report['error']=str(e);report['passed']=False
    (run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ['run','mode','passed','exitCode','expectedExitCode','elapsedSeconds','complete','error','jobAfterCleanup']},indent=2))
    return 0 if report.get('passed') else 1

if __name__=='__main__':raise SystemExit(main())

#!/usr/bin/env python3
"""Check actual Viewer-to-Exporter/Importer UI handoff in a local package copy."""
from __future__ import annotations

import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid

import smoke_release_packages as win


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('suite', type=Path)
    parser.add_argument('smo', type=Path)
    parser.add_argument('san', type=Path)
    parser.add_argument('ui_probe', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--export-fbx', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    package = output / 'suite copy'
    shutil.copytree(args.suite.resolve(), package)
    assets = output / '\u0422\u0435\u0441\u0442 \u043c\u043e\u0434\u0435\u043b\u0438 \u0441 \u043f\u0440\u043e\u0431\u0435\u043b\u0430\u043c\u0438'
    assets.mkdir()
    source_inputs = [args.smo.resolve(), args.san.resolve()]
    before = {str(p): digest(p) for p in source_inputs}
    for source in source_inputs:
        shutil.copy2(source, assets / source.name)
    model, clip = [assets / source.name for source in source_inputs]
    desktop_name = 'SparkplugHandoff_' + uuid.uuid4().hex
    desktop = win.check(win.create_desktop(desktop_name, None, None, 0, 0x01FF, None))
    job = None
    handles = []
    reports = {'schemaVersion': 1, 'status': 'running', 'inputs': before, 'uiActions': [], 'limits': {'jobCommitBytes': 896*1024*1024,'activeProcesses':4,'windowWaitSeconds':30,'uiHelperSeconds':15}}
    started = time.perf_counter()
    environment = dict(os.environ)
    environment.pop('SMO_FBX_BRIDGE_PATH',None)
    for key in ['TEMP','TMP','LOCALAPPDATA','APPDATA','DOTNET_BUNDLE_EXTRACT_BASE_DIR']:
        directory=output/key.lower()
        directory.mkdir()
        environment[key]=str(directory)
    env=C.create_unicode_buffer('\0'.join(k+'='+v for k,v in sorted(environment.items(), key=lambda pair:pair[0].upper()))+'\0\0')

    def launch(executable, arguments=()):
        info = win.ProcessInfo()
        startup = win.Startup(cb=C.sizeof(win.Startup), desktop=desktop_name, flags=1, show=0)
        command=C.create_unicode_buffer(subprocess.list2cmdline([str(executable),*map(str,arguments)]))
        win.check(win.create_process(str(executable),command,None,None,False,0x4|0x400,env,str(executable.parent),C.byref(startup),C.byref(info)))
        handles.append(info)
        win.check(win.assign_job(job,info.process))
        if win.resume(info.thread)==0xFFFFFFFF: raise C.WinError(C.get_last_error())
        return info

    def wait_window(executable,title):
        deadline=time.perf_counter()+30
        while time.perf_counter()<deadline:
            ids={p['pid'] for p in win.processes(job) if Path(p['path']).resolve()==executable}
            found=next((w for w in win.windows(desktop) if w['pid'] in ids and w['title']==title and w['responsive']),None)
            if found:return found
            time.sleep(.2)
        raise RuntimeError('No responsive owned window: '+title)

    def ui(window,action,label,control=None,value=None):
        target=output/(label+'.json')
        params=[window['hwnd'],action,target]
        if control:params.append(control)
        if value:params.append(value)
        process=launch(args.ui_probe.resolve(),params)
        deadline=time.perf_counter()+15
        while time.perf_counter()<deadline:
            status=W.DWORD()
            win.check(win.exit_code(process.process,C.byref(status)))
            if status.value!=259:break
            time.sleep(.1)
        else:raise RuntimeError('UI helper timeout: '+label)
        if status.value!=0:raise RuntimeError('UI helper failed: '+label+' code='+str(status.value))
        result=json.loads(target.read_text(encoding='utf-8-sig'))
        if result['status']!='passed':raise RuntimeError('UI operation failed: '+label)
        reports['uiActions'].append({'action':action,'control':control,'report':target.name,'sha256':digest(target)})
        return result

    def close_window(window):
        win.check(win.post_message(window['hwnd'],0x10,0,0))
        deadline=time.perf_counter()+4
        while time.perf_counter()<deadline:
            if not any(p['pid']==window['pid'] for p in win.processes(job)):return
            time.sleep(.1)
        raise RuntimeError('Owned application did not close gracefully: '+window['title'])

    try:
        job=win.check(win.create_job(None,None))
        limits=win.ExtendedLimit()
        limits.basic.flags=0x2000|0x200|0x8
        limits.basic.activeProcesses=4
        limits.jobMemory=896*1024*1024
        win.check(win.set_job(job,9,C.byref(limits),C.sizeof(limits)))
        launcher=launch(package/'SmoViewer.exe',[model,clip])
        viewer=wait_window(package/'app/SmoViewer.exe','SMO Viewer')
        state=ui(viewer,'snapshot','viewer-loaded')
        controls={c['id']:c for c in state['controls'] if c['id']}
        if not all(controls.get(name,{}).get('enabled') for name in ('LaunchExporterButton','LaunchImporterButton')):
            raise RuntimeError('Viewer model has not enabled handoff buttons')
        reports['viewerModelLoaded']=True
        for product,title,button,path_control in [
            ('SmoExporter','Sparkplug SMO Exporter','LaunchExporterButton','ModelPathText'),
            ('SmoImporter','Sparkplug SMO Importer','LaunchImporterButton','SourcePathText')]:
            ui(viewer,'invoke',product+'-invoke',button)
            executable=package/'tools'/product/(product+'.Gui.exe')
            window=wait_window(executable,title)
            state=ui(window,'snapshot',product+'-loaded')
            controls={c['id']:c for c in state['controls'] if c['id']}
            control=controls.get(path_control,{})
            actual=control.get('value',control.get('name',''))
            if actual!=str(model):raise RuntimeError(product+' received a different model: '+actual)
            reports[product]={'payload':str(executable),'payloadSha256':digest(executable),'sourcePath':actual,'controls':len(state['controls'])}
            if product=='SmoExporter':
                animation_source=controls.get('AnimationSourceText',{}).get('name','')
                expected_source='\u041f\u043e\u043b\u0443\u0447\u0435\u043d\u043e \u0438\u0437 SmoViewer: 1 SAN'
                if animation_source!=expected_source or not any(c['type']=='ControlType.CheckBox' and c['name']==clip.stem for c in state['controls']):
                    raise RuntimeError('Exporter did not receive the single expected SAN')
                if list((output/'temp').glob('smoviewer-animations-*.json')):
                    raise RuntimeError('Consumed Viewer animation manifest was not cleaned up')
                reports['SmoExporter']['receivedSanCount']=1
                reports['SmoExporter']['animationManifestCleaned']=True
                if args.export_fbx:
                    expected_bridge=str(package/'native/SmoFbxBridge.exe')
                    if not any(expected_bridge in c.get('name','') for c in state['controls']):
                        raise RuntimeError('Exporter did not report the bundled native bridge')
                    ui(window,'select','fbx-format','FormatComboBox','FBX \u2014 skeleton + animations')
                    ui(window,'check','fbx-animation',clip.stem)
                    ui(window,'invoke','fbx-export','ExportButton')
                    exported=assets/(model.stem+'_export')/(model.stem+'.fbx')
                    deadline=time.perf_counter()+45
                    while not exported.is_file() and time.perf_counter()<deadline:
                        time.sleep(.2)
                    if not exported.is_file():raise RuntimeError('GUI FBX export did not produce a file')
                    final_export=ui(window,'snapshot','fbx-completed')
                    if not any(c.get('name','').startswith('\u0413\u043e\u0442\u043e\u0432\u043e:') and '\u0430\u043d\u0438\u043c\u0430\u0446\u0438\u0439 1;' in c['name'] for c in final_export['controls']):
                        raise RuntimeError('GUI did not report one successfully exported animation')
                    reports['fbxExport']={'path':str(exported),'sha256':digest(exported),'bytes':exported.stat().st_size,'bridge':expected_bridge,'bridgeSha256':digest(Path(expected_bridge))}
            close_window(window)
        final=ui(viewer,'snapshot','viewer-after-handoff')
        reports['viewerFinalControls']=len(final['controls'])
        close_window(viewer)
        launcher_status=W.DWORD()
        win.check(win.exit_code(launcher.process,C.byref(launcher_status)))
        if launcher_status.value!=0:raise RuntimeError('Root launcher exit code '+str(launcher_status.value))
        reports['remainingOwnedProcesses']=win.processes(job)
        if reports['remainingOwnedProcesses']:raise RuntimeError('Owned process remained after close')
        if any(digest(p)!=before[str(p)] for p in source_inputs):raise RuntimeError('Original input changed')
        if any(digest(assets/p.name)!=before[str(p)] for p in source_inputs):raise RuntimeError('Copied input changed')
        reports['inputsUnchanged']=True
        reports['status']='passed'
    except Exception as exc:
        reports['status']='failed'
        reports['error']=str(exc)
        if job:
            reports['processesAtFailure']=win.processes(job)
            reports['windowsAtFailure']=win.windows(desktop)
    finally:
        if job:
            limits=win.ExtendedLimit()
            if win.query_job(job,9,C.byref(limits),C.sizeof(limits),None):reports['peakJobCommitBytes']=limits.peakJobMemory
            win.terminate_job(job,1)
            win.close_handle(job)
        for info in handles:
            code=W.DWORD()
            if win.exit_code(info.process,C.byref(code)) and code.value==259:win.terminate_process(info.process,1)
            win.close_handle(info.thread)
            win.close_handle(info.process)
        win.close_desktop(desktop)
        reports['elapsedSeconds']=time.perf_counter()-started
        (output/'report.json').write_text(json.dumps(reports,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in reports.items() if k not in ['windowsAtFailure','processesAtFailure','uiActions']}))
    return int(reports['status']!='passed')


if __name__=='__main__':raise SystemExit(main())

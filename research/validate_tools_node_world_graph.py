#!/usr/bin/env python3
"""Compare selected PC Node worlds with the actual shared whole-resource loader."""
import ctypes as C
import hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/SanToVmd'))
import sparkplug_native as native

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def main(source,output):
    source=Path(source);report=json.loads(source.read_text(encoding='utf-8'))
    dll=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll'
    assert report['native_dll_sha256']==sha(dll)
    lib=native.library();lib.spv_scene_sample.argtypes=[C.c_void_p,C.c_float,C.POINTER(C.c_float),C.c_uint32]
    files=[]
    for row in report['files']:
        if 'ps2' in Path(row['file']).name.lower():continue
        data=Path(row['file']).read_bytes();assert sha(row['file'])==row['sha256']
        with native.Graph(data) as graph:
            count=len(row['nodes']);ids=(C.c_uint32*count)(*(n['id'] for n in row['nodes']))
            scene=native.check(lib.spv_graph_scene(graph._get(),ids,count))
            try:
                matrices=(C.c_float*(count*16))();native.check(lib.spv_scene_sample(scene,0,matrices,len(matrices)))
                actual=bytes(matrices)
                for index,node in enumerate(row['nodes']):
                    assert actual[index*64:(index+1)*64].hex().upper()==node['world_hex'],(row['file'],node['id'])
            finally:lib.spv_scene_destroy(scene)
        files.append({'file':row['file'],'sha256':row['sha256'],'nodes':count})
    result={'status':'passed','native_dll_sha256':sha(dll),'pc_exe_sha256':sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
        'source':str(source),'source_sha256':sha(source),'files':files,'nodes':sum(f['nodes'] for f in files),
        'scope':'Exact float bits, including signed zero; concrete Node/RenderNode selected by managed scene, compared with reconstructed whole resource graph. PS2 is not loaded through PC whole graph.'}
    target=Path(output).resolve();target.relative_to(ROOT/'local-data/results')
    target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('PASS graph worlds:',result['nodes'])
if __name__=='__main__':main(*sys.argv[1:])

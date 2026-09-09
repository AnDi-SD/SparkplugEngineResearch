"""Exact original-capture comparisons and bounded Octree host ABI fixtures.

The tiny FFPS envelope below is independent TEST input, not a tool serializer.
No SkyBox is removed from or substituted in a real game file.
"""
from pathlib import Path
import argparse,ctypes as C,hashlib,json,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools/SanToVmd'))
import sparkplug_native as native
U,F,H=C.c_uint32,C.c_float,C.c_void_p
class Fields(C.Structure):
    _fields_=[('known',U),('pivot',F*3),('mins',F*3),('maxs',F*3)]
class Octree(C.Structure):
    _fields_=[('parent',U),('children',U*8),('fields',Fields)]
def field(identity,payload):
    assert 0<=identity<31 and 0<len(payload)<256
    return bytes([0xA0+identity,len(payload)])+payload
def fixture():
    root=struct.pack('<II',0x21A70829,0x4F4F4253);children=[]
    for slot in range(8):
        leaf=struct.pack('<II',0x67672341,0x4F4F4253)+b'\0'
        children.append((slot+2,0x67672341,len(root)+14,len(leaf)))
        root+=field(2,struct.pack('<III',slot,slot+2,len(leaf))+leaf)
    root+=b'\0'+field(0,struct.pack('<3f',1,2,3))+field(1,struct.pack('<3f',-1,-2,-3))+field(2,struct.pack('<3f',9,8,7))+b'\0'
    entries=[(1,0x21A70829,0,len(root)),*children];origin=36+18*len(entries)
    data=struct.pack('<8I',0x53504646,0x26,0,origin+len(root),2,origin,len(root),len(entries))
    for identity,kind,offset,size in entries:data+=struct.pack('<IHIII',identity,0,kind,offset,size)
    return data+struct.pack('<I',0)+root
def main():
    parser=argparse.ArgumentParser();parser.add_argument('captures',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    cases=json.loads(args.captures.read_text(encoding='utf-8'));checks=guards=0
    binary=ROOT/'artifacts/native/viewer/Release/ViewerSpatialSerializationChecks.exe'
    processes=[]
    for case in cases:
        p=subprocess.run([str(binary),'--capture',case['mode'],case['wire']],capture_output=True,text=True,encoding='utf-8',timeout=20)
        processes.append({'mode':case['mode'],'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr})
        assert p.returncode==0,p.stderr
        assert json.loads(p.stdout)==case['state'],case['mode'];checks+=1
    (args.output/'source-processes.json').write_text(json.dumps(processes,indent=2)+'\n',encoding='utf-8')
    lib=native.library()
    lib.spv_octree_fields_read.argtypes=[C.POINTER(C.c_uint8),U,C.POINTER(Fields)];lib.spv_octree_fields_read.restype=C.c_int
    lib.spv_graph_octree.argtypes=[H,U,C.POINTER(Octree)];lib.spv_graph_octree.restype=C.c_int
    def reject(value):
        nonlocal guards
        assert value==0 and lib.spv_last_error();guards+=1
    for case in cases:
        wire=bytes.fromhex(case['wire']);offset=0
        # Navigate only this fixed test generator's short headers to the second
        # section. This helper is never used to read application assets.
        while wire[offset]:
            assert 0xA0<=wire[offset]<0xBF;offset+=2+wire[offset+1]
        own=wire[offset+1:];buffer=(C.c_uint8*len(own)).from_buffer_copy(own);value=Fields()
        native.check(lib.spv_octree_fields_read(buffer,len(buffer),C.byref(value)))
        assert bytes(value.mins).hex()==case['state']['mins'];checks+=1
        if case['mode'] in ('octree:values','octree:raw'):
            assert value.known==1 and bytes(value.pivot).hex()==case['state']['pivot'] and bytes(value.maxs).hex()==case['state']['maxs'];checks+=1
        else:
            assert value.known==0 and bytes(value.maxs)==bytes(12);checks+=1
    value=Fields();one=(C.c_uint8*1)(0)
    reject(lib.spv_octree_fields_read(None,1,C.byref(value)))
    reject(lib.spv_octree_fields_read(one,0,C.byref(value)))
    reject(lib.spv_octree_fields_read(one,1048577,C.byref(value)))
    reject(lib.spv_octree_fields_read(one,1,None))
    for malformed in [b'\xA0\x0B'+bytes(11)+b'\0',b'\xA1\x0C'+bytes(8),b'\0\0']:
        buffer=(C.c_uint8*len(malformed)).from_buffer_copy(malformed)
        reject(lib.spv_octree_fields_read(buffer,len(buffer),C.byref(value)))
    data=fixture();(args.output/'eight-children.smo').write_bytes(data)
    with native.Graph(data) as graph:
        assert len(graph.objects)==9 and graph.root==1;checks+=1
        value=Octree();native.check(lib.spv_graph_octree(graph._get(),1,C.byref(value)))
        assert value.parent==0 and list(value.children)==list(range(2,10));checks+=1
        assert value.fields.known==1 and tuple(value.fields.pivot)==(1,2,3);checks+=1
        reject(lib.spv_graph_octree(None,1,C.byref(value)))
        reject(lib.spv_graph_octree(graph._get(),1,None))
        reject(lib.spv_graph_octree(graph._get(),2,C.byref(value)))
        reject(lib.spv_graph_octree(graph._get(),0xFFFFFFFF,C.byref(value)))
    result={'status':'passed','original_cases':len(cases),'checks':checks,'guards':guards,'loaded_fixture_objects':9,
        'native_dll_sha256':hashlib.sha256(Path(lib._name).read_bytes()).hexdigest().upper(),
        'original_captures_sha256':hashlib.sha256(args.captures.read_bytes()).hexdigest().upper(),
        'scope':'Exact reader states and actual whole 9-object fixture load; real levels retain their SkyBox dependency.'}
    (args.output/'report.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result))
if __name__=='__main__':main()

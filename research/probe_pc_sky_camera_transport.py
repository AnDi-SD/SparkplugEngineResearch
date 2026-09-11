"""Bounded original SkyBox camera-parent worlds versus the common host projection."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from probe_pc_scene_special_registry import SpecialSceneFixture
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
class Pose(C.Structure):_fields_=[('flags',C.c_uint32),('position',C.c_float*3),('scale',C.c_float*3),('orientation',C.c_float*9),('retained',C.c_float*3)]
def main(folder):
    out=(ROOT/folder).resolve();assert out.is_relative_to(ROOT/'local-data/results') and not out.exists();out.mkdir(parents=True)
    (out/'probe-source.py').write_bytes(Path(__file__).read_bytes());started=time.perf_counter();checks=0
    dll=ROOT/'artifacts/native/viewer/Release/SparkplugViewerNative.dll';lib=C.CDLL(str(dll))
    lib.spv_sky_camera_world.argtypes=[C.POINTER(Pose),C.POINTER(C.c_float),C.POINTER(C.c_float),C.c_uint32];lib.spv_sky_camera_world.restype=C.c_int
    lib.spv_last_error.restype=C.c_char_p
    report=dict(status='running',nativeSha256=sha(dll),sourceExecutableSha256=sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),cases=[])
    def check(ok,message):
        nonlocal checks
        checks+=1
        if not ok:raise AssertionError(message)
    try:
        fixture=SpecialSceneFixture();p=fixture.p;scene=fixture.scene();root=p.uint(scene+0x14)
        camera=fixture.call(0x4a9120);fixture.call(0x421a60,this=root,args=(camera,));sky=fixture.sky();fixture.call(0x421a60,this=camera,args=(sky,))
        identity=(1.,0.,0.,0.,1.,0.,0.,0.,1.);rotated=(0.,1.,0.,-1.,0.,0.,0.,0.,1.)
        local_rotation=(1.,0.,0.,0.,0.,1.,0.,-1.,0.)
        for case,inherit in enumerate((0x70000,0x70000,0x50000,0x10000,0x60000,0)):
            cp=(10.+case*1.25,-20.+case*.5,30.-case*.25);co=identity if case==0 else rotated
            position=(1.25,-2.5,3.75);scale=(2.,-.5,1.25);orientation=identity if case==0 else local_rotation;retained=(17.,19.,23.)
            flags=(p.uint(sky+0xb0)&~0x70000)|inherit|1
            p.put_floats(camera+0x20,cp);p.put_floats(camera+0x40,co);p.put_uint(camera+0xb0,p.uint(camera+0xb0)|1)
            p.put_floats(sky+0x20,position);p.put_floats(sky+0x30,scale);p.put_floats(sky+0x40,orientation)
            p.put_floats(sky+0x74,retained);p.put_uint(sky+0xb0,flags)
            fixture.call(0x45a7d0,this=p.uint(0x75db90))
            wp=p.floats(sky+0x74,3);ws=p.floats(sky+0x80,3);wo=p.floats(sky+0x8c,9)
            check(wo==orientation,'Original SkyBox retains its local orientation')
            if not inherit&0x10000:check(wp==retained,'Original no-position-inheritance keeps prior world position')
            # Matrix packing is the already-proven common Node affine convention.
            # Original evidence below also retains all 15 unmodified world words.
            expected=[f32(wo[r*3+c]*ws[r]) if c<3 else 0. for r in range(3) for c in range(4)]+[*wp,1.]
            cw=[co[r*3+c] if c<3 else 0. for r in range(3) for c in range(4)]+[*cp,1.]
            pose=Pose(flags,(C.c_float*3)(*position),(C.c_float*3)(*scale),(C.c_float*9)(*orientation),(C.c_float*3)(*retained))
            output=(C.c_float*16)();check(lib.spv_sky_camera_world(C.byref(pose),(C.c_float*16)(*cw),output,16),str(lib.spv_last_error()))
            check(bytes(output)==struct.pack('<16f',*expected),'Common camera-parent projection equals original world PRS-derived affine')
            report['cases'].append(dict(flags=flags,poseHex=bytes(pose).hex(),cameraWorld=cw,
                originalWorldHex=bytes(p.mu.mem_read(sky+0x74,60)).hex(),expectedMatrixHex=struct.pack('<16f',*expected).hex(),actualMatrixHex=bytes(output).hex()))
        report['arenaReservedBytes']=p.allocated
        fixture.close();report['remainingAllocations']=[hex(a) for a in fixture.allocations if a not in fixture.freed]
        check(not report['remainingAllocations'],'Original teardown frees tracked native allocations')
        # Invalid host camera/input must not partially replace output.
        valid=(C.c_float*16)(*cw)
        for count,invalid in ((15,None),(16,0),(16,3),(16,15)):
            bad=(C.c_float*16)(*cw)
            if invalid is not None:bad[invalid]=float('nan') if invalid==0 else .5
            canary=(C.c_ubyte*64)(*([0xa5]*64));before=bytes(canary)
            check(not lib.spv_sky_camera_world(C.byref(pose),bad,C.cast(canary,C.POINTER(C.c_float)),count) and bytes(canary)==before,'Invalid input preserves output')
        report['status']='passed-scoped-worlds'
    except Exception as error:report.update(status='failed',error=str(error));raise
    finally:
        report.update(checks=checks,seconds=time.perf_counter()-started,dependencies=sorted((dict(path=Path(m.__file__).resolve().relative_to(ROOT).as_posix(),sha256=sha(m.__file__)) for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda row:row['path']))
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({k:report.get(k) for k in ('status','checks','seconds','arenaReservedBytes','error')}))
    return 0
if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(*sys.argv[2:]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

#!/usr/bin/env python3
"""Original whole SceneRender with real graph/Shadow and explicit device leaves.

Visibility whole-ctor gap remains explicit; ordinary and partition paths are
tested separately. No renderer factory, D3D API or nonempty shadow geometry is
claimed. Camera/mesh/device failures are controlled recording boundaries.
"""
from pathlib import Path
import sys
from pc_instruction_emulator import run_bounded
from probe_pc_scene_partition_init import VisibilityFixture
from probe_pc_visibility_runtime import CullingFixture

checks=0


def check(value,label):
    global checks
    checks+=1
    if not value:raise AssertionError(label)


class SceneRenderFixture(VisibilityFixture):
    add=CullingFixture.add

    def __init__(self,partition=True,perspective=True):
        super().__init__()
        p=self.p
        # Explicit immediate/general test modes. Raw PE bytes are not claimed
        # to be the fully initialized application's global configuration.
        p.mu.mem_write(0x74024c,b'\0')
        p.mu.mem_write(0x75f8e8,b'\0')
        self.scene_object=self.initialized_scene() if partition else self.scene()
        self.root=p.uint(p.uint(self.scene_object+0x38)+0x1d4) if partition else 0
        self.native_camera=self.object(0x4a9120)
        self.call(0x421a60,this=p.uint(self.scene_object+0x14),args=(self.native_camera,))
        self.objects.remove(self.native_camera)
        p.mu.mem_write(self.native_camera+0x230,b'\1')
        p.mu.mem_write(self.native_camera+0x231,bytes([not perspective]))
        self.call(0x45a7d0,this=p.uint(0x75db90))
        table=p.uint(self.renderer+0x18)
        for slot,offset,name in ((0x30,0x60,'projection'),(0x34,0x70,'view')):
            p.put_uint(table+slot,self.RX+offset)
            self.results[name]=1
            p.seams[self.RX+offset]=lambda p,n=name:self.device(p,n,1)
        self.already_sorted_qsort_boundary()
        self.com_result=0
        device=p.uint(self.renderer+0xc9e8)
        p.put_uint(p.uint(device)+0xe4,self.RX+0x80)

        def state(p):
            args=tuple(p.uint(p.reg('ESP')+4*i) for i in (1,2,3))
            check(args[0]==device and args[1]<256,'bounded actual D3D render-state boundary')
            self.calls.append(('state',args[1:]))
            p.fixture_return(12,eax=self.com_result)
        p.seams[self.RX+0x80]=state
        self.events=[]
        self.observing=False
        names={0x46d270:'visibility',0x48da60:'sky',0x4a85f0:'shadow',
               0x4c7210:'lens',0x4ce080:'glare',0x456910:'general-flush',
               0x454850:'alpha-flush',0x456310:'queue',0x40f9a0:'event',
               0x424b60:'node-draw',0x44fc00:'static-draw',0x4d72c0:'partition-draw'}

        def observe(mu,address,size,_):
            if self.observing and address in names:
                name=names[address]
                words=3 if name=='event' else 2 if name.endswith('-draw') else 0
                args=tuple(p.uint(p.reg('ESP')+4+4*i) for i in range(words))
                self.events.append((name,p.reg('ECX'),args))
        p.mu.hook_add(p.uc.UC_HOOK_CODE,observe)

    def draw(self):
        self.calls.clear()
        self.events.clear()
        self.observing=True
        try:return self.call(0x45ec70,this=self.scene_object,args=(self.native_camera,))&255
        finally:
            self.observing=False
            shadow=self.p.uint(0x75db7c)
            if shadow and shadow in self.allocations and shadow not in self.objects:
                self.objects.append(shadow) # actual Scene lazy factory, teardown caller-owned

    def event_names(self):return [event[0] for event in self.events]


def camera_and_render_failure_gates():
    f=SceneRenderFixture()
    p=f.p
    node,support=f.add('dynamic',(0.,0.,10.,1.))
    f.results['view']=0
    check(f.draw()==0 and [c[0] for c in f.calls]==['view'] and not f.events,
          'whole Scene returns false before anything after failed camera view')
    f.results['view'],f.results['projection']=7,0
    check(f.draw()==0 and [c[0] for c in f.calls]==['view','projection'] and not f.events,
          'camera projection false stops Scene before sky/visibility')
    f.results['projection']=7
    check(f.draw()==1 and p.uint(f.scene_object+0x40)==1,
          'whole perspective Scene succeeds through actual empty-shadow/lens graph')
    check([c[0] for c in f.calls].count('mesh')==1 and
          [e[2] for e in f.events if e[0]=='node-draw']==[(f.native_camera,1)],
          'partition path dispatches adjusted support with force1')
    check(f.event_names()==['sky','alpha-flush','visibility','node-draw','general-flush',
                      'shadow','alpha-flush','lens','glare'],
          'whole original Scene phase ordering with empty specialized lists: '+repr(f.events))
    check('event' not in f.event_names(),
          'Debug18 false skips terminal Base notification1A, not unconditional end-frame event')
    f.results['matrix']=0
    check(f.draw()==0 and 'node-draw' in f.event_names() and 'shadow' not in f.event_names() and
          'event' not in f.event_names() and not any(c[0]=='mesh' for c in f.calls),
          'RenderNode matrix false propagates to whole Scene and suppresses later phases')
    f.results['matrix'],f.results['mesh']=1,0
    check(f.draw()==1 and 'shadow' in f.event_names() and 'glare' in f.event_names(),
          'RenderNode ignores Model false, whole Scene still reaches final phases')
    f.close()


def static_failure_and_queued_flush():
    f=SceneRenderFixture()
    p=f.p
    obj,support=f.add('static',(0.,0.,10.,1.))
    f.results['matrix']=0
    check(f.draw()==1 and any(c[0]=='mesh' for c in f.calls),
          'Static ignores matrix false, Scene reaches mesh and successful end')
    f.results['matrix'],f.results['mesh']=1,0
    check(f.draw()==0 and 'static-draw' in f.event_names() and 'shadow' not in f.event_names(),
          'Static propagates Model false and stops whole Scene')
    p.mu.mem_write(0x74024c,b'\1') # explicit observed queue-enable state
    p.mu.mem_write(0x75f8e8,b'\0') # choose proved general queue, not typed no-op path
    check(f.draw()==1 and f.event_names().count('queue')==1 and 'shadow' in f.event_names(),
          'queued Static succeeds despite mesh false inside ignored general flush')
    check(p.uint(f.renderer+0xc050)&255==0 and
          p.uint(f.renderer+0xc05c)==p.uint(f.renderer+0xc058),
          'Scene clears queueing flag and general flush clears logical queue')
    f.close()


def ordinary_path_and_first_plane():
    for partition in (False,True):
        f=SceneRenderFixture(partition=partition,perspective=False)
        obj,support=f.add('dynamic',(0.,0.,.25,.125))
        check(f.draw()==1,'both explicit Scene branches complete')
        check([e[2] for e in f.events if e[0]=='node-draw']==[(f.native_camera,int(partition))],
              'partition uses force1, ordinary linked list uses force0')
        check(any(c[0]=='mesh' for c in f.calls)==partition,
              'pre-near sphere selected/forced only by partition path; CPU ordinary culls')
        check(('visibility' in f.event_names())==partition and
              not any(c[0]=='state' for c in f.calls) and 'shadow' in f.event_names(),
              'orthographic-branch Shadow executes native early return without state calls')
        f.close()


def shadow_empty_state_cache():
    f=SceneRenderFixture()
    p=f.p
    f.com_result=0x88760868
    check(f.draw()==1,'empty shadow pass does not propagate D3D state failure')
    states=[c[1] for c in f.calls if c[0]=='state']
    check(states==[(53,1),(55,1),(54,1),(58,0xffffffff),(52,1),(59,0xffffffff),
                   (174,1),(52,0),(174,0)],'first empty-shadow pass exact changed-state calls')
    check(p.uint(f.renderer+0xe4f4+53*4)==1 and p.uint(f.renderer+0xc190)==0,
          'actual state helper updates cache despite HRESULT failure; shadow clears current-light cache')
    check(f.draw()==1 and [c[1] for c in f.calls if c[0]=='state']==
          [(52,1),(174,1),(52,0),(174,0)],
          'second shadow pass suppresses cached equal states even after previous device failures')
    shadow=p.uint(0x75db7c)
    check(shadow in f.allocations and f.allocations[shadow]==0x3c and p.uint(shadow)==0x6ef138,
          'actual lazy Scene dependency is exact DXShadowVolumeManager3C')
    f.close()
    check(shadow in f.freed and p.uint(0x75db7c)==0,'actual shadow destructor clears singleton')


def debug_terminal_event():
    f=SceneRenderFixture(perspective=False)
    p=f.p
    check(f.draw()==1 and 'event' not in f.event_names(),'empty ordinary frame has no debug event')
    debug=p.uint(0x75526c)
    p.mu.mem_write(debug+0x18,b'\1')
    check(f.draw()==1 and f.events[-1]==('event',f.scene_object,(0x1a,0,0)),
          'Debug18 true reaches actual terminal notification1A on empty support graph')
    p.mu.mem_write(debug+0x18,b'\0')
    f.close()


CASES={'gates':camera_and_render_failure_gates,'static':static_failure_and_queued_flush,
       'paths':ordinary_path_and_first_plane,'shadow':shadow_empty_state_cache,
       'debug':debug_terminal_event}


def main(case):
    CASES[case]()
    print(f'PASS {checks}/{checks}: original whole SceneRender [{case}], explicit boundaries')
    return 0


if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(main(sys.argv[2]))
    # Independent fresh scenarios, not resuming a capped native invocation.
    # Each child retains30sec and every native call100k/2sec limits.
    for case in (sys.argv[1:] or CASES):
        result=run_bounded(Path(__file__),(case,))
        if result:raise SystemExit(result)
    raise SystemExit(0)

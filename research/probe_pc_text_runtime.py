"""Bounded original PC Font measurement, Text layout/read and TextNode ownership.

No method under study is substituted. Stream/allocation leaves come from the
existing fixture. The Text reader's Font reference resolver is a declared leaf
returning the actual parsed Font for one explicit fixture reference; no whole
file load or renderer startup is claimed.
"""
from pathlib import Path
import hashlib, json, struct, sys, time
from pc_instruction_emulator import run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture

ROOT=Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()
def field(i,data): return bytes([0xe0+i])+struct.pack('<I',len(data))+data
def word(i): return struct.pack('<I',i)
def string(value): return struct.pack('<H',len(value))+value

def guest(folder):
    out=ROOT/folder;out.mkdir(parents=True,exist_ok=False)
    (out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
    started=time.perf_counter();f=PCWriteBytesFixture();p=f.p
    report=dict(kind='original-pc-text-runtime',status='running',pc=sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),
      calls=[],measure=[],layout=[],node=[],reader=[],new_seams=['4678B0: explicit Font reference 0x1234 only'],
      inputs=['zero borrowed renderer page at35000000/global75DB68; no device/startup',
        'actual Font factory+reader, fixture external refcount1',
        'actual FontManager factory assigned to singleton; explicit borrowed default+28',
        'TextRenderable RTTI parent records seeded for actual IsKindOf'],
      profile='micro',arena=65536,allocation_bound=32768,child_timeout_seconds=30)
    def call(entry,this=0,args=()):
        row=dict(entry=f'{entry:08X}');report['calls'].append(row)
        try:
            value=f.call(entry,this=this,args=args);row['returned']=True;return value
        except Exception as error:
            row.update(returned=False,error=repr(error),eip=f'{p.reg("EIP"):08X}',tail=[f'{v:08X}' for v in p.tail]);raise
        finally:row.update(instructions=sum(p.visits.values()),limits=p.last_execution_limits)
    def ref(obj):return int.from_bytes(p.mu.mem_read(obj+8,2),'little')
    def snapshot(text):
        return dict(words={f'{i:02X}':p.uint(text+i) for i in range(0x58,0xa4,4)},
          font_refcount=ref(font),manager_font=p.uint(manager+0x2c),manager_color=p.uint(manager+0x30))
    def set_text(text,value):
        address=0
        if value is not None:
            address=p.allocate(len(value)+1);p.mu.mem_write(address,value+b'\0')
        call(0x4380f0,this=text,args=(address,1))
    try:
        renderer=0x35000000;p.mu.mem_map(renderer,0x10000,p.uc.UC_PROT_READ|p.uc.UC_PROT_WRITE)
        p.put_uint(0x75db68,renderer)
        for record,identity,parent in ((0x75d848,0x19a745d7,0x75e030),
          (0x75e030,0x4fda4542,0x7555f8),(0x7555f8,0x44de07fd,0x755310),(0x755310,0x415352a1,0)):
            p.put_uint(record,identity);p.put_uint(record+0x48,parent)
        font=call(0x462ec0);font_serializer=call(0x442500)
        glyphs=b''.join(struct.pack('<B4I',5 if i==65 else 7 if i==66 else 1,0,0,0x3f800000,0x3f800000) for i in range(32,256))
        wire=field(0,word(0)+word(12)+word(3)+glyphs)+b'\0'
        (out/'font-input.bin').write_bytes(wire);f.data=wire;f.position=0
        assert call(0x442660,this=font_serializer+0x10,args=(f.stream,font))&255 and f.position==len(wire)
        p.mu.mem_write(font+8,b'\1\0')
        manager=call(0x4c35a0);p.put_uint(0x755270,manager);p.put_uint(manager+0x28,font)
        # Direct native measure also records that empty inputs leave height out untouched.
        for value,wrap in [(None,0),(b'',0),(b'AB',0),(b'ABAB',5),(b'ABAB',12),
          (b'\nA\n',0),(b'A\tB\r\x1f\x80\xff',0),(b'A\0B',0),(b'AB\tA',5)]:
            address=0
            if value is not None:address=p.allocate(len(value)+1);p.mu.mem_write(address,value+b'\0')
            height=p.allocate(4);p.put_uint(height,0xdeadbeef)
            width=call(0x462c00,this=font,args=(address,wrap,height))
            report['measure'].append(dict(text=None if value is None else value.hex(),wrap=wrap,width=width,height=p.uint(height)))
        text=call(0x41a640);report['factory']=snapshot(text)
        # Actual reader owns the font; explicit resolver does not increment refs.
        def resolve(p):
            assert p.uint(p.reg('ESP')+4)==0x4693490a and p.uint(p.reg('ESP')+8)==f.stream
            assert f.data[f.position:f.position+8]==word(0x1234)+word(0)
            f.position+=8;p.fixture_return(12,eax=font)
        p.seams[0x4678b0]=resolve
        serializer=call(0x441ac0)
        for name,own in [('font-first',field(4,word(0x1234)+word(0))+field(0,string(b'AB\0'))+field(1,word(0x12345678))),
            ('repeat-fields',field(2,word(5))+field(3,word(2))+field(0,string(b'ABAB\0'))+field(4,word(0x1234)+word(0))),
            ('embedded-null',field(0,string(b'A\0B\0'))+field(3,word(1))),
            ('empty',field(0,string(b'\0')))]:
            wire=b'\0'+own+b'\0';(out/(name+'-input.bin')).write_bytes(wire)
            f.data=wire;f.position=0
            assert call(0x441c10,this=serializer+0x10,args=(f.stream,text))&255 and f.position==len(wire)
            report['reader'].append(dict(name=name,state=snapshot(text)))
        for name,value,wrap,align in [('left',b'AB',0,0),('center',b'AB',0,1),('right',b'AB',0,2),
          ('unknown-align',b'AB',0,0xffffffff),('wrap',b'ABAB',5,0),('newline',b'A\nB',0,1),
          ('high-byte',b'A\tB\x80\xff',0,2),('empty-after',b'',0,0)]:
            p.put_uint(text+0x68,wrap);p.put_uint(text+0x80,align)
            set_text(text,value);report['layout'].append(dict(name=name,text=None if value is None else value.hex(),
              wrap=wrap,alignment=align,state=snapshot(text)))
        node=call(0x41a5e0)
        report['node_factory']=dict(size=f.allocations[node],tail=[p.uint(node+i) for i in (0x1d4,0x1d8)])
        set_text(text,b'AB');p.mu.mem_write(text+8,b'\1\0') # explicit external owner
        second=call(0x41a640);p.mu.mem_write(second+8,b'\1\0')
        for name,entry,arg in [('first',0x437970,text),('duplicate',0x437970,text),('second',0x437970,second),
            ('detach-old',0x437a10,text),('detach-current',0x437a10,second),('clear',0x4379d0,None)]:
            call(entry,this=node,args=() if arg is None else (arg,))
            report['node'].append(dict(name=name,cached=p.uint(node+0x1d8),count=(p.uint(node+0xc0)-p.uint(node+0xbc))//4,
              refs=[ref(text),ref(second)],sphere=[p.uint(node+i) for i in range(0xc8,0xd8,4)]))
        p.put_uint(renderer+0xc190,node+0xf0)
        call(0x437a70,this=node,args=(1,));assert p.uint(renderer+0xc190)==0
        for obj in (text,second,serializer,font_serializer):call(p.uint(p.uint(obj)),this=obj,args=(1,))
        assert ref(font)==1
        call(p.uint(p.uint(manager)),this=manager,args=(1,));p.put_uint(0x755270,0)
        call(p.uint(p.uint(font)),this=font,args=(1,))
        report['objects_released']=all(obj in f.freed for obj in (node,text,second,font,manager,serializer,font_serializer))
        assert report['objects_released'] and not f.errors
        report['status']='passed-scoped-runtime'
    except Exception as error:report.update(status='blocked',error=repr(error))
    report.update(elapsed_seconds=time.perf_counter()-started,arena_used=p.allocated,diagnostics=[v.hex() for v in f.errors],
      allocations=[dict(address=f'{a:08X}',size=n,freed=a in f.freed) for a,n in f.allocations.items()])
    paths={Path(__file__).resolve()};paths.update(Path(m.__file__).resolve() for m in list(sys.modules.values())
      if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research'))
    report['dependencies']=[dict(path=x.relative_to(ROOT).as_posix(),sha256=sha(x)) for x in sorted(paths)]
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report.get(k) for k in ('status','error','elapsed_seconds','arena_used','measure','node','objects_released')}))
    return 0 if report['status'].startswith('passed') else 1

if __name__=='__main__':
    if sys.argv[1:2]==['--guest']:raise SystemExit(guest(sys.argv[2]))
    raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

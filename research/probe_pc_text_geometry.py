"""Actual PC FontManager3D/Text draw CPU geometry with bounded backend callbacks.

Font/manager/material factories, initialization, text layout, glyph lookup,
geometry loop and ownership execute original instructions. Only renderer
buffer acquisition/setup/draw are explicit modern-backend observation seams.
"""
from pathlib import Path
import hashlib,json,struct,sys,time
from pc_instruction_emulator import ROOT,run_bounded
from pc_serializer_fixtures import PCWriteBytesFixture
from probe_pc_text_runtime import field,word,string

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def guest(folder,mode='normal'):
 out=ROOT/folder;out.mkdir(parents=True,exist_ok=False);started=time.perf_counter()
 (out/'probe-source.py').write_bytes(Path(__file__).read_bytes())
 report=dict(status='running',pc=sha(ROOT/'local-data/pc-pristine/WinxClub.exe'),checks=0,calls=[],cases=[],
  seams=['backend Setup0','backend acquire buffer flags900','backend draw triangles counts only'],
  scope='Original PC CPU geometry and default material initialization; declared output buffers, no GPU/startup proof')
 f=PCWriteBytesFixture();p=f.p
 def check(ok,message):
  report['checks']+=1
  if not ok:raise AssertionError(message)
 def call(entry,this=0,args=()):
  row=dict(entry=f'{entry:08X}');report['calls'].append(row)
  try:return f.call(entry,this=this,args=args)
  except Exception as error:
   row.update(error=repr(error),eip=f'{p.reg("EIP"):08X}',tail=[f'{v:08X}' for v in p.tail]);raise
  finally:row.update(instructions=sum(p.visits.values()),limits=p.last_execution_limits)
 try:
  renderer=0x35000000;vb=renderer+0x10000;ib=renderer+0x14000;descriptor=renderer+0x15000
  p.mu.mem_map(renderer,0x20000,p.uc.UC_PROT_READ|p.uc.UC_PROT_WRITE)
  table=renderer+0x15100;support=renderer+0x15200;p.put_uint(renderer,table);p.put_uint(renderer+0x18,support)
  p.put_uint(0x75db68,renderer);p.put_uint(renderer+0xc174,ib);p.put_uint(descriptor+0x54,vb)
  events=[]
  def setup(machine):
   check(machine.reg('ECX')==renderer+0x18 and machine.uint(machine.reg('ESP')+4)==0,'backend setup arguments')
   events.append(dict(kind='setup',material=p.uint(renderer+0xc18c)));machine.fixture_return(4,eax=1)
  def acquire(machine):
   check(machine.reg('ECX')==renderer and machine.uint(machine.reg('ESP')+4)==0x900,'backend acquire exact vertex flags')
   events.append(dict(kind='acquire'));machine.fixture_return(4,eax=descriptor)
  def draw(machine):
   args=[machine.uint(machine.reg('ESP')+4+i*4) for i in range(3)]
   check(machine.reg('ECX')==renderer+0x18 and args[0]==2 and args[1]<=256 and args[2]<=512,'bounded triangle draw')
   events.append(dict(kind='draw',args=args));machine.fixture_return(12,eax=1)
  for target,address,callback in ((support+0x68,0x340000a0,setup),(table+0x1c,0x340000b0,acquire),(support+0x2c,0x340000c0,draw)):
   p.put_uint(target,address);p.seams[address]=callback
  font=call(0x462ec0);serializer=call(0x442500);manager=call(0x4c35a0);p.put_uint(0x755270,manager)
  glyphs=b''.join(struct.pack('<B4f',5 if i==65 else 7 if i==66 else 3 if i==32 else 1,
    .125,.25,.375,.75) for i in range(32,256))
  data=field(0,word(0)+word(12)+word(3)+glyphs)+b'\0';f.data=data;f.position=0
  (out/'font-input.bin').write_bytes(data)
  check(call(0x442660,this=serializer+0x10,args=(f.stream,font))&255 and f.position==len(data),'actual Font reader')
  p.mu.mem_write(font+8,b'\1\0');p.put_uint(manager+0x28,font)
  check(call(0x41e8b0,this=manager)&255,'actual common material initializer')
  material=p.uint(manager+0x38);pass_=p.uint(material+0x4c);layer=p.uint(pass_+0x18);texture=p.uint(layer+0x10)
  check(material==p.uint(manager+0x34) and p.uint(material)==0x6ef264 and p.uint(material+8)&65535==2,'both manager references own actual DXMaterial')
  report['material']=dict(size=f.allocations[material],states=[p.uint(material+0x18+i*4) for i in range(11)],
   colors=bytes(p.mu.mem_read(material+0x78,68)).hex(),flags=list(p.mu.mem_read(material+0x6c,2)),
   passBlend=p.uint(pass_+0x10),layers=p.uint(pass_+0x14),layerTable=p.uint(layer),
   textureStates=[p.uint(texture+0x10+i*4) for i in range(9)],refs=p.uint(material+8)&65535)
  # Actual IsKindOf traversal on the default layer used by Render3D.
  for record,identity,parent in ((0x75ffa8,0x234c576b,0x75df70),(0x75df70,0x7f577c6d,0x755310),
   (0x755310,0x415352a1,0)):
   p.put_uint(record,identity);p.put_uint(record+0x48,parent)
  cases=[('null',None,0,(0.,0.,0.)),('empty',b'',0,(0.,0.,0.)),('AB',b'AB',0,(0.,0.,0.)),
   ('space',b'A B',0,(2.25,-4.,3.75)),('newline',b'A\nB',0,(-1.5,2.,-3.75)),
   ('controls',b'A\t\r\x1f\x80\xff',0,(0.,0.,0.)),('word-wrap',b'AB AB AB',12,(0.,0.,0.))]
  if mode=='long-word':cases=[('long-word',b'ABAB',5,(0.,0.,0.))]
  for name,text,wrap,position in cases:
   events.clear();p.mu.mem_write(vb,b'\xa5'*0x4000);p.mu.mem_write(ib,b'\xa5'*0x1000)
   pos=p.allocate(12);p.mu.mem_write(pos,struct.pack('<3f',*position));address=0
   if text is not None:address=p.allocate(len(text)+1);p.mu.mem_write(address,text+b'\0')
   call(0x41e770,this=manager,args=(font,0xff804020))
   returned=call(0x41f3c0,this=manager,args=(pos,address,wrap))&255
   check(returned==1,'original FontManager3D success')
   draw_event=next((e for e in events if e['kind']=='draw'),None);vertices=0 if draw_event is None else draw_event['args'][2]
   indices=0 if draw_event is None else draw_event['args'][1]*3
   check(bytes(p.mu.mem_read(vb+512*24,0x1000))==b'\xa5'*0x1000 and bytes(p.mu.mem_read(ib+512*3,0x400))==b'\xa5'*0x400,'bounded output canaries')
   report['cases'].append(dict(name=name,text=None if text is None else text.hex(),wrap=wrap,position=position,
    vertices=vertices,indices=indices,vertexBytes=bytes(p.mu.mem_read(vb,vertices*24)).hex(),
    indexBytes=bytes(p.mu.mem_read(ib,indices*2)).hex(),events=list(events),returned=returned))
  if mode=='normal':
   # Text's native gate queues every AlphaSort-enabled object until the
   # renderer enters its alpha-dispatch phase. Supply that phase explicitly;
   # this probe studies the draw body, not queue scheduling/visibility.
   p.mu.mem_write(renderer+0x44,b'\x01');report['textDrawPhase']='explicit renderer+44=1'
   text=call(0x41a640);text_serializer=call(0x441ac0)
   for alignment in (0,1,2,7):
    data=b'\0'+field(0,string(b'AB\0'))+field(1,word(0x7f123456))+field(3,word(alignment))+b'\0'
    f.data=data;f.position=0
    check(call(0x441c10,this=text_serializer+0x10,args=(f.stream,text))&255 and f.position==len(data),'actual Text reader with borrowed default Font')
    events.clear();p.mu.mem_write(vb,b'\xa5'*0x4000);p.mu.mem_write(ib,b'\xa5'*0x1000)
    check(call(0x437d30,this=text,args=(0,0))&255,'actual complete Text render path')
    event=next(e for e in events if e['kind']=='draw');vertices=event['args'][2];indices=event['args'][1]*3
    check(p.uint(manager+0x34)==material and p.uint(material+8)&65535==2,'Text render restores previous owning material')
    report['cases'].append(dict(name='text-align-'+str(alignment),origin='TextRenderable',alignment=alignment,text='4142',wrap=0,
     color=0x7f123456,width=p.uint(text+0x84),vertices=vertices,indices=indices,
     vertexBytes=bytes(p.mu.mem_read(vb,vertices*24)).hex(),indexBytes=bytes(p.mu.mem_read(ib,indices*2)).hex(),events=list(events),returned=1))
   call(p.uint(p.uint(text)),this=text,args=(1,));call(p.uint(p.uint(text_serializer)),this=text_serializer,args=(1,))
  call(p.uint(p.uint(manager)),this=manager,args=(1,));check(material in f.freed and pass_ in f.freed and layer in f.freed and texture in f.freed,'actual manager destructor frees both material edges and nested owners')
  call(p.uint(p.uint(serializer)),this=serializer,args=(1,));call(p.uint(p.uint(font)),this=font,args=(1,))
  for address in (0x75526c,0x755264):
   obj=p.uint(address)
   if obj:call(p.uint(p.uint(obj)),this=obj,args=(1,))
  check(set(f.allocations)==set(f.freed),'all tracked actual objects freed');report['status']='passed-scoped-geometry'
 except Exception as error:report.update(status='failed',error=repr(error),eip=f'{p.reg("EIP"):08X}')
 report['elapsedSeconds']=time.perf_counter()-started;report['arena']=p.allocated
 report['dependencies']=sorted((dict(path=Path(m.__file__).resolve().relative_to(ROOT).as_posix(),sha256=sha(m.__file__))
  for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().is_relative_to(ROOT/'research')),key=lambda row:row['path'])
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:report.get(k) for k in ('status','checks','elapsedSeconds','arena','error','eip')}));return 0 if report['status'].startswith('passed') else 1
if __name__=='__main__':
 if sys.argv[1:2]==['--guest']:raise SystemExit(guest(*sys.argv[2:]))
 raise SystemExit(run_bounded(Path(__file__),sys.argv[1:]))

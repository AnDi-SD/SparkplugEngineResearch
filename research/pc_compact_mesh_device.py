"""Fixed external COM buffer/declaration data in one declared interface page.

Engine objects, declaration elements/map and CPU temporaries remain in arena.
Only two <=512-byte COM payloads and their interface tables use this page.
"""
def install_mesh_device(f):
 p=f.p;base=0x34060000;p.mu.mem_map(base,0x1000,p.uc.UC_PROT_ALL)
 f.mesh_buffers={};f.mesh_events=[];f.declaration_arrays=[];f.mesh_declaration_refs=0
 buffer_table=base+0x180;declaration=base+0x200;declaration_table=base+0x220
 f.mesh_com_tokens={declaration:1};p.put_uint(declaration,declaration_table)
 def args(m,n):return [m.uint(m.reg('ESP')+4+4*i) for i in range(n)]
 def create(m,kind):
  device,size,usage,format_,pool,out,shared=args(m,7)
  f.check(device==f.device and 0<size<=512 and not shared and len(f.mesh_buffers)<2,'bounded external COM buffer creation')
  index=len(f.mesh_buffers);obj=base+0x100+index*0x10;data=base+0x400+index*0x200
  p.put_uint(obj,buffer_table);p.mu.mem_write(data,bytes(size));p.put_uint(out,obj)
  f.mesh_buffers[obj]=dict(kind=kind,data=data,size=size,refs=1,locks=0)
  f.mesh_com_tokens[obj]=2 if kind=='index' else 3
  f.mesh_events.append(['create',kind,size,usage,format_,pool]);m.fixture_return(28,eax=0)
 def release(m):
  obj=args(m,1)[0];b=f.mesh_buffers[obj];f.check(b['refs']==1 and b['locks']==0,'COM buffer lifetime and completed locks')
  b['refs']=0;f.mesh_events.append(['release',b['kind']]);m.fixture_return(4,eax=0)
 def lock(m):
  obj,offset,size,out,flags=args(m,5);b=f.mesh_buffers[obj]
  f.check(b['refs']==1 and (offset,size,flags)==(0,0,0),'actual whole-buffer lock request')
  b['locks']+=1;m.put_uint(out,b['data']);f.mesh_events.append(['lock',b['kind']]);m.fixture_return(20,eax=0)
 def unlock(m):
  obj=args(m,1)[0];b=f.mesh_buffers[obj];f.check(b['locks']==1,'actual unlock matches live lock')
  b['locks']=0;f.mesh_events.append(['unlock',b['kind']]);m.fixture_return(4,eax=0)
 def declaration_create(m):
  device,elements,out=args(m,3);size=f.allocations.get(elements,0)
  f.check(device==f.device and 0<size<=256 and size%8==0,'bounded actual declaration element array')
  raw=bytes(p.mu.mem_read(elements,size));end=bytes.fromhex('ff00000011000000')
  locations=[i for i in range(0,size,8) if raw[i:i+8]==end]
  f.check(len(locations)==1 and not f.mesh_declaration_refs,'one real declaration terminator/create')
  f.declaration_arrays.append(raw[:locations[0]+8].hex());f.mesh_declaration_refs=1;m.put_uint(out,declaration);m.fixture_return(12,eax=0)
 def declaration_release(m):
  f.check(args(m,1)==[declaration] and f.mesh_declaration_refs==1,'single original declaration COM release')
  f.mesh_declaration_refs=0;m.fixture_return(4,eax=0)
 device_table=p.uint(f.device)
 for slot,address,handler in ((0x68,base+0x10,lambda m:create(m,'vertex')),(0x6c,base+0x20,lambda m:create(m,'index')),
                             (0x158,base+0x90,declaration_create)):
  p.put_uint(device_table+slot,address);p.seams[address]=handler
 for slot,address,handler in ((8,base+0x30,release),(0x2c,base+0x40,lock),(0x30,base+0x50,unlock)):
  p.put_uint(buffer_table+slot,address);p.seams[address]=handler
 p.put_uint(declaration_table+8,base+0xb0);p.seams[base+0xb0]=declaration_release
 head=f.arena.allocate_owned(24)
 for off in (0,4,8):p.put_uint(head+off,head)
 p.mu.mem_write(head+20,b'\1\1');p.put_uint(f.renderer+0xf35c,head);p.put_uint(f.renderer+0xf360,0)

def clear_mesh_device(f):
 f.call(0x4ae140,this=f.renderer+0xf358)
 f.check(not f.mesh_declaration_refs and all(b['refs']==0 and b['locks']==0 for b in f.mesh_buffers.values()),
         'all mesh COM resources and map entries retired')

#pragma once
// Own allocation/protection fixtures. No foreign process or native game code.
static void SceneReadWindowTests() {
  namespace memory=winx_remix::scene_memory;
  SYSTEM_INFO system{};GetSystemInfo(&system);const size_t page=system.dwPageSize;
  auto bytes=static_cast<unsigned char*>(VirtualAlloc(nullptr,page*8,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
  Check(bytes!=nullptr,"own memory fixture allocation");
  const auto base=reinterpret_cast<uintptr_t>(bytes);
  for(size_t i=0;i<page*8;++i)bytes[i]=static_cast<unsigned char>(i);
  unsigned char expected[16384]{},actual[16384]{};DWORD previous=0;
  const DWORD protections[]={PAGE_READWRITE,PAGE_READONLY,PAGE_EXECUTE_READ,PAGE_NOACCESS,PAGE_READWRITE|PAGE_GUARD};
  for(auto protection:protections) {
    Check(VirtualProtect(bytes+page,page,protection,&previous)!=0,"set owned second-page protection");
    for(auto offset:{page,page-172})for(size_t count:{size_t(4),size_t(344)}) {
      const bool readable=memory::Read(base+offset,expected,count);
      memory::ReadWindow window;
      for(unsigned repeat=0;repeat<3;++repeat) {
        Check(window.Read(base+offset,actual,count)==readable,"exact, prefetched and cached access match OS result");
        if(readable)Check(!memcmp(expected,actual,count),"complete bytes match exact OS read");
        MEMORY_BASIC_INFORMATION info{};
        Check(VirtualQuery(bytes+page,&info,sizeof(info))==sizeof(info)&&info.Protect==protection,"guard and read protections remain unchanged");
      }
    }
  }
  Check(VirtualProtect(bytes+page,page,PAGE_READWRITE,&previous)!=0,"restore own readable page");
  {
    memory::ReadWindow window;
    Check(window.Read(base,actual,344)&&window.Read(base+360,actual,344),"adjacent records populate one observation window");
    Check(window.Read(base+page-172,actual,344)&&!memcmp(actual,bytes+page-172,344),"cached record crosses page boundary");
    Check(window.Read(base+1,actual,16384)&&!memcmp(actual,bytes+1,16384),"maximum read crosses multiple pages");
  }
  bytes[12]^=1;
  {
    memory::ReadWindow interleaved;
    const size_t offsets[]={0,page*2+10,page+20,page*3+30,360,page*2+370,page+380,page*3+390,720,page*2+730,page+740,page*3+750};
    for(auto offset:offsets)Check(interleaved.Read(base+offset,actual,344)&&!memcmp(actual,bytes+offset,344),"interleaved pages retain exact record bytes");
  }
  {
    memory::ReadWindow next;
    Check(next.Read(base,actual,344)&&!memcmp(actual,bytes,344),"new observation reads changed bytes");
    Check(next.Read(base+360,actual,344),"new observation can prefetch");
  }
  Check(VirtualProtect(bytes,page,PAGE_NOACCESS,&previous)!=0,"change protection between observations");
  {
    memory::ReadWindow next;Check(!next.Read(base,actual,344),"new observation rejects newly inaccessible source");
  }
  Check(VirtualProtect(bytes,page,PAGE_READWRITE,&previous)!=0,"restore first page");
  Check(VirtualFree(bytes+page*7,page,MEM_DECOMMIT)!=0,"decommit owned final page");
  {
    memory::ReadWindow next;Check(!next.Read(base+page*7,actual,344),"reserved memory is inaccessible");
    for(auto address:{uintptr_t(0),uintptr_t(0xffff),uintptr_t(0x7fff0000),uintptr_t(0xffffffff)})
      Check(!next.Read(address,actual,4),"reject out of PC address range");
    Check(!next.Read(base,actual,16385)&&!next.Read(0x7ffeffff,actual,4),"reject excessive size and upper-bound crossing");
  }
  Check(VirtualFree(bytes,0,MEM_RELEASE)!=0,"release owned allocation");
}

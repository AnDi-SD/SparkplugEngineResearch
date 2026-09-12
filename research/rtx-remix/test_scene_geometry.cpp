// Own render-extension contracts on literal observed PC layouts. No game code
// or GPU is simulated; original SceneRender integration is checked live.
#define WINX_REMIX_TEST
#include "winx_d3d9_probe.cpp"
static unsigned checks;
static void Check(bool ok,const char* text) {++checks;if(!ok){fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
static uint32_t Ptr(const void* p) {return reinterpret_cast<uintptr_t>(p);}
static void Put(void* p,unsigned at,uint32_t v) {memcpy(static_cast<char*>(p)+at,&v,4);}
struct Fixture {
  unsigned char scene[0x54]{},system[0x1d8]{},root[0x84]{},child[0x84]{},zone[0xc8]{};
  unsigned char objects[2][0x10c]{},payload[0x8c]{},manager[0x54]{};
  unsigned char dynamic[0x130]{};uint32_t dynamicList[2]{};
  uint32_t staticList[3]{},children[1]{},zoneRoots[2]{},selection[2]{};
  Fixture() {
    Put(scene,0x38,Ptr(system));Put(system,0x1d4,Ptr(root));
    for(auto node:{root,child}) {Put(node,0,0x6e4420);Put(node,0x80,Ptr(scene));Put(node,0x60,Ptr(zone));}
    Put(root,0x58,Ptr(children));Put(root,0x5c,1);children[0]=Ptr(child);
    Put(child,0x54,Ptr(root));Put(child,0x78,Ptr(payload));
    Put(zone,0,0x6ebacc);zoneRoots[0]=Ptr(root);zoneRoots[1]=Ptr(child);Vector(zone,0xb4,zoneRoots,2);
    for(unsigned i=0;i<2;++i) {
      auto p=objects[i];Put(p,0,0x6e65e8);Put(p,0x14,0x6e6604);Put(p,0x84,Ptr(p));Put(p,0x88,Ptr(scene));Put(p,0x78,41+i);
    }
    staticList[0]=Ptr(objects[0]);staticList[1]=Ptr(objects[1]);staticList[2]=Ptr(objects[0]);Vector(root,0x30,staticList,3);
    Put(payload,0,0x6f4540);Put(payload,0x10,0x6f4528);Put(payload,0x80,Ptr(payload));Put(payload,0x88,Ptr(scene));Put(payload,0x74,43);
    Put(dynamic,0xb4,0x6dcadc);Put(dynamic,0x3c,Ptr(scene));Put(dynamic,0x124,Ptr(dynamic));Put(dynamic,0x118,44);
    dynamicList[0]=dynamicList[1]=Ptr(dynamic);Vector(root,0x20,dynamicList,2);
    selection[0]=Ptr(objects[0])+0x14;selection[1]=0;Vector(manager,0x28,selection,2);
    Put(manager,0,0x6e8cdc);Put(manager,0x14,91);
  }
  static void Vector(void* object,unsigned at,uint32_t* entries,unsigned count) {
    Put(object,at+4,Ptr(entries));Put(object,at+8,Ptr(entries)+count*4);Put(object,at+12,Ptr(entries)+count*4);
  }
};
static uintptr_t __fastcall MockRender(void*,void*,void*) {return 0x12345678;}
static uintptr_t __fastcall RaiseRender(void*,void*,void*) {RaiseException(0xe0421001,0,0,nullptr);return 0;}
static bool RunRaised(scene_geometry::Scope* scope) {
  __try {scene_geometry::RenderAndRestore(scope,nullptr,nullptr);}
  __except(GetExceptionCode()==0xe0421001?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {return true;}
  return false;
}
int main() {
  using namespace scene_geometry;Fixture f;sceneGeometryEnabled=true;
  uint32_t readValue=0;
  Check(!Read(1,&readValue,4) && !Read(0x7ffeffff,&readValue,4),"local read address and end bounds");
  Check(!Read(Ptr(f.scene),&readValue,16385),"local read size bound");
  auto inaccessible=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);
  Check(inaccessible && !Read(Ptr(inaccessible),&readValue,4),"local inaccessible page returns unsupported");
  if(inaccessible)VirtualFree(inaccessible,0,MEM_RELEASE);
  const auto registry=ReadRegistry(Ptr(f.scene));
  Check(registry.valid && registry.nodes==2 && registry.supports.size()==4 && registry.dynamic==1,"static and dynamic ownership with duplicate references and zone roots");
  Put(f.dynamic,0xb4,0x6e2ca0);const auto custom=ReadRegistry(Ptr(f.scene));
  Check(custom.valid && custom.supports.size()==3 && custom.unsupportedDynamic==1,"custom dynamic support remains on original selection path");Put(f.dynamic,0xb4,0x6dcadc);
  Put(f.dynamic,0x3c,0);Check(!ReadRegistry(Ptr(f.scene)).valid,"foreign dynamic scene rejected");Put(f.dynamic,0x3c,Ptr(f.scene));
  Put(f.objects[0],0x88,0);Check(!ReadRegistry(Ptr(f.scene)).valid,"foreign scene object rejected");Put(f.objects[0],0x88,Ptr(f.scene));
  Put(f.payload,0,0x123);Check(!ReadRegistry(Ptr(f.scene)).valid,"unknown payload ABI rejected");Put(f.payload,0,0x6f4540);
  Put(f.root,0x5c,9);Check(!ReadRegistry(Ptr(f.scene)).valid,"bounded child array");Put(f.root,0x5c,1);
  Put(f.root,0x38,Ptr(f.staticList)+limit*4+4);Check(!ReadRegistry(Ptr(f.scene)).valid,"invalid static vector rejected");Fixture::Vector(f.root,0x30,f.staticList,3);
  uint32_t original[3]{};memcpy(original,f.manager+0x2c,12);
  {
    Scope scope(Ptr(f.scene),0);
    scope.ExtendSelection(Ptr(f.manager),registry);
    Check(scope.manager==Ptr(f.manager) && scope.expanded.size()==5,"original entries retained and missing static/dynamic supports appended");
    Check(scope.expanded[0]==f.selection[0] && scope.expanded[1]==0,"original order and null slots preserved");
    Check(Word(Ptr(f.manager)+0x2c)==Ptr(scope.expanded.data()),"temporary output borrows only scope storage");
    Check(Word(Ptr(f.objects[0])+0x78)==41,"already selected object's mark unchanged");
    Check(Word(Ptr(f.objects[1])+0x78)==91 && Word(Ptr(f.payload)+0x74)==91,"additional static visibility marks use current original stamp");
    Check(Word(Ptr(f.dynamic)+0x118)==91,"dynamic mark uses current original stamp without changing Enabled");
    BeforeSelect();
    Check(memcmp(original,f.manager+0x2c,12)==0,"native clear sees original allocation");
    Check(Word(Ptr(f.objects[1])+0x78)==42 && Word(Ptr(f.payload)+0x74)==43,"marks restored before another selection");
    Check(Word(Ptr(f.dynamic)+0x118)==44,"dynamic mark restored before another selection");
    scope.Restore();Check(memcmp(original,f.manager+0x2c,12)==0,"restore idempotent");
  }
  Check(active==nullptr,"scope exit restores active stack");
  {
    Scope outer(Ptr(f.scene),0);outer.ExtendSelection(Ptr(f.manager),registry);
    {Scope nested(Ptr(f.scene),0);Check(memcmp(original,f.manager+0x2c,12)==0 && active==&nested,"nested scene restores parent borrow before native work");}
    Check(active==&outer,"nested scope restores parent identity");
  }
  {
    Scope scope(Ptr(f.scene),0);scope.ExtendSelection(Ptr(f.manager),registry);
    originalRender=reinterpret_cast<NativeRender>(MockRender);
    Check(RenderAndRestore(&scope,nullptr,nullptr)==0x12345678,"preserves entire original return register");
    Check(memcmp(original,f.manager+0x2c,12)==0,"normal renderer return restores native allocation");
  }
  {
    Scope scope(Ptr(f.scene),0);scope.ExtendSelection(Ptr(f.manager),registry);
    originalRender=reinterpret_cast<NativeRender>(RaiseRender);
    Check(RunRaised(&scope),"original structured exception propagates to caller");
    Check(memcmp(original,f.manager+0x2c,12)==0 && active==nullptr,"exception finally restores allocation and active scope");
    Check(Word(Ptr(f.objects[1])+0x78)==42 && Word(Ptr(f.payload)+0x74)==43,"exception finally restores marks");
  }
  Check(active==nullptr,"no remaining borrowed scope");
  printf("PASS %u checks; native allocations untouched; no remaining borrowed scope\n",checks);
}

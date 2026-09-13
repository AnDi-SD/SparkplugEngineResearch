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
static DWORD WINAPI AdvanceRegistryEpoch(void*) {
  for(unsigned i=0;i<1000;++i)scene_geometry::AdvanceMutationSerial();
  return 0;
}
static void RegistrySnapshots(Fixture& f) {
  using namespace scene_geometry;
  auto registry=ReadRegistry(Ptr(f.scene));
  Check(registry.valid && registry.scene==Ptr(f.scene) && registry.system==Ptr(f.system) &&
    registry.root==Ptr(f.root) && registry.mutationSerial==MutationSerial(),"registry records actual scene/partition system/root and mutation serial");
  Check(registry.occurrencesComplete && registry.occurrences.size()==6,"every static/render-node vector occurrence and payload retained, no duplicate graph visits");
  for(unsigned i=0;i<3;++i) {
    const auto& occurrence=registry.occurrences[i];
    Check(occurrence.node==Ptr(f.root) && occurrence.object==f.staticList[i] &&
      occurrence.support==f.staticList[i]+0x14 && occurrence.source==OccurrenceSource::StaticVector &&
      occurrence.ordinal==i,"static occurrence source and original ordinal");
  }
  for(unsigned i=0;i<2;++i) {
    const auto& occurrence=registry.occurrences[3+i];
    Check(occurrence.node==Ptr(f.root) && occurrence.object==Ptr(f.dynamic) && occurrence.support==Ptr(f.dynamic)+0xb4 &&
      occurrence.source==OccurrenceSource::RenderNodeVector && occurrence.ordinal==i,"each repeated render-node vector slot keeps its native source and ordinal");
  }
  const auto& payload=registry.occurrences[5];
  Check(payload.node==Ptr(f.child) && payload.object==Ptr(f.payload) && payload.support==Ptr(f.payload)+0x10 &&
    payload.source==OccurrenceSource::PartitionPayload && payload.ordinal==0,"payload occurrence uses child owner and adjusted support");
  Check(registry.supports.size()==4 && std::is_sorted(registry.supports.begin(),registry.supports.end()) &&
    std::adjacent_find(registry.supports.begin(),registry.supports.end())==registry.supports.end(),"visibility supports still sorted and unique independently of occurrences");

  uint32_t duplicateChild[]={Ptr(f.objects[0]),0,Ptr(f.objects[0])};
  Fixture::Vector(f.child,0x30,duplicateChild,3);
  auto duplicated=ReadRegistry(Ptr(f.scene));
  Check(duplicated.valid && duplicated.supports==registry.supports && duplicated.occurrences.size()==8,
    "same static object in another node retains each registration without changing visibility");
  Check(duplicated.occurrences[6].node==Ptr(f.child) && duplicated.occurrences[6].ordinal==0 &&
    duplicated.occurrences[7].node==Ptr(f.child) && duplicated.occurrences[7].ordinal==2 &&
    duplicated.occurrences[6].object==duplicated.occurrences[7].object,"null gaps do not renumber native vector slots");
  Fixture::Vector(f.child,0x30,nullptr,0);
  uint32_t dynamicChild[]={Ptr(f.dynamic),0,Ptr(f.dynamic)};
  Fixture::Vector(f.child,0x20,dynamicChild,3);
  const auto repeatedDynamic=ReadRegistry(Ptr(f.scene));
  Check(repeatedDynamic.valid && repeatedDynamic.supports==registry.supports && repeatedDynamic.dynamic==1 &&
    repeatedDynamic.occurrences.size()==8,"render-node references across partition cells keep occurrences and one visibility support");
  Check(repeatedDynamic.occurrences[6].node==Ptr(f.child) && repeatedDynamic.occurrences[6].ordinal==0 &&
    repeatedDynamic.occurrences[7].node==Ptr(f.child) && repeatedDynamic.occurrences[7].ordinal==2 &&
    repeatedDynamic.occurrences[6].source==OccurrenceSource::RenderNodeVector &&
    repeatedDynamic.occurrences[7].support==Ptr(f.dynamic)+0xb4,"render-node null gaps and duplicate cross-cell ordinals preserved");
  Put(f.dynamic,0xb4,0x6e2ca0);const auto unsupported=ReadRegistry(Ptr(f.scene));
  Check(unsupported.valid && unsupported.dynamic==0 && unsupported.unsupportedDynamic==1 &&
    unsupported.occurrences.size()==4 && unsupported.supports.size()==3,"custom dynamic support produces no supported occurrence even when repeated across cells");
  Put(f.dynamic,0xb4,0x6dcadc);Fixture::Vector(f.child,0x20,nullptr,0);
  Put(f.dynamic,0,0x6e02cc);const auto derived=ReadRegistry(Ptr(f.scene));
  Check(derived.valid && derived.supports==registry.supports && derived.occurrences.size()==6,
    "inherited support qualification does not impose exact RenderNode primary whitelist");Put(f.dynamic,0,0);
  uint64_t outerSerial=0;
  {
    Scope scope(Ptr(f.scene),0);outerSerial=scope.serial;
    f.staticList[1]=0;
    const auto& snapshot=scope.RegistrySnapshot();
    Check(snapshot.valid && snapshot.occurrences.size()==5,"scope snapshot is lazy until first consumer");
    f.staticList[1]=Ptr(f.objects[1]);
    Check(&scope.RegistrySnapshot()==&snapshot && snapshot.occurrences.size()==5,"second consumer reuses immutable scope observation without another walk");
    Scope nested(Ptr(f.scene),0);
    Check(nested.serial>scope.serial && nested.RegistrySnapshot().occurrences.size()==6 &&
      &nested.RegistrySnapshot()!=&snapshot,"nested scope has its own serial and snapshot");
  }
  {
    Scope scope(Ptr(f.scene),0);
    Check(scope.serial>outerSerial && scope.RegistrySnapshot().occurrences.size()==6,"subsequent scope observes current graph with a new operation serial");
    const auto& snapshot=scope.RegistrySnapshot();const auto captured=snapshot.mutationSerial;
    uint32_t selection[3]{};memcpy(selection,f.manager+0x2c,12);
    Check(AdvanceMutationSerial()==captured+1,"known retirement advances adapter mutation serial");
    Check(&scope.RegistrySnapshot()==&snapshot && snapshot.mutationSerial==captured &&
      snapshot.mutationSerial!=MutationSerial(),"retirement does not silently refresh a snapshot already joined to an owner");
    scope.ExtendSelection(Ptr(f.manager),snapshot);
    Check(scope.manager==0 && scope.expanded.empty() && !memcmp(selection,f.manager+0x2c,12),
      "known stale cached registry cannot replace native visibility output");
  }
  {
    Scope failed(Ptr(f.scene),0);Put(f.system,0x1d4,0);
    Check(!failed.RegistrySnapshot().valid,"unavailable root produces rejected lazy observation");
    Put(f.system,0x1d4,Ptr(f.root));
    Check(!failed.RegistrySnapshot().valid,"failed observation is not retried under same scope identity");
  }
  {
    Scope fresh(Ptr(f.scene),0);
    Check(fresh.RegistrySnapshot().valid && fresh.RegistrySnapshot().mutationSerial==MutationSerial(),
      "next scope can observe restored root at current mutation epoch");
  }
  {
    Fixture pressure;
    constexpr unsigned nodeCount=occurrenceLimit/limit+1;
    std::vector<unsigned char> nodes(nodeCount*0x84);
    std::vector<uint32_t> links(nodeCount),repeats(limit,Ptr(pressure.objects[0]));
    for(unsigned i=0;i<nodeCount;++i) {
      auto node=nodes.data()+i*0x84;Put(node,0,0x6dcb08);Put(node,0x80,Ptr(pressure.scene));
      Fixture::Vector(node,0x30,repeats.data(),limit);
      if(i+1<nodeCount) {links[i]=Ptr(node+0x84);Put(node,0x58,Ptr(&links[i]));Put(node,0x5c,1);}
    }
    Put(pressure.system,0x1d4,Ptr(nodes.data()));pressure.selection[0]=0;
    Scope scope(Ptr(pressure.scene),0);const auto& capped=scope.RegistrySnapshot();
    Check(capped.valid && capped.nodes==nodeCount && capped.supports.size()==1 &&
      !capped.occurrencesComplete && capped.occurrences.size()==occurrenceLimit,
      "provenance overflow remains bounded and completes the original unique-support walk");
    uint32_t original[3]{};memcpy(original,pressure.manager+0x2c,12);
    scope.ExtendSelection(Ptr(pressure.manager),capped);
    Check(scope.manager==Ptr(pressure.manager) && scope.expanded.size()==3 && scope.expanded[0]==0 &&
      scope.expanded[1]==0 && scope.expanded[2]==Ptr(pressure.objects[0])+0x14,
      "provenance cap does not disable visibility extension or deduplicate original selection slots");
    scope.Restore();Check(!memcmp(original,pressure.manager+0x2c,12),"capped provenance visibility borrow is restored");
  }
  {
    const auto before=MutationSerial();HANDLE threads[2]={CreateThread(nullptr,0,AdvanceRegistryEpoch,nullptr,0,nullptr),
      CreateThread(nullptr,0,AdvanceRegistryEpoch,nullptr,0,nullptr)};
    Check(threads[0] && threads[1],"foreign retirement metadata test threads created");
    Check(WaitForMultipleObjects(2,threads,TRUE,3000)==WAIT_OBJECT_0,"bounded concurrent retirement metadata completes");
    CloseHandle(threads[0]);CloseHandle(threads[1]);
    Check(MutationSerial()==before+2000,"x86 atomic mutation serial loses no concurrent retirement increments");
  }
  Check(active==nullptr,"registry observation leaves no borrowed active scope");
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
  RegistrySnapshots(f);
  printf("PASS %u checks; native allocations untouched; no remaining borrowed scope\n",checks);
}

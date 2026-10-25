#pragma once
// Own identity/lifetime fixture, included by test_native_owner.cpp.
namespace owner_test {
namespace lifetime=native_instance_lifetime;
static lifetime::Token retiringToken;
static void __fastcall CheckInstanceRetired(void* self,void*) {
  MockDestroy(self,nullptr);lifetime::Token rejected;
  Check(!lifetime::Current(retiringToken)&&!lifetime::Borrow(retiringToken.key,rejected),"native destruction cannot resurrect an identity before original returns");
}
static void __fastcall RaiseDestroy(void*,void*) {RaiseException(0xe0525c01u,0,0,nullptr);}
static bool ExceptionalDestroy(void* pointer) {
  __try {source::SkinDestroy(pointer,nullptr);}
  __except(GetExceptionCode()==0xe0525c01u?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
struct ForeignLifetime {HANDLE entered,finish;uint32_t object;};
static DWORD WINAPI ForeignDestroy(void* value) {
  auto& input=*static_cast<ForeignLifetime*>(value);
  lifetime::BeginRetirement();source::Retire(input.object,false,false,true);SetEvent(input.entered);
  WaitForSingleObject(input.finish,3000);lifetime::EndRetirement();return 0;
}
static void Instances(Fixture& fixture) {
  lifetime::Invalidate();
  const lifetime::Key key{Ptr(fixture.scene),Ptr(fixture.objects[0]),Ptr(fixture.models[0]),Ptr(fixture.meshes[0]),1,7,0x13572468};
  lifetime::Token first,again,other;
  Check(lifetime::Borrow(key,first)&&lifetime::Current(first),"qualified identity acquired");
  ++frameId;Check(lifetime::Borrow(key,again)&&again.identity==first.identity,"identity survives frame changes");
  auto changed=key;++changed[1];Check(lifetime::Borrow(changed,other)&&other.identity!=first.identity,"same geometry on another actor remains separate");
  const auto otherId=other.identity;
  source::originalSkinDestroy=reinterpret_cast<source::NativeDestroy>(&CheckInstanceRetired);
  expectedSelf=uint32_t(key[2]);retiringToken=first;const auto calls=originalDestroyCalls;
  source::SkinDestroy(reinterpret_cast<void*>(expectedSelf),nullptr);
  Check(originalDestroyCalls==calls+1&&!lifetime::Current(first)&&!lifetime::Current(other)&&!lifetime::activeRetirements.load(),"shared Skin retirement invalidates all of its actor uses and forwards once");
  Check(lifetime::Borrow(key,again)&&again.identity!=first.identity,"same Skin address after retirement receives new identity");
  Check(lifetime::Borrow(changed,other)&&other.identity!=otherId,"other actor sharing retired Skin also receives new identity");
  source::Retire(uint32_t(key[1]),false,true);
  Check(!lifetime::Current(again)&&lifetime::Current(other),"actor retirement preserves unrelated actor identity");
  Check(lifetime::Borrow(key,again),"actor address can be reused with a fresh identity");
  source::Retire(uint32_t(key[0]),true);
  Check(!lifetime::Current(again)&&!lifetime::Current(other),"scene retirement removes all scene identities");
  Check(lifetime::Borrow(key,first),"fresh scene use after retirement");changed=key;++changed[4];
  Check(lifetime::Borrow(changed,again)&&again.identity!=first.identity,"device reset epoch prevents stale history reuse");
  auto otherDevice=key;++otherDevice[5];Check(lifetime::Borrow(otherDevice,other),"another device has independent identity");
  lifetime::RetireDevice(uintptr_t(key[5]));
  Check(!lifetime::Current(first)&&!lifetime::Current(again)&&lifetime::Current(other),"device retirement clears only its own identity records");
  Check(lifetime::Borrow(key,first)&&lifetime::Borrow(changed,again),"fresh identities after device retirement");
  ForeignLifetime worker{CreateEventW(nullptr,TRUE,FALSE,nullptr),CreateEventW(nullptr,TRUE,FALSE,nullptr),uint32_t(key[2])};
  Check(worker.entered&&worker.finish,"owned synchronization events");
  HANDLE thread=CreateThread(nullptr,0,ForeignDestroy,&worker,0,nullptr);Check(thread!=nullptr,"owned foreign destructor worker");
  Check(WaitForSingleObject(worker.entered,3000)==WAIT_OBJECT_0,"foreign retirement started");
  Check(!lifetime::Current(first)&&!lifetime::Borrow(key,other),"foreign destruction invalidates old snapshots and blocks new identities until completion");
  SetEvent(worker.finish);Check(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0,"foreign destructor completed");
  CloseHandle(thread);CloseHandle(worker.entered);CloseHandle(worker.finish);
  Check(lifetime::Borrow(key,other)&&other.identity!=first.identity&&!lifetime::Current(again),"completed foreign retirement starts new identity history");
  source::originalSkinDestroy=reinterpret_cast<source::NativeDestroy>(&RaiseDestroy);
  Check(ExceptionalDestroy(reinterpret_cast<void*>(uintptr_t(key[2])))&&!lifetime::activeRetirements.load(),"SEH destruction releases active-retirement barrier");
  source::originalSkinDestroy=nullptr;
  // Verify the actual hook witness against owned bytes without patching a game.
  const auto previousPatches=source::installedPatches;const auto previousInstalled=source::lifetimeHooksInstalled;
  std::array<uint32_t,8> slots{};
  for(unsigned i=0;i<slots.size();++i){slots[i]=100+i;auto& patch=source::installedPatches[i];patch={};patch.address=reinterpret_cast<uintptr_t>(&slots[i]);patch.size=4;memcpy(patch.replacement,&slots[i],4);}
  source::lifetimeHooksInstalled=true;
  Check(source::LifetimeInstalled()&&lifetime::Borrow(key,first),"complete installed hook set qualifies an identity");
  slots[7]^=1;Check(!source::LifetimeInstalled()&&!lifetime::Current(first),"lost Skin destructor hook invalidates persistent identities");
  slots[7]^=1;Check(source::LifetimeInstalled()&&lifetime::Borrow(key,again)&&again.identity!=first.identity,"restored hook cannot resurrect history from coverage gap");
  source::installedPatches=previousPatches;source::lifetimeHooksInstalled=previousInstalled;
  lifetime::Invalidate();
  for(size_t i=0;i<lifetime::limit;++i){changed=key;changed[2]=i+1;Check(lifetime::Borrow(changed,again),"bounded registry accepts each distinct Skin");}
  changed[2]=lifetime::limit+1;const auto saved=again;
  Check(!lifetime::Borrow(changed,again)&&again.identity==saved.identity&&again.key==saved.key,"full registry rejects without corrupting previous token");
  lifetime::Invalidate();Check(!lifetime::Current(saved)&&lifetime::instances.empty(),"explicit teardown clears all identity records");
}
} // namespace owner_test

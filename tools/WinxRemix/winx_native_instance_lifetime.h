#pragma once
// Own identity registry. Native addresses are keys only and never dereferenced.
// The caller proves current graph membership and installed destruction hooks.
#include <atomic>
namespace native_instance_lifetime {
using Key=std::array<uint64_t,7>; // scene, owner, Skin, mesh, device epoch/device, primary identity
struct Token {Key key{};uint64_t identity=0,foreignRevision=0;};
enum class Kind {Scene,Owner,Skin};
static std::map<Key,uint64_t> instances;
static uint64_t nextIdentity,synchronizedForeign;
static std::atomic<uint64_t> foreignRevision{0};
static std::atomic<unsigned> activeRetirements{0};
static constexpr size_t limit=8192;
static void BeginRetirement(){activeRetirements.fetch_add(1);}
static void EndRetirement(){activeRetirements.fetch_sub(1);}
static void Retire(uint32_t address,Kind kind,bool ownerThread) {
  // Never wait for render-thread guard on another native thread: the original
  // caller may be waiting for that thread. New borrows clear all old identities
  // after the foreign destructor has returned.
  if(!ownerThread){foreignRevision.fetch_add(1);return;}
  std::lock_guard<std::recursive_mutex> lock(guard);
  const unsigned field=kind==Kind::Scene?0:kind==Kind::Owner?1:2;
  for(auto it=instances.begin();it!=instances.end();) {
    if(it->first[field]==address)it=instances.erase(it);else ++it;
  }
}
static void Invalidate(){std::lock_guard<std::recursive_mutex> lock(guard);instances.clear();}
static void RetireDevice(uintptr_t device) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  for(auto it=instances.begin();it!=instances.end();) {
    if(it->first[5]==device)it=instances.erase(it);else ++it;
  }
}
static bool Borrow(const Key& key,Token& output) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(activeRetirements.load()||!key[0]||!key[1]||!key[2]||!key[3]||!key[5]||!key[6])return false;
  const auto revision=foreignRevision.load();
  if(revision!=synchronizedForeign){instances.clear();synchronizedForeign=revision;}
  try {
    auto found=instances.find(key);
    if(found==instances.end()) {
      if(instances.size()>=limit||nextIdentity==UINT64_MAX)return false;
      found=instances.emplace(key,++nextIdentity).first;
    }
    if(activeRetirements.load()||foreignRevision.load()!=revision)return false;
    output={key,found->second,revision};return true;
  }catch(const std::bad_alloc&){return false;}
}
static bool Current(const Token& input) {
  std::lock_guard<std::recursive_mutex> lock(guard);
  if(!input.identity||activeRetirements.load()||input.foreignRevision!=foreignRevision.load())return false;
  const auto found=instances.find(input.key);
  return found!=instances.end()&&found->second==input.identity&&
    !activeRetirements.load()&&input.foreignRevision==foreignRevision.load();
}
} // namespace native_instance_lifetime

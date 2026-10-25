#pragma once
// Own CP13/PC memory adapter. Reuse bytes only within one synchronous registry
// observation, never across API calls or separate current-state checks.
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace winx_remix::scene_memory {
struct LocalReadStats {
    uint64_t virtualQueryCalls=0,regionCacheHits=0,repeatedRegionQueries=0,regionTrackingOverflows=0;
};
static bool Read(uintptr_t address,void* output,size_t size) {
    SIZE_T got=0;
    return address>=0x10000&&address<0x7fff0000&&size<=16384&&
        size<=0x7fff0000-address&&ReadProcessMemory(GetCurrentProcess(),
            reinterpret_cast<const void*>(address),output,size,&got)&&got==size;
}
// Render-thread local reads for one synchronous observation. No game/API calls
// or protection changes may occur while this window is in use. Keep protection
// metadata only, not object bytes: each current-state check owns a new window.
class LocalReadWindow {
    struct Region {uintptr_t begin=0,end=0;bool readable=false;};
    Region regions_[8];
    unsigned nextRegion_=0;
    // Diagnostic keys never authorize a read. This separate bounded tracker
    // observes repeated queries after the existing eight-slot cache evicts a
    // region. It is discarded with this window, including when a sink is reused.
    struct ObservedRegion {uintptr_t begin,end;};
    ObservedRegion observed_[64];
    unsigned observedCount_=0;
    LocalReadStats* stats_=nullptr;
    void Observe(uintptr_t begin,uintptr_t end) noexcept {
        if(!stats_)return;
        for(unsigned i=0;i<observedCount_;++i)
            if(observed_[i].begin==begin&&observed_[i].end==end){++stats_->repeatedRegionQueries;return;}
        if(observedCount_<64)observed_[observedCount_++]={begin,end};
        else ++stats_->regionTrackingOverflows;
    }
    static bool Copy(uintptr_t address,void* output,size_t size) {
        __try {
            memcpy(output,reinterpret_cast<const void*>(address),size);
            return true;
        } __except((GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION||
                    GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR)?
                   EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {
            return false;
        }
    }
    const Region* Find(uintptr_t address) {
        for(const auto& region:regions_)
            if(address>=region.begin&&address<region.end){if(stats_)++stats_->regionCacheHits;return &region;}
        MEMORY_BASIC_INFORMATION info{};
        if(stats_)++stats_->virtualQueryCalls;
        if(VirtualQuery(reinterpret_cast<const void*>(address),&info,sizeof(info))!=sizeof(info))return nullptr;
        const uintptr_t begin=reinterpret_cast<uintptr_t>(info.BaseAddress);
        if(info.RegionSize>UINTPTR_MAX-begin)return nullptr;
        Observe(begin,begin+info.RegionSize);
        auto& region=regions_[nextRegion_];nextRegion_=(nextRegion_+1)%8;
        const DWORD access=info.Protect&0xff;
        region={begin,begin+info.RegionSize,info.State==MEM_COMMIT&&
            !(info.Protect&PAGE_GUARD)&&(access==PAGE_READONLY||
              access==PAGE_READWRITE||access==PAGE_WRITECOPY||
              access==PAGE_EXECUTE_READ||access==PAGE_EXECUTE_READWRITE||
              access==PAGE_EXECUTE_WRITECOPY)};
        return &region;
    }
public:
    explicit LocalReadWindow(LocalReadStats* stats=nullptr) noexcept :stats_(stats) {}
    bool Read(uintptr_t address,void* output,size_t size) {
        if(address<0x10000||address>=0x7fff0000||size>16384||size>0x7fff0000-address)return false;
        if(!size)return true;
        const uintptr_t end=address+size;
        for(uintptr_t cursor=address;cursor<end;) {
            const auto* region=Find(cursor);
            if(!region||!region->readable||region->end<=cursor)return false;
            cursor=region->end<end?region->end:end;
        }
        return Copy(address,output,size);
    }
};
class ReadWindow {
    static constexpr size_t pageBytes=4096;
    struct Page {uintptr_t address=0;bool valid=false;unsigned char bytes[pageBytes];};
    static constexpr unsigned windowPages=4;
    Page pages_[windowPages];
    unsigned nextPage_=0;
    static bool CompatiblePages() {
        static const bool compatible=[](){SYSTEM_INFO info{};GetSystemInfo(&info);return info.dwPageSize==pageBytes;}();
        return compatible;
    }
public:
    bool Read(uintptr_t address,void* output,size_t size) {
        if(address<0x10000||address>=0x7fff0000||size>16384||size>0x7fff0000-address)return false;
        if(!size||!CompatiblePages())return scene_memory::Read(address,output,size);
        auto destination=static_cast<unsigned char*>(output);
        while(size) {
            const uintptr_t page=address&~uintptr_t(pageBytes-1);
            Page* found=nullptr;
            for(auto& entry:pages_)if(entry.address==page){found=&entry;break;}
            if(!found) {
                // Keep exact reads for scattered records. A small set of
                // page tags also recognizes interleaved nearby allocations.
                auto& entry=pages_[nextPage_];nextPage_=(nextPage_+1)%windowPages;
                entry.address=page;entry.valid=false;
                return scene_memory::Read(address,destination,size);
            }
            if(!found->valid) {
                // The OS keeps protection/guard handling. If the wider read
                // cannot be made, retain the original exact-range fallback.
                if(!scene_memory::Read(page,found->bytes,pageBytes))return scene_memory::Read(address,destination,size);
                found->valid=true;
            }
            const size_t offset=address-page,available=pageBytes-offset;
            const size_t count=size<available?size:available;
            memcpy(destination,found->bytes+offset,count);
            address+=count;destination+=count;size-=count;
        }
        return true;
    }
};
} // namespace winx_remix::scene_memory

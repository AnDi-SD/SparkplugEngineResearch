#pragma once
// Own CP13/PC memory adapter. Reuse bytes only within one synchronous registry
// observation, never across API calls or separate current-state checks.
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace winx_remix::scene_memory {
static bool Read(uintptr_t address,void* output,size_t size) {
    SIZE_T got=0;
    return address>=0x10000&&address<0x7fff0000&&size<=16384&&
        size<=0x7fff0000-address&&ReadProcessMemory(GetCurrentProcess(),
            reinterpret_cast<const void*>(address),output,size,&got)&&got==size;
}
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

#include "Msvcr71Qsort7107031.h"
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace sparkplug::host::msvcr71_7_10_7031_4
{
bool Sort(void* records,std::size_t count,std::size_t width,Compare compare,const void* context)
{
    if(count<2||!width)return true; // 7C38265A / 7C38266B
    if(!records||!compare||count>std::size_t(std::numeric_limits<std::int32_t>::max())/width)return false;
    auto* bytes=static_cast<unsigned char*>(records);
    using Index=std::int64_t; // permit sentinels without forming out-of-array pointers
    auto at=[&](Index index){return bytes+std::size_t(index)*width;};
    auto cmp=[&](Index left,Index right){return compare(context,at(left),at(right));};
    auto swap=[&](Index left,Index right){
        if(left==right)return;
        for(std::size_t i=0;i<width;++i)std::swap(at(left)[i],at(right)[i]);
    };
    std::array<std::pair<Index,Index>,30> pending{};
    std::size_t depth=0;
    Index lo=0,hi=static_cast<Index>(count)-1;
    for(;;)
    {
        const Index size=hi-lo+1;
        if(size<=8) // 7C38269E: max-to-high shortsort, including equal-key swaps
        {
            for(Index end=hi;end>lo;--end)
            {
                Index largest=lo;
                for(Index item=lo+1;item<=end;++item)if(cmp(item,largest)>0)largest=item;
                swap(largest,end);
            }
        }
        else
        {
            Index mid=lo+size/2;
            if(cmp(lo,mid)>0)swap(lo,mid);
            if(cmp(lo,hi)>0)swap(lo,hi);
            if(cmp(mid,hi)>0)swap(mid,hi);
            Index low=lo,high=hi;
            for(;;)
            {
                // 7C382780..7C3827B8: the pivot itself is never compared.
                if(mid>low)do{++low;}while(low<mid&&cmp(low,mid)<=0);
                if(mid<=low)do{++low;}while(low<=hi&&cmp(low,mid)<=0);
                do{--high;}while(high>mid&&cmp(high,mid)>0);
                if(low>high)break;
                swap(low,high);
                if(mid==high)mid=low; // pivot identity follows its swapped record
            }
            // 7C38281C..7C382858: skip equal records around the pivot before
            // scheduling subranges. Alpha's never-zero comparator is retained.
            ++high;
            if(mid<high)do{--high;}while(high>mid&&cmp(high,mid)==0);
            if(mid>=high)do{--high;}while(high>lo&&cmp(high,mid)==0);
            // 7C38286A compares signed byte distances. The guarded extent
            // makes element-distance comparison equivalent without overflow.
            if(high-lo>=hi-low)
            {
                if(lo<high)pending[depth++]={lo,high};
                if(low<hi){lo=low;continue;}
            }
            else
            {
                if(low<hi)pending[depth++]={low,hi};
                if(lo<high){hi=high;continue;}
            }
        }
        // The smaller subrange is processed first; bounded count and cutoff8
        // keep the original thirty-entry pending stack sufficient.
        if(!depth)return true;
        const auto range=pending[--depth];lo=range.first;hi=range.second;
    }
}
}

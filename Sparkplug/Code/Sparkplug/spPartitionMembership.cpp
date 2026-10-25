// Borrowed PC membership bodies shared by PartitionNode, RenderNode and
// OcclusionVolume. Host vector storage substitutes for the native vector ABI.
// Duplicates, first-match swap-last, callback-before-pop and notify=false stale
// opposite links are deliberate original behavior, not repaired implicitly.
#include "spPartitionNode.h"
#include "spRenderNode.h"
#include "spOcclusionVolume.h"
#include <algorithm>

namespace sparkplug::reconstruction
{
    namespace
    {
        template<class T> bool RemoveFirst(std::vector<T*>& values,T* value) noexcept
        {
            const auto at=std::find(values.begin(),values.end(),value);
            if(at==values.end())return false;
            *at=values.back();values.pop_back();return true;
        }
        template<class T> void ReleaseStorage(std::vector<T*>& values) noexcept
        {std::vector<T*>{}.swap(values);}
    }
    bool spPartitionNode::InsertDynamicForAnalysis(spRenderNode* value)
    {
        if(!IsExactly(ClassID)||!value||dynamics_.size()>=4096)return false;
        if(!value->AppendPartitionForAnalysis(this))return false;
        dynamics_.push_back(value);return true;
    }
    bool spPartitionNode::InsertOccluderForAnalysis(spOcclusionVolume* value)
    {
        if(!IsExactly(ClassID)||!value||occluders_.size()>=4096)return false;
        if(!value->AppendPartitionForAnalysis(this))return false;
        occluders_.push_back(value);return true;
    }
    void spPartitionNode::RemoveDynamicForAnalysis(spRenderNode* value,bool notify) noexcept
    {if(RemoveFirst(dynamics_,value)&&notify)value->RemovePartitionForAnalysis(this,false);}
    void spPartitionNode::RemoveOccluderForAnalysis(spOcclusionVolume* value,bool notify) noexcept
    {if(RemoveFirst(occluders_,value)&&notify)value->RemovePartitionForAnalysis(this,false);}
    void spPartitionNode::ClearDynamicsForAnalysis(bool notify) noexcept
    {
        if(!notify){ReleaseStorage(dynamics_);return;}
        while(!dynamics_.empty()){dynamics_.back()->RemovePartitionForAnalysis(this,false);dynamics_.pop_back();}
    }
    void spPartitionNode::ClearOccludersForAnalysis(bool notify) noexcept
    {
        if(!notify){ReleaseStorage(occluders_);return;}
        while(!occluders_.empty()){occluders_.back()->RemovePartitionForAnalysis(this,false);occluders_.pop_back();}
    }
    bool spRenderNode::AppendPartitionForAnalysis(spPartitionNode* partition)
    {
        if(!partition||partitions_.size()>=4096)return false;
        partitions_.push_back(partition);return true;
    }
    void spRenderNode::RemovePartitionForAnalysis(spPartitionNode* partition,bool notify) noexcept
    {if(RemoveFirst(partitions_,partition)&&notify)partition->RemoveDynamicForAnalysis(this,false);}
    void spRenderNode::ClearPartitionsForAnalysis(bool notify) noexcept
    {
        if(!notify){ReleaseStorage(partitions_);return;}
        while(!partitions_.empty()){partitions_.back()->RemoveDynamicForAnalysis(this,false);partitions_.pop_back();}
    }
    bool spOcclusionVolume::AppendPartitionForAnalysis(spPartitionNode* partition)
    {
        if(!partition||partitions_.size()>=4096)return false;
        partitions_.push_back(partition);return true;
    }
    void spOcclusionVolume::RemovePartitionForAnalysis(spPartitionNode* partition,bool notify) noexcept
    {if(RemoveFirst(partitions_,partition)&&notify)partition->RemoveOccluderForAnalysis(this,false);}
    void spOcclusionVolume::ClearPartitionsForAnalysis(bool notify) noexcept
    {
        if(!notify){ReleaseStorage(partitions_);return;}
        while(!partitions_.empty()){partitions_.back()->RemoveOccluderForAnalysis(this,false);partitions_.pop_back();}
    }
    spOcclusionVolume::~spOcclusionVolume(){ClearPartitionsForAnalysis(true);}
}

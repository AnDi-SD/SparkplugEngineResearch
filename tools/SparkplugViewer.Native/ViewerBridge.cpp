#include "ViewerBridge.h"
#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spNodeController.h"
#include "Code/Sparkplug/spSkin.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Analysis/PC/spAnimationMath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace {
using namespace sparkplug::reconstruction;
using Sample = spTransformEval::SampleForAnalysis;
using Sampler = spTransformTrackEval::TrackSamplerForAnalysis;
using Cache = spTransformTrackEval::KeyCacheForAnalysis;
std::mutex gate; // reconstructed RTTI/singletons are not a concurrent host API
thread_local char lastError[2048]{};
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> int guarded(F action) noexcept {
    try { std::lock_guard<std::mutex> lock(gate); lastError[0]=0; action(); return 1; }
    catch(const std::exception& error) { std::snprintf(lastError,sizeof(lastError),"%s",error.what()); }
    catch(...) { std::snprintf(lastError,sizeof(lastError),"Unknown native viewer failure"); }
    return 0;
}
template<std::size_t N> std::array<float,N> values(const float* input) {
    std::array<float,N> result{};
    for(std::size_t i=0;i<N;++i) { require(std::isfinite(input[i]),"Non-finite transform"); result[i]=input[i]; }
    return result;
}
struct Clip { std::shared_ptr<spAnimation> animation; };
// Host memory adapter: borrows one pinned C# span without copying the SMO or
// allocating a stream buffer per field. All field grammar stays in Sparkplug.
class BorrowedInput final : public spStream {
    const std::uint8_t* data_; std::uint32_t size_, position_ = 0;
public:
    BorrowedInput(const std::uint8_t* data, std::uint32_t size) : data_(data), size_(size) {}
    bool Open(const char*) override { return false; }
    bool Open(std::uint32_t, const char*) override { return false; }
    bool Close() override { return false; }
    bool Seek(SeekSource source, std::int32_t offset) override {
        std::int64_t base = source == SeekSource::essStart ? 0 : source == SeekSource::essCurrent ? position_ : size_;
        if(source != SeekSource::essStart && source != SeekSource::essCurrent && source != SeekSource::essEnd) return false;
        const auto next=base+offset; if(next<0 || next>size_) return false;
        position_=static_cast<std::uint32_t>(next); return true;
    }
    bool GetCurrentPosition(std::uint32_t& position) const override { position=position_; return true; }
    bool GetSize(std::uint32_t* size) const override { if(!size) return false; *size=size_; return true; }
    bool ReadData(void* destination, std::uint32_t count) override {
        if(count>size_-position_ || (!destination && count)) return false;
        if(count) std::memcpy(destination,data_+position_,count); position_+=count; return true;
    }
    bool WriteData(const void*, std::uint32_t) override { return false; }
    bool vfunc_WriteFromStream(spStream*, std::uint32_t) override { return false; }
};
struct Binding {
    Sampler sampler;
    spTransformTrackEval::PlaybackForAnalysis playback;
    spNodeController controller;
};
struct Scene {
    std::vector<SpvNode> initial;
    std::vector<std::shared_ptr<spNode>> nodes;
    std::shared_ptr<spAnimation> animation;
    std::vector<std::unique_ptr<Binding>> bindings;
    void reset() {
        for(std::size_t i=0;i<nodes.size();++i) {
            nodes[i]->SetPositionForAnalysis(values<3>(initial[i].position));
            nodes[i]->SetScaleForAnalysis(values<3>(initial[i].scale));
            nodes[i]->SetOrientationForAnalysis(sparkplug::evidence::pc::animation_math::ToMatrix(values<4>(initial[i].rotation)));
            nodes[i]->MarkLocalTransformDirtyForAnalysis();
        }
    }
};
Scene& scene(void* handle) { require(handle!=nullptr,"Null scene handle"); return *static_cast<Scene*>(handle); }
Clip& clip(void* handle) { require(handle!=nullptr,"Null clip handle"); return *static_cast<Clip*>(handle); }
std::vector<float> keyTimes(const spAnimTrack& track, std::uint32_t role) {
    require(role<3,"Invalid PRS role");
    const auto* keys=track.GetKeysForAnalysis(); require(keys!=nullptr,"Missing prepared track");
    std::vector<float> times;
    for(const auto& axis:(*keys)[role]) if(axis) times.insert(times.end(),axis->times.begin(),axis->times.end());
    std::sort(times.begin(),times.end()); times.erase(std::unique(times.begin(),times.end()),times.end());
    return times;
}
}
SPV_API std::uint32_t spv_abi_version() noexcept { return 2; }
SPV_API const char* spv_last_error() noexcept { return lastError; }
SPV_API int spv_read_field(const std::uint8_t* data, std::uint32_t count, SpvFieldHeader* output) noexcept {
    return guarded([&]{
        require(data && count && output,"Missing field input/output");
        BorrowedInput source(data,count); spDataBlockSerializer serializer;
        const auto* header=serializer.ReadHeaderForAnalysis(source);
        require(header && header->dataStreamPosition<=count && header->payloadSize<=count-header->dataStreamPosition,"Truncated field");
        // The inspector also displays the encoded ID of a section terminator.
        // The engine has already validated its bytes and treats its ID as -1.
        const auto field=header->IsTerminator() ? ((data[0]&31)==31 ? data[1] : data[0]&31) : header->fieldID;
        *output={field,header->payloadSize,header->dataStreamPosition};
    });
}
SPV_API int spv_node_local(const SpvNode* input, float* output, std::uint32_t count) noexcept {
    return guarded([&]{
        require(input && output && count==16,"Invalid local matrix input/output");
        spNode node;
        node.SetPositionForAnalysis(values<3>(input->position));
        node.SetScaleForAnalysis(values<3>(input->scale));
        node.SetOrientationForAnalysis(sparkplug::evidence::pc::animation_math::ToMatrix(values<4>(input->rotation)));
        node.MarkLocalTransformDirtyForAnalysis();
        require(node.UpdateWorldForAnalysis(),"Cannot update local node");
        const auto matrix=node.GetWorldMatrixForAnalysis(); std::copy(matrix.begin(),matrix.end(),output);
    });
}
SPV_API int spv_skin_matrix(const float* inverseBind, const float* world, float* output, std::uint32_t count) noexcept {
    return guarded([&]{
        require(inverseBind && world && output && count==16,"Invalid skin matrix input/output");
        const auto matrix=spSkin::ComposePaletteMatrixForAnalysis(values<16>(inverseBind),values<16>(world));
        std::copy(matrix.begin(),matrix.end(),output);
    });
}
SPV_API void* spv_scene_create(const SpvNode* input, std::uint32_t count) noexcept {
    std::unique_ptr<Scene> result;
    if(!guarded([&]{
        require(count<=16384 && (input||!count),"Invalid node array (maximum 16384)");
        result=std::make_unique<Scene>();
        if(count) result->initial.assign(input,input+count);
        result->nodes.reserve(count);
        // Validate the entire forest before creating owning child links.
        for(std::uint32_t i=0;i<count;++i) {
            auto parent=input[i].parent; std::uint32_t depth=0;
            while(parent!=-1) {
                require(parent>=0 && static_cast<std::uint32_t>(parent)<count,"Invalid parent ordinal");
                require(parent!=static_cast<std::int32_t>(i) && ++depth<=256,"Cyclic or overdeep node hierarchy");
                parent=input[parent].parent;
            }
            const auto q=values<4>(input[i].rotation);
            double norm=0; for(auto v:q) norm+=double(v)*v;
            require(std::abs(norm-1)<0.01,"Expected unit node quaternion");
            require(input[i].billboard<=2,"Unsupported billboard axis");
            auto node=std::make_shared<spNode>();
            node->SetBillboardAxisForAnalysis(input[i].billboard);
            result->nodes.push_back(std::move(node));
        }
        result->reset();
        for(std::uint32_t i=0;i<count;++i)
            if(input[i].parent>=0) require(result->nodes[input[i].parent]->AttachChildForAnalysis(result->nodes[i]),"Cannot attach node");
    })) return nullptr;
    return result.release();
}
SPV_API void spv_scene_destroy(void* handle) noexcept { guarded([&]{ delete static_cast<Scene*>(handle); }); }
SPV_API void* spv_clip_load(const std::uint8_t* data, std::uint32_t count) noexcept {
    std::unique_ptr<Clip> result;
    if(!guarded([&]{
        require(data && count>=36 && count<=64*1024*1024,"SAN must fit 64 MiB");
        std::uint32_t objects=0; std::memcpy(&objects,data+28,4);
        require(objects==1,"Viewer requires a single spAnimation SAN");
        spSerializerManager manager; spResourceManager resources;
        require(manager.RegisterForAnalysis(spAnimation::ClassID,std::make_shared<spAnimationSerializer>(
            spAnimationSerializer::KeyPoolPolicyForAnalysis::AllowMissingWithOwnedKeys),255,1),"Cannot register animation serializer");
        spSerializerReadContextForAnalysis context(manager,resources);
        spMemoryStream stream; require(stream.ResizeAndSetSize(count),"Cannot allocate SAN stream");
        std::memcpy(stream.GetBuffer(),data,count);
        std::string error;
        auto* object=manager.LoadResourcesForAnalysis(stream,context,&error);
        require(object && !context.failed,error.c_str());
        auto animation=std::dynamic_pointer_cast<spAnimation>(context.ShareObjectForAnalysis(object));
        require(animation && animation->GetTrackCountForAnalysis()>0,"SAN has no animation tracks");
        std::uint32_t position=0;
        require(stream.GetCurrentPosition(position) && position+stream.GetLogicalOriginForAnalysis()==count,"Unaccounted SAN tail");
        result=std::make_unique<Clip>(); result->animation=std::move(animation);
    })) return nullptr;
    return result.release();
}
SPV_API void spv_clip_destroy(void* handle) noexcept { guarded([&]{ delete static_cast<Clip*>(handle); }); }
SPV_API int spv_clip_info(void* handle, float* duration, std::uint32_t* count) noexcept {
    return guarded([&]{ require(duration && count,"Null clip info output"); auto& a=*clip(handle).animation;
        *duration=a.GetTotalTimeForAnalysis(); *count=static_cast<std::uint32_t>(a.GetTrackCountForAnalysis()); });
}
SPV_API int spv_clip_track(void* handle,std::uint32_t index,char* name,std::uint32_t capacity,SpvTrackInfo* info) noexcept {
    return guarded([&]{
        auto* track=clip(handle).animation->GetTrackForAnalysis(index);
        require(track && name && capacity && info,"Invalid track output");
        const char* source=track->GetName(); if(!source) source="";
        const auto size=std::strlen(source); require(size<capacity,"Track name output too small");
        std::memcpy(name,source,size+1); *info={};
        std::uint32_t* counts[]={&info->positionKeys,&info->rotationKeys,&info->scaleKeys};
        for(std::uint32_t role=0;role<3;++role) *counts[role]=static_cast<std::uint32_t>(keyTimes(*track,role).size());
    });
}
SPV_API int spv_clip_channel(void* handle,std::uint32_t ordinal,std::uint32_t role,SpvChannelInfo* info) noexcept {
    return guarded([&]{
        auto* track=clip(handle).animation->GetTrackForAnalysis(ordinal);
        require(track && role<3 && info,"Invalid channel output"); *info={};
        const auto* keys=track->GetKeysForAnalysis(); require(keys!=nullptr,"Missing prepared track");
        for(std::size_t axis=0;axis<3;++axis) if(const auto& key=(*keys)[role][axis]) {
            ++info->axes; info->sourceKeys+=static_cast<std::uint32_t>(key->times.size());
            info->representations[axis]=key->representation;
        }
        info->uniqueTimes=static_cast<std::uint32_t>(keyTimes(*track,role).size());
    });
}
SPV_API int spv_clip_times(void* handle,std::uint32_t ordinal,std::uint32_t role,float* output,std::uint32_t count) noexcept {
    return guarded([&]{
        auto* track=clip(handle).animation->GetTrackForAnalysis(ordinal); require(track!=nullptr,"Invalid track ordinal");
        const auto times=keyTimes(*track,role); require(count==times.size() && (output||!count),"Invalid key-time output size");
        if(count) std::memcpy(output,times.data(),times.size()*sizeof(float));
    });
}
SPV_API void* spv_clip_create_linear(const SpvLinearChannel* channels,std::uint32_t count) noexcept {
    std::unique_ptr<Clip> result;
    if(!guarded([&]{
        require(channels && count==3,"Expected three linear PRS channels");
        spAnimTrack::TrackDataForAnalysis data;
        for(std::size_t role=0;role<3;++role) {
            const auto& input=channels[role]; const std::size_t width=role==1?4:3;
            require(input.count<=100000 && (!input.count||(input.times && input.values)),"Invalid linear channel input");
            if(!input.count) continue;
            for(std::uint32_t i=0;i<input.count;++i) require(std::isfinite(input.times[i]) &&
                (!i||input.times[i]>=input.times[i-1]),"Linear key times must be finite and nondecreasing");
            sparkplug::evidence::pc::animation_keys::KeyDataForAnalysis key;
            key.times.assign(input.times,input.times+input.count);
            key.values.assign(input.values,input.values+input.count*width);
            data[role][0]=std::move(key);
        }
        result=std::make_unique<Clip>(); result->animation=std::make_shared<spAnimation>();
        require(result->animation->ResizeTrackCapacityForAnalysis(1),"Cannot allocate linear track");
        auto* track=result->animation->AppendTrackForAnalysis();
        require(track && track->SetKeysForAnalysis(std::move(data)),"Invalid linear PRS keys");
        require(result->animation->SetTotalTimeForAnalysis(track->GetDurationForAnalysis()),"Invalid linear clip duration");
    })) return nullptr;
    return result.release();
}
SPV_API int spv_scene_bind(void* handle,void* clipHandle,const std::int32_t* roles,std::uint32_t count) noexcept {
    return guarded([&]{
        auto& s=scene(handle); auto animation=clip(clipHandle).animation;
        require(count==s.nodes.size()*3 && (roles||!count),"Invalid role binding array");
        std::vector<std::unique_ptr<Binding>> next; next.reserve(s.nodes.size());
        for(std::size_t i=0;i<s.nodes.size();++i) {
            std::array<Sampler,3> samplers;
            std::array<std::int32_t,3> ordinals{};
            for(unsigned role=0;role<3;++role) {
                auto ordinal=roles[i*3+role]; require(ordinal>=-1,"Invalid track ordinal");
                ordinals[role]=ordinal;
                if(ordinal<0) continue;
                auto* track=animation->GetTrackForAnalysis(ordinal); require(track!=nullptr,"Track ordinal outside clip");
                samplers[role]=track->GetSamplerForAnalysis();
            }
            auto binding=std::make_unique<Binding>();
            binding->sampler=[samplers,ordinals,caches=std::array<Cache,3>{}](float time,Cache&) mutable {
                Sample result;
                std::array<Sample,3> sampled;
                for(unsigned role=0;role<3;++role) if(samplers[role]) {
                    unsigned previous=0;
                    while(previous<role && ordinals[previous]!=ordinals[role]) ++previous;
                    auto sample=sampled[role]=previous<role ? sampled[previous] : samplers[role](time,caches[role]);
                    if(role==0) {result.position=sample.position;result.hasPosition=sample.hasPosition;}
                    if(role==1) {result.rotation=sample.rotation;result.hasRotation=sample.hasRotation;}
                    if(role==2) {result.scale=sample.scale;result.hasScale=sample.hasScale;}
                }
                return result;
            };
            binding->controller.SetNodeForAnalysis(s.nodes[i]);
            auto* evaluator=dynamic_cast<spTransformTrackEval*>(binding->controller.GetEvaluatorForAnalysis());
            require(evaluator && evaluator->SetInputsForAnalysis({{&binding->playback,&binding->sampler,0xFFFFFFFF,{}}}),"Cannot bind transform evaluator");
            next.push_back(std::move(binding));
        }
        s.bindings=std::move(next); s.animation=std::move(animation); s.reset();
    });
}
SPV_API int spv_clip_sample(void* handle,std::uint32_t ordinal,float time,SpvSample* output) noexcept {
    return guarded([&]{
        require(output && std::isfinite(time),"Invalid sample output/time");
        auto* track=clip(handle).animation->GetTrackForAnalysis(ordinal); require(track!=nullptr,"Invalid sample track");
        Cache cache; auto sample=track->GetSamplerForAnalysis()(time,cache);
        std::copy(sample.position.begin(),sample.position.end(),output->position);
        std::copy(sample.rotation.begin(),sample.rotation.end(),output->rotation);
        std::copy(sample.scale.begin(),sample.scale.end(),output->scale);
        output->validRoles=(sample.hasPosition?1:0)|(sample.hasRotation?2:0)|(sample.hasScale?4:0);
    });
}
SPV_API int spv_scene_sample(void* handle,float time,float* output,std::uint32_t floats) noexcept {
    return guarded([&]{
        auto& s=scene(handle); require(std::isfinite(time),"Non-finite sample time");
        require(floats==s.nodes.size()*16 && (output||!floats),"Invalid world output length");
        // Direct seeking starts from authored rest PRS, independent of previous frames.
        s.reset();
        for(auto& binding:s.bindings) { binding->playback.time=time; binding->controller.ApplyForAnalysis(time); }
        for(auto& node:s.nodes) if(!node->GetParentForAnalysis())
            require(node->UpdateWorldForAnalysis(),"World update failed");
        for(std::size_t i=0;i<s.nodes.size();++i) {
            auto matrix=s.nodes[i]->GetWorldMatrixForAnalysis();
            for(auto value:matrix) require(std::isfinite(value),"Non-finite world result");
            std::memcpy(output+i*16,matrix.data(),64);
        }
    });
}
SPV_API int spv_scene_palette(void* handle,const SpvBone* bones,std::uint32_t count,float* output,std::uint32_t floats) noexcept {
    return guarded([&]{
        auto& s=scene(handle); require(count<=65536 && floats==count*16 && (count==0||(bones&&output)),"Invalid palette array");
        for(std::uint32_t i=0;i<count;++i) {
            require(bones[i].node>=0 && static_cast<std::size_t>(bones[i].node)<s.nodes.size(),"Palette references a missing node");
            auto matrix=spSkin::ComposePaletteMatrixForAnalysis(values<16>(bones[i].inverseBind),s.nodes[bones[i].node]->GetWorldMatrixForAnalysis());
            for(auto value:matrix) require(std::isfinite(value),"Non-finite palette result");
            std::memcpy(output+i*16,matrix.data(),64);
        }
    });
}

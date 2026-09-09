#include "ViewerBridge.h"
#include "ResourceGraph.h"
#include "RenderMeshView.h"
#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spNodeController.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/Sparkplug/spPartitionRenderable.h"
#include "Code/Sparkplug/spOctreeNode.h"
#include "Code/Sparkplug/spOctreeNodeSerializer.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spSkyBox.h"
#include "Code/Sparkplug/spLensFlare.h"
#include "Code/Sparkplug/spParticleSystem.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spFontSerializer.h"
#include "Code/Sparkplug/spSphereBV.h"
#include "Code/Sparkplug/spSphereBVSerializer.h"
#include "Code/Sparkplug/spBoxBV.h"
#include "Code/Sparkplug/spBoxBVSerializer.h"
#include "Code/Sparkplug/spOBBBV.h"
#include "Code/Sparkplug/spOBBBVSerializer.h"
#include "Code/Sparkplug/spStaticRenderObjectSerializer.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spSkin.h"
#include "Code/Sparkplug/spSkinSerializer.h"
#include "Code/Sparkplug/spAnimTexController.h"
#include "Code/Sparkplug/spUVController.h"
#include "Code/Sparkplug/spAnimTexControllerSerializer.h"
#include "Code/Sparkplug/spTransFunctionEval.h"
#include "Code/Sparkplug/spTransFunctionEvalSerializer.h"
#include "Code/Sparkplug/spMatColorControllerSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Code/Sparkplug/spDataBlockSerializer.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spMeshBV.h"
#include "Code/Sparkplug/spMeshBVSerializer.h"
#include "Code/Sparkplug/spPS2MeshDataSerializer.h"
#include "Code/Sparkplug/spDXMeshDataSerializer.h"
#include "Code/Sparkplug/spMeshData.h"
#include "Code/Sparkplug/spCollisionInfoSerializer.h"
#include "Code/Sparkplug/spCollisionInfo.h"
#include "Code/Sparkplug/spDXTextureDataSerializer.h"
#include "Code/Sparkplug/spTextureBuffer.h"
#include "Code/wxFaceData.h"
#include "Analysis/PC/spAnimationMath.h"
#include "Analysis/PC/spTextureResizeFilter.h"
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
SpvMaterialReference referenceForView(const std::optional<sparkplug::evidence::pc::serialization::InspectedReference>& value) {
    return value?SpvMaterialReference{value->offset,value->size}:SpvMaterialReference{};
}
SpvFunctionInfo functionForView(const spFunctionEval& value) {
    const auto& state=value.GetStateForAnalysis();
    return {state.functionType,state.frequency,state.amplitude,state.xOffset,state.yOffset,state.pitch};
}
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
// Small host memory backend for the original scalar header writer. No game
// fields are decoded here and no allocation is needed per edited header.
class HeaderOutput final : public spStream {
    std::uint8_t* bytes_;std::uint32_t capacity_,position_=0;
public:
    HeaderOutput(std::uint8_t* bytes,std::uint32_t capacity):bytes_(bytes),capacity_(capacity){}
    bool Open(const char*) override{return false;}
    bool Open(std::uint32_t,const char*) override{return false;}
    bool Close() override{return false;}
    bool Seek(SeekSource,std::int32_t) override{return false;}
    bool GetCurrentPosition(std::uint32_t& value) const override{value=position_;return true;}
    bool GetSize(std::uint32_t* value) const override{if(!value)return false;*value=position_;return true;}
    bool ReadData(void*,std::uint32_t) override{return false;}
    bool WriteData(const void* bytes,std::uint32_t size) override{
        if(size>capacity_-position_||(!bytes&&size))return false;
        if(size)std::memcpy(bytes_+position_,bytes,size);position_+=size;return true;
    }
    bool vfunc_WriteFromStream(spStream*,std::uint32_t) override{return false;}
};
struct ContainerIndex {
    SpvContainerInfo info{};
    std::vector<SpvContainerEntry> entries;
    ContainerIndex(const std::uint8_t* data,std::uint32_t count) {
        require(data&&count>=36&&count<=64u*1024u*1024u,"FFPS inspection requires 36 bytes..64 MiB");
        BorrowedInput input(data,count);spSerializerManager manager;spSerializerFileHeader header{};
        const auto status=manager.ReadAndValidateHeaderForAnalysis(input,
            spSerializerManager::PlatformPC|spSerializerManager::PlatformPS2,&header);
        require(status!=spSerializerFileHeaderStatus::HeaderReadFailed
            &&status!=spSerializerFileHeaderStatus::WrongFileType,"The file does not start with the FFPS signature");
        // A raw inspector may display wrong-version/platform/size documents.
        // Return native header status explicitly; never claim a runtime load.
        info={header.signature,header.version,header.exportTag,header.declaredFileSize,
            header.platformMask,header.dataOffset,header.dataSize,0,static_cast<std::uint32_t>(status)};
        require(header.dataOffset>=36&&header.dataOffset<=count,"FFPS table is outside the file");
        BorrowedInput table(data,header.dataOffset-4);
        require(table.Seek(spStream::SeekSource::essStart,sizeof(header)),"Cannot seek to FAT index");
        const bool read=spResourceFATHelperForAnalysis::ReadIndexEntriesForAnalysis(table,
            [&](auto entry,const auto& location) {
                SpvContainerEntry result{location.tableOffset,entry->id,location.nameOffset,location.nameBytes,
                    entry->classID,entry->offset,entry->size,0,0};
                const std::uint64_t physical=std::uint64_t(header.dataOffset)+entry->offset;
                if(physical<=count-8) {
                    spSerializerObjectHeaderForAnalysis objectHeader{};
                    require(input.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(physical))
                        &&spSerializer::ReadObjectHeaderForAnalysis(input,objectHeader),"Cannot inspect bounded object header");
                    result.signatureClassID=objectHeader.classID;
                    result.signatureFlags=1u|(spSerializer::HasCanonicalObjectMarkerForAnalysis(objectHeader)?2u:0u)
                        |(objectHeader.classID==entry->classID?4u:0u);
                }
                entries.push_back(result);return true;
            },true,&info.objectCount);
        require(read,"Truncated or oversized resource FAT index");
        std::uint32_t position=0,fileCount=0;
        require(table.GetCurrentPosition(position)&&position==header.dataOffset-4,"FAT table does not end at the declared terminator");
        require(input.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(position))
            &&input.Read(fileCount)&&fileCount==0,"Expected zero external-file index count");
    }
};
static_assert(sizeof(SpvContainerInfo)==36&&sizeof(SpvContainerEntry)==36);
struct TextureSectionView {
    SpvTextureSectionInfo info{};
    std::uint32_t field1C=0;
    std::vector<SpvTextureMip> mips;
    std::vector<std::byte> xrgb;
    TextureSectionView(const std::uint8_t* bytes,std::uint32_t size,std::uint32_t kind) {
        require(bytes&&size&&size<=16u*1024u*1024u&&kind<=1,"Invalid bounded texture section");
        BorrowedInput input(bytes,size);spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);std::string error;
        if(kind==0) {
            bool initialized=false;
            if(!spTextureDataSerializer::ReadCrossSectionForAnalysis(context,input,size,
                [](const spTextureBuffer&){return true;},initialized,&error,[&](const spTextureBuffer& buffer,std::uint32_t offset){
                    const auto w=buffer.GetWidthForAnalysis(),h=buffer.GetHeightForAnalysis();
                    const auto pixelSize=buffer.GetPixelSizeForAnalysis();
                    info={0,w,h,buffer.GetPixelFormatForAnalysis(),pixelSize,pixelSize*8,1,1};
                    mips={{w,h,w,std::uint32_t(w)*pixelSize,h,offset,static_cast<std::uint32_t>(buffer.GetBufferForAnalysis().size())}};
                    xrgb.clear();if(info.format==1)xrgb=buffer.GetBufferForAnalysis();
                }))throw std::runtime_error(error);
            require(initialized,"Cross texture has no pixel field");
        } else {
            spDXTextureDataSerializer::NativeReadForAnalysis observed;
            if(!spDXTextureDataSerializer::ReadNativeSectionForAnalysis(context,input,size,observed,&error))throw std::runtime_error(error);
            field1C=observed.field1C;
            info={1,observed.width,observed.height,observed.flags,0,observed.flags==0?32u:observed.flags==1?4u:8u,
                observed.field1C!=0,static_cast<std::uint32_t>(observed.mips.size())};
            for(std::size_t i=0;i<observed.mips.size();++i) {
                const auto& mip=observed.mips[i];
                mips.push_back({mip.width,mip.height,mip.width,mip.rowBytes,mip.rows,observed.pixelOffsets[i],
                    static_cast<std::uint32_t>(mip.packedBytes.size())});
            }
        }
    }
};
struct MaterialView {
    SpvMaterialInfo info{};
    std::vector<SpvMaterialLayer> layers;
    std::vector<SpvMaterialPass> passes;
    MaterialView(const std::uint8_t* bytes,std::uint32_t size) {
        require(bytes&&size&&size<=16u*1024u*1024u,"Invalid bounded material field stream");
        BorrowedInput input(bytes,size);spDXMaterial material;
        spMaterialSerializer::InspectionForAnalysis observed;std::string error;
        if(!spMaterialDataSerializer{}.InspectPayloadForAnalysis(input,size,material,observed,&error))throw std::runtime_error(error);
        const auto& states=material.GetRenderStatesForAnalysis();std::copy(states.begin(),states.end(),info.states);
        info.vertexAlpha=material.GetVertexAlphaByteForAnalysis();
        if(observed.color) {
            const auto& c=*observed.color;info.hasColor=1;
            info.colors[0]=c.ambient;info.colors[1]=c.diffuse;info.colors[2]=c.specular;info.colors[3]=c.emissive;info.power=c.power;
        }
        info.colorController=referenceForView(observed.colorController);
        info.passes=static_cast<std::uint32_t>(material.GetPassCountForAnalysis());
        for(std::uint32_t i=0;i<info.passes;++i) {
            const auto* pass=dynamic_cast<const spMaterialPassLayer*>(material.GetPassForAnalysis(i));
            require(pass!=nullptr,"Material pass has no supported view");
            passes.push_back({pass->GetFinalBlendOperationForAnalysis(),static_cast<std::uint32_t>(pass->GetLayerCountForAnalysis())});
            for(std::uint32_t j=0;j<pass->GetLayerCountForAnalysis();++j) {
                const auto* layer=dynamic_cast<const spStdLayer*>(pass->GetLayerForAnalysis(j).get());
                require(layer&&layer->GetMaterialTextureForAnalysis(),"Material layer has no supported texture holder");
                const auto* holder=layer->GetMaterialTextureForAnalysis().get();
                SpvMaterialLayer result{};result.pass=i;result.index=j;result.classID=layer->vfunc_18().classID;
                result.blend=pass->GetFinalBlendOperationForAnalysis();result.statesField=-1;
                const auto& textureStates=holder->GetTextureStatesForAnalysis();std::copy_n(textureStates.begin(),9,result.states);
                result.uvEnabled=holder->HasStaticTransformForAnalysis();
                const auto& matrix=holder->GetUVTransformForAnalysis();std::copy(matrix.begin(),matrix.end(),result.uvMatrix);
                const auto at=observed.layers.find(holder);
                if(at!=observed.layers.end()) {
                    const auto& fields=at->second;result.statesField=fields.textureStatesField;result.hasUV=fields.hasUVField;
                    result.texture=referenceForView(fields.references[0]);result.animation=referenceForView(fields.references[1]);result.uvController=referenceForView(fields.references[2]);
                }
                layers.push_back(result);
            }
        }
        info.layers=static_cast<std::uint32_t>(layers.size());
    }
};
static_assert(sizeof(SpvMaterialReference)==8&&sizeof(SpvMaterialInfo)==88&&sizeof(SpvMaterialLayer)==124);
struct ModelView {
    SpvModelInfo info{};
    std::vector<SpvSkinBone> bones;
    std::vector<SpvSkinPaletteField> paletteFields;
    ModelView(const std::uint8_t* bytes,std::uint32_t size,std::uint32_t kind) {
        require(bytes&&size&&size<=16u*1024u*1024u&&kind<=1,"Invalid bounded Model/Skin field stream");
        BorrowedInput input(bytes,size);std::string error;
        std::unique_ptr<spModel> partial;spModelSerializer::InspectionForAnalysis observed;
        if(kind==0) {
            partial=std::make_unique<spModel>();
            if(!spModelSerializer{}.InspectPayloadForAnalysis(input,size,*partial,observed,&error))throw std::runtime_error(error);
        } else {
            auto skin=std::make_unique<spSkin>();spSkinSerializer::InspectionForAnalysis result;
            if(!spSkinSerializer{}.InspectPayloadForAnalysis(input,size,*skin,result,&error))throw std::runtime_error(error);
            info.skinMask=result.fieldMask;info.weights=result.fieldMask?result.weights:skin->GetWeightCountForAnalysis();
            for(const auto& field:result.paletteFields)
                paletteFields.push_back({field.headerOffset,field.payloadOffset,field.payloadSize,field.assignmentOrder});
            bones.reserve(result.bones.size());
            for(const auto& binding:result.bones) {
                SpvSkinBone bone{};bone.reference={binding.reference.offset,binding.reference.size};
                bone.id=binding.reference.id;bone.inlineSize=binding.reference.inlineSize;
                std::copy(binding.inverseBind.begin(),binding.inverseBind.end(),bone.inverseBind);bones.push_back(bone);
            }
            observed=std::move(result.model);partial=std::move(skin);
        }
        info.alpha=partial->IsAlphaSortEnabledForAnalysis();info.priority=partial->GetPriorityForAnalysis();
        info.projection=partial->GetProjectionGroupForAnalysis();info.bones=static_cast<std::uint32_t>(bones.size());
        info.renderableMask=observed.renderable.fieldMask;info.modelMask=observed.fieldMask;
        info.material=referenceForView(observed.renderable.material);info.fog=referenceForView(observed.renderable.fog);info.mesh=referenceForView(observed.mesh);
    }
};
static_assert(sizeof(SpvModelInfo)==56&&sizeof(SpvSkinBone)==80);
static_assert(sizeof(SpvSkinPaletteField)==16&&sizeof(SpvSkinPaletteBinding)==68);
struct AnimTextureView {
    SpvAnimTextureInfo info{};
    std::vector<SpvAnimTextureFrame> frames;
    spTextureTrack track; // actual key selector; resource slots explicitly unresolved
    AnimTextureView(const std::uint8_t* bytes,std::uint32_t count) {
        require(bytes&&count&&count<=16u*1024u*1024u,"Invalid bounded animated texture stream");
        BorrowedInput input(bytes,count);spAnimTexController partial;
        spAnimTexControllerSerializer::InspectionForAnalysis observed;std::string error;
        if(!spAnimTexControllerSerializer{}.InspectPayloadForAnalysis(input,count,partial,observed,&error))throw std::runtime_error(error);
        const auto& times=partial.GetTextureTrackForAnalysis().GetTimesForAnalysis();
        require(times.size()==observed.textures.size(),"Inspected animated texture track has inconsistent slots");
        info={static_cast<std::uint32_t>(times.size()),observed.hasTrack,partial.GetTextureTrackForAnalysis().GetDurationForAnalysis()};
        for(std::size_t i=0;i<times.size();++i)frames.push_back({times[i],{observed.textures[i].offset,observed.textures[i].size}});
        require(track.SetKeysForAnalysis(times,std::vector<std::shared_ptr<spTexture>>(times.size())),"Cannot retain inspected texture timeline");
    }
};
static_assert(sizeof(SpvAnimTextureInfo)==12&&sizeof(SpvAnimTextureFrame)==12);
static_assert(sizeof(SpvTextureSectionInfo)==32&&sizeof(SpvTextureMip)==28);
struct SerializedBytes {
    std::vector<std::uint8_t> bytes;
    explicit SerializedBytes(spMemoryStream& source) {
        std::uint32_t size=0;require(source.GetSize(&size)&&size&&source.GetBuffer(),"Empty serialized output");
        const auto* data=static_cast<const std::uint8_t*>(source.GetBuffer());bytes.assign(data,data+size);
    }
};
// Host same-length edit shared by typed writer adapters. Field interpretation
// stays with the actual reader and writer; no header grammar is repeated here.
void PatchObservedFieldPayload(spMemoryStream& destination,std::uint32_t sourceSize,
    std::uint32_t field,std::uint32_t payloadOffset,std::uint32_t payloadSize,
    spMemoryStream& encoded) {
    require(encoded.Seek(spStream::SeekSource::essStart,0),"Cannot rewind written scalar field");
    spDataBlockSerializer blocks;
    const auto* header=blocks.ReadHeaderForAnalysis(encoded);
    std::uint32_t encodedSize=0;
    require(header&&header->fieldID==field&&header->payloadSize==payloadSize&&
        encoded.GetSize(&encodedSize)&&std::uint64_t(header->dataStreamPosition)+header->payloadSize==encodedSize&&
        std::uint64_t(payloadOffset)+payloadSize<=sourceSize,
        "SCALAR_AUTHORING: shared writer output differs from observed field extent");
    require(destination.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(payloadOffset))&&
        destination.WriteData(static_cast<const std::uint8_t*>(encoded.GetBuffer())+header->dataStreamPosition,payloadSize),
        "Cannot apply observed scalar payload");
}
struct MeshBVView {
    std::unique_ptr<spMeshBV> mesh;
    std::unique_ptr<spFaceDataContainer> standaloneFaces;
    SpvMeshBVInfo info{};
    const spFaceDataContainer* faces() const {
        if(standaloneFaces)return standaloneFaces.get();
        const auto* data=mesh?mesh->GetDataForAnalysis():nullptr;
        return data?data->GetFacesForAnalysis():nullptr;
    }
    MeshBVView(const std::uint8_t* bytes,std::uint32_t size,std::uint32_t kind) {
        require(bytes&&size&&size<=16u*1024u*1024u&&kind<=2,"Invalid bounded MeshBV inspection input");
        (void)winx::reconstruction::wxFaceData::StaticRTTI();
        BorrowedInput input(bytes,size);std::string error;
        if(kind==2) {
            standaloneFaces=std::make_unique<spFaceDataContainer>();
            require(standaloneFaces->ReadForAnalysis(input,size),"Invalid or unregistered face data");
        } else {
            mesh=std::make_unique<spMeshBV>();
            if(kind==0) {
                spSerializerManager manager;spResourceManager resources;
                spSerializerReadContextForAnalysis context(manager,resources);
                if(!spMeshBVSerializer().ReadPayloadForAnalysis(context,input,size,*mesh,&error))throw std::runtime_error(error);
                info.fieldMask=mesh->GetSerializedFieldMaskForAnalysis();
                info.vertexPayloadOffset=mesh->GetVertexPayloadOffsetForAnalysis();
            } else {
                auto geometry=spMeshBVSerializer::ReadGeometryForAnalysis(input,size,&info.vertexPayloadOffset,&error);
                require(geometry!=nullptr,error.c_str());
                require(mesh->SetDataAndBoundsForAnalysis(std::move(geometry)),"Unsupported or invalid MeshBV geometry");
                info.fieldMask=1;
            }
            const auto* data=mesh->GetDataForAnalysis();
            require(data&&data->GetIndicesForAnalysis()&&data->GetVerticesForAnalysis(),"MeshBV contains no geometry to inspect");
            info.primitiveType=static_cast<std::uint32_t>(data->GetIndicesForAnalysis()->GetTypeForAnalysis());
            info.indices=data->GetIndicesForAnalysis()->GetIndexCountForAnalysis();
            info.vertices=data->GetVerticesForAnalysis()->GetVertexCountForAnalysis();
        }
        std::uint32_t position=0;
        require(input.GetCurrentPosition(position)&&position==size,"Trailing MeshBV inspection bytes");
        if(const auto* container=faces()) {
            info.hasFaces=1;info.faceClassID=container->GetElementClassForAnalysis();
            info.faces=static_cast<std::uint32_t>(container->GetElementsForAnalysis().size());
            for(const auto& element:container->GetElementsForAnalysis())
                require(dynamic_cast<const winx::reconstruction::wxFaceData*>(element.get())!=nullptr,"Face data has no wxFaceData view");
        }
    }
};
static_assert(sizeof(SpvMeshBVInfo)==32&&sizeof(SpvFaceData)==16);
struct Binding {
    Sampler sampler;
    spTransformTrackEval::PlaybackForAnalysis playback;
    spNodeController controller;
};
struct Scene {
    std::shared_ptr<spvhost::ResourceGraph> graph;
    std::vector<SpvNode> initial;
    std::vector<spNode::Matrix3> orientations;
    std::vector<std::shared_ptr<spNode>> nodes;
    std::unordered_map<const spNode*,std::int32_t> nodeOrdinals;
    std::shared_ptr<spAnimation> animation;
    std::vector<std::unique_ptr<Binding>> bindings;
    void reset() {
        for(std::size_t i=0;i<nodes.size();++i) {
            nodes[i]->SetPositionForAnalysis(values<3>(initial[i].position));
            nodes[i]->SetScaleForAnalysis(values<3>(initial[i].scale));
            nodes[i]->SetOrientationForAnalysis(orientations[i]);
            nodes[i]->MarkLocalTransformDirtyForAnalysis();
        }
    }
    void sample(float time) {
        require(std::isfinite(time),"Non-finite sample time");
        // Seeking always starts from authored rest PRS.
        reset();
        for(auto& binding:bindings) { binding->playback.time=time; binding->controller.ApplyForAnalysis(time); }
        for(auto& node:nodes) if(!node->GetParentForAnalysis())
            require(node->UpdateWorldForAnalysis(),"World update failed");
    }
};
Scene& scene(void* handle) { require(handle!=nullptr,"Null scene handle"); return *static_cast<Scene*>(handle); }
std::unique_ptr<Scene> makeGraphScene(std::shared_ptr<spvhost::ResourceGraph> graph,
    const std::uint32_t* ids,std::uint32_t count) {
    require(graph&&count<=16384&&(ids||!count),"Invalid graph scene selection");
    auto result=std::make_unique<Scene>();result->graph=std::move(graph);
    auto& indices=result->nodeOrdinals;
    for(std::uint32_t i=0;i<count;++i) {
        auto node=result->graph->Node(ids[i]);
        require(indices.emplace(node.get(),static_cast<std::int32_t>(i)).second,"Repeated node in graph scene");
        result->nodes.push_back(std::move(node));
    }
    for(const auto& node:result->nodes) {
        SpvNode rest{};rest.parent=-1;rest.billboard=node->GetBillboardAxisForAnalysis();
        if(const auto* parent=node->GetParentForAnalysis()) {
            auto i=indices.find(parent);require(i!=indices.end(),"Graph scene selection omits a parent");rest.parent=i->second;
        }
        const auto& p=node->GetPositionForAnalysis();const auto& s=node->GetScaleForAnalysis();
        std::copy(p.begin(),p.end(),rest.position);std::copy(s.begin(),s.end(),rest.scale);
        result->initial.push_back(rest);result->orientations.push_back(node->GetOrientationForAnalysis());
    }
    return result;
}
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
SPV_API int spv_ps2_mesh_header(const std::uint8_t* bytes,std::uint32_t size,SpvPs2MeshHeader* output) noexcept {
    return guarded([&]{
        require(bytes&&output&&size>=40&&size<=32u*1024u*1024u,"Invalid bounded PS2 mesh packet envelope");
        BorrowedInput input(bytes,size);spPS2MeshDataSerializer::NativePayloadHeaderForAnalysis header;
        require(spPS2MeshDataSerializer::ReadNativeHeaderForAnalysis(input,header),"Truncated PS2 mesh header");
        // Inspector extent guard, not original hardware-packet validation.
        require(std::uint64_t(header.packetQwords)*16==size-40,"PS2 packet qword extent differs from its field");
        std::copy(header.sphere.begin(),header.sphere.end(),output->sphere);
        output->primitives=header.primitiveCount;output->vertices=header.vertexCount;output->componentFlags=header.componentFlags;
        output->packetQwords=header.packetQwords;output->additionalUVCount=header.additionalUVCount;output->weightCount=header.weightCount;
    });
}
SPV_API int spv_mesh_bounds(const std::uint8_t* bytes,std::uint32_t size,float* output) noexcept {
    return guarded([&]{require(bytes&&output&&size==24,"Mesh bounding box requires exactly 24 bytes");
        BorrowedInput input(bytes,size);spPS2MeshDataSerializer::BoundingBoxForAnalysis bounds;
        require(spPS2MeshDataSerializer::ReadBoundingBoxForAnalysis(input,bounds),"Truncated mesh bounds");
        std::copy(bounds.minimum.begin(),bounds.minimum.end(),output);std::copy(bounds.maximum.begin(),bounds.maximum.end(),output+3);
    });
}
SPV_API void* spv_texture_section_read(const std::uint8_t* bytes,std::uint32_t size,std::uint32_t kind) noexcept {
    std::unique_ptr<TextureSectionView> result;
    if(!guarded([&]{result=std::make_unique<TextureSectionView>(bytes,size,kind);}))return nullptr;
    return result.release();
}
SPV_API void spv_texture_section_destroy(void* handle) noexcept {guarded([&]{delete static_cast<TextureSectionView*>(handle);});}
SPV_API int spv_texture_section_info(void* handle,SpvTextureSectionInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Missing texture section info");*output=static_cast<TextureSectionView*>(handle)->info;});
}
SPV_API int spv_texture_section_mips(void* handle,SpvTextureMip* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle,"Missing texture section handle");const auto& mips=static_cast<TextureSectionView*>(handle)->mips;
        require(count==mips.size()&&(output||!count),"Texture mip output count differs");std::copy(mips.begin(),mips.end(),output);});
}
SPV_API int spv_texture_section_bgra(void* handle,std::uint8_t* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle&&output,"Missing texture preview input/output");const auto& view=*static_cast<TextureSectionView*>(handle);
        require(view.info.kind==0&&view.info.format==1&&count==view.xrgb.size(),"This projection requires stored XRGB pixels");
        for(std::size_t i=0;i<view.xrgb.size();i+=4) {
            const auto color=sparkplug::evidence::pc::texture_mips::DecodeRawPixel(view.xrgb.data()+i,1);
            for(unsigned c=0;c<4;++c)output[i+c]=static_cast<std::uint8_t>(sparkplug::evidence::pc::texture_mips::EncodeRawChannel(color[c],255,.5));
        }
    });
}
SPV_API int spv_texture_section_field1c(void* handle,std::uint32_t* output) noexcept {
    return guarded([&]{require(handle&&output,"Missing texture field1C input/output");*output=static_cast<TextureSectionView*>(handle)->field1C;});
}
SPV_API void* spv_texture_write_bgra(const std::uint8_t* pixels,std::uint32_t count,std::uint32_t width,
    std::uint32_t height,std::uint32_t field1C,std::uint32_t kind) noexcept {
    std::unique_ptr<SerializedBytes> result;
    if(!guarded([&]{
        require(pixels&&width&&height&&width<=16384&&height<=16384&&std::uint64_t(width)*height*4==count
            &&count<=16u*1024u*1024u&&field1C<=255&&kind<=1,"Invalid bounded BGRA writer input");
        spTextureData texture;spTextureData::NativeMipForAnalysis mip;
        mip.width=width;mip.rows=height;mip.rowStride=width*4;
        const auto* bytes=reinterpret_cast<const std::byte*>(pixels);mip.bytes.assign(bytes,bytes+count);
        std::vector<spTextureData::NativeMipForAnalysis> mips;mips.push_back(std::move(mip));
        require(texture.SetNativeMipDataForAnalysis(width,height,0,static_cast<std::uint8_t>(field1C),std::move(mips)),"Cannot prepare CPU texture mip input");
        spMemoryStream output;require(output.Open("tool.texture.output"),"Cannot open output memory stream");
        if(kind==1)require(spDXTextureDataSerializer::WriteMipRecordForAnalysis(output,texture,texture.GetNativeMipsForAnalysis()[0],true),"Cannot serialize first mip record");
        else {
            auto source=std::make_unique<spMemoryStream>();require(source->Open("tool.texture.source"),"Cannot open embedded memory stream");
            spSerializerManager manager;require(manager.SetSerializationPolicyForAnalysis(1),"Cannot select native-only texture output");
            std::string error;
            if(!spDXTextureDataSerializer().WritePayloadWithContextForAnalysis(manager,*source,texture,&error))throw std::runtime_error(error);
            require(spSerializer::WriteObjectHeaderForAnalysis(output,texture),"Cannot write TextureData object header");
            if(!spTextureDataSerializer::WriteEmbeddedSourceForAnalysis(output,texture,source,&error))throw std::runtime_error(error);
            require(!source,"Embedded source ownership was not consumed");
        }
        result=std::make_unique<SerializedBytes>(output);
    }))return nullptr;
    return result.release();
}
SPV_API void spv_serialized_bytes_destroy(void* handle) noexcept {guarded([&]{delete static_cast<SerializedBytes*>(handle);});}
SPV_API int spv_serialized_bytes_size(void* handle,std::uint32_t* output) noexcept {
    return guarded([&]{require(handle&&output,"Missing serialized bytes input/output");*output=static_cast<std::uint32_t>(static_cast<SerializedBytes*>(handle)->bytes.size());});
}
SPV_API int spv_serialized_bytes_copy(void* handle,std::uint8_t* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle&&output,"Missing serialized byte copy input/output");const auto& bytes=static_cast<SerializedBytes*>(handle)->bytes;
        require(bytes.size()==count,"Serialized output byte count differs");std::copy(bytes.begin(),bytes.end(),output);});
}
namespace {
void PrepareTriangleBuffers(const SpvMeshVertex* vertices,std::uint32_t vertexCount,
    const std::uint32_t* indices,std::uint32_t indexCount,std::uint32_t flags,spIndexBuffer& ib,spVertexBuffer& vb) {
        require(vertices&&indices&&vertexCount&&vertexCount<=65536&&indexCount&&indexCount<=3000000
            &&indexCount%3==0,"Invalid bounded triangle writer input");
        require((flags&~0x197eu)==0&&((flags&0x1e)==0||(flags&0x1e)==0x1e),"Unsupported host vertex attributes");
        require(vb.InitializeForAnalysis(flags,0,0),"Cannot inspect original vertex layout");
        const auto layout=spvhost::RenderMeshView::Layout(vb);
        require(std::uint64_t(vertexCount)*layout.stride+std::uint64_t(indexCount)*2<=16u*1024u*1024u,
            "Triangle writer exceeds host buffer budget");
        require(vb.InitializeForAnalysis(flags,vertexCount,0),"Cannot initialize original vertex buffer");
        std::vector<std::byte> data(vb.GetVertexSizeForAnalysis());
        for(std::uint32_t i=0;i<vertexCount;++i) {
            const auto& source=vertices[i];auto* target=data.data()+std::size_t(i)*layout.stride;
            const auto copy=[&](const void* input,std::int32_t offset,std::uint32_t size) {
                if(offset<0)return;
                require(std::uint64_t(offset)+size<=layout.stride,"Attribute exceeds original vertex layout");
                std::memcpy(target+offset,input,size);
            };
            // Host DTO assignment, using the one original component table.
            // No inferred weights, normal normalization or duplicate wire grammar.
            const auto finite=[](const auto& values){return std::all_of(std::begin(values),std::end(values),[](float x){return std::isfinite(x);});};
            require(finite(source.position)&&finite(source.normal)&&finite(source.uv0)&&finite(source.uv1)&&finite(source.weights),
                "Non-finite host vertex attribute");
            copy(source.position,0,sizeof(source.position));copy(source.normal,layout.normal,sizeof(source.normal));
            copy(source.uv0,layout.uv0,sizeof(source.uv0));copy(source.uv1,layout.uv1,sizeof(source.uv1));
            copy(source.weights,layout.weights,sizeof(source.weights));copy(&source.color,layout.color,4);copy(&source.bones,layout.bones,4);
        }
        require(vb.SetDataForAnalysis(data),"Cannot assign original vertex buffer data");
        require(ib.InitializeForAnalysis(indexCount/3,spIndexBuffer::eIndexBufferType::Type2,0),"Cannot initialize original index buffer");
        for(std::uint32_t i=0;i<indexCount;++i)
            require(indices[i]<vertexCount&&ib.SetIndexForAnalysis(i,indices[i]),"Triangle index exceeds vertex buffer");
}
}
SPV_API void* spv_mesh_write_triangles(const SpvMeshVertex* vertices,std::uint32_t vertexCount,
    const std::uint32_t* indices,std::uint32_t indexCount,std::uint32_t flags,std::uint32_t kind) noexcept {
    std::unique_ptr<SerializedBytes> result;
    if(!guarded([&]{
        require(kind<=1,"Unsupported mesh writer kind");
        spIndexBuffer ib;spVertexBuffer vb;
        PrepareTriangleBuffers(vertices,vertexCount,indices,indexCount,flags,ib,vb);
        spMeshData mesh;require(mesh.InitializeForAnalysis(ib,vb),"Cannot initialize original mesh owner");
        spMemoryStream output;require(output.Open("tool.mesh.output"),"Cannot open mesh output stream");
        require(spSerializer::WriteObjectHeaderForAnalysis(output,mesh),"Cannot write MeshData object header");
        spSerializerManager manager;require(manager.SetSerializationPolicyForAnalysis(kind),"Cannot select mesh writer policy");
        spMeshDataSerializer portable;spDXMeshDataSerializer pc;std::string error;
        const bool written=kind==0?portable.WritePayloadWithContextForAnalysis(manager,output,mesh,&error)
            :pc.WritePayloadWithContextForAnalysis(manager,output,mesh,&error);
        if(!written)throw std::runtime_error(error);
        result=std::make_unique<SerializedBytes>(output);
    }))return nullptr;
    return result.release();
}
SPV_API void* spv_mesh_bv_write_triangles(const SpvMeshVertex* vertices,std::uint32_t vertexCount,
    const std::uint32_t* indices,std::uint32_t indexCount) noexcept {
    std::unique_ptr<SerializedBytes> result;
    if(!guarded([&]{
        spIndexBuffer ib;spVertexBuffer vb;
        PrepareTriangleBuffers(vertices,vertexCount,indices,indexCount,0,ib,vb);
        auto geometry=spMeshBVSerializer::CreateGeometryForAnalysis(ib.CopyBufferForAnalysis(),vb.CopyBufferForAnalysis());
        spMeshBV mesh;require(mesh.SetDataAndBoundsForAnalysis(std::move(geometry)),"Cannot prepare original MeshBV owner/bounds");
        spMemoryStream output;require(output.Open("tool.mesh_bv.output"),"Cannot open MeshBV output stream");
        require(spSerializer::WriteObjectHeaderForAnalysis(output,mesh),"Cannot write MeshBV object header");
        std::string error;
        if(!spMeshBVSerializer().WritePayloadForAnalysis(output,mesh,&error))throw std::runtime_error(error);
        result=std::make_unique<SerializedBytes>(output);
    }))return nullptr;
    return result.release();
}
SPV_API void* spv_mesh_read(const std::uint8_t* bytes,std::uint32_t size,std::uint32_t kind,std::uint32_t platformMask) noexcept {
    std::unique_ptr<spvhost::RenderMeshView> result;
    if(!guarded([&]{require(bytes&&size,"Missing render mesh input");BorrowedInput input(bytes,size);
        result=std::make_unique<spvhost::RenderMeshView>(input,size,kind,platformMask);}))return nullptr;
    return result.release();
}
SPV_API void spv_mesh_destroy(void* handle) noexcept {guarded([&]{delete static_cast<spvhost::RenderMeshView*>(handle);});}
SPV_API int spv_mesh_info(void* handle,SpvMeshInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Missing mesh info input/output");*output=static_cast<spvhost::RenderMeshView*>(handle)->info;});
}
SPV_API int spv_mesh_vertices(void* handle,SpvMeshVertex* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle,"Missing render mesh handle");const auto& values=static_cast<spvhost::RenderMeshView*>(handle)->vertices;
        require(values.size()==count&&(output||!count),"Mesh vertex output count mismatch");if(count)std::copy(values.begin(),values.end(),output);});
}
SPV_API int spv_mesh_indices(void* handle,std::uint32_t* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle,"Missing render mesh handle");const auto& values=static_cast<spvhost::RenderMeshView*>(handle)->indices;
        require(values.size()==count&&(output||!count),"Mesh index output count mismatch");if(count)std::copy(values.begin(),values.end(),output);});
}
SPV_API int spv_vertex_layout(std::uint32_t flags,SpvVertexLayout* output) noexcept {
    return guarded([&]{require(output,"Missing vertex layout output");spVertexBuffer vb;
        require(vb.InitializeForAnalysis(flags,0),"Cannot initialize original vertex layout");*output=spvhost::RenderMeshView::Layout(vb);});
}
SPV_API void* spv_mesh_bv_read(const std::uint8_t* bytes,std::uint32_t size,std::uint32_t kind) noexcept {
    std::unique_ptr<MeshBVView> result;
    if(!guarded([&]{result=std::make_unique<MeshBVView>(bytes,size,kind);}))return nullptr;
    return result.release();
}
SPV_API void spv_mesh_bv_destroy(void* handle) noexcept {guarded([&]{delete static_cast<MeshBVView*>(handle);});}
SPV_API int spv_mesh_bv_info(void* handle,SpvMeshBVInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Missing MeshBV info input/output");*output=static_cast<MeshBVView*>(handle)->info;});
}
SPV_API int spv_mesh_bv_geometry(void* handle,float* positions,std::uint32_t floats,std::int32_t* indices,std::uint32_t count) noexcept {
    return guarded([&]{
        require(handle,"Missing MeshBV handle");const auto& view=*static_cast<MeshBVView*>(handle);
        require(view.mesh&&floats==std::uint64_t(view.info.vertices)*3&&count==view.info.indices
            &&(positions||!floats)&&(indices||!count),"MeshBV geometry output size mismatch");
        const auto* data=view.mesh->GetDataForAnalysis();
        const auto& vertices=data->GetVerticesForAnalysis()->GetDataForAnalysis();
        require(vertices.size()==std::uint64_t(floats)*sizeof(float),"MeshBV position buffer layout mismatch");
        if(floats)std::memcpy(positions,vertices.data(),vertices.size());
        for(std::uint32_t i=0;i<count;++i)indices[i]=static_cast<std::int32_t>(*data->GetIndicesForAnalysis()->GetIndexForAnalysis(i));
    });
}
SPV_API int spv_mesh_bv_faces(void* handle,SpvFaceData* output,std::uint32_t count) noexcept {
    return guarded([&]{
        require(handle,"Missing MeshBV handle");const auto& view=*static_cast<MeshBVView*>(handle);
        require(count==view.info.faces&&(output||!count),"MeshBV face output size mismatch");
        if(!count)return;
        const auto& faces=view.faces()->GetElementsForAnalysis();
        for(std::uint32_t i=0;i<count;++i) {
            const auto& face=static_cast<const winx::reconstruction::wxFaceData&>(*faces[i]);
            output[i]={face.GetSurfaceTypeForAnalysis(),face.GetFlagsForAnalysis(),face.GetSurfaceIDForAnalysis(),face.GetSerializedFieldMaskForAnalysis()};
        }
    });
}
SPV_API void* spv_container_inspect(const std::uint8_t* data,std::uint32_t count) noexcept {
    std::unique_ptr<ContainerIndex> result;
    if(!guarded([&]{result=std::make_unique<ContainerIndex>(data,count);}))return nullptr;
    return result.release();
}
SPV_API void spv_container_destroy(void* handle) noexcept {guarded([&]{delete static_cast<ContainerIndex*>(handle);});}
SPV_API int spv_container_info(void* handle,SpvContainerInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Missing container info input/output");*output=static_cast<ContainerIndex*>(handle)->info;});
}
SPV_API int spv_container_entries(void* handle,SpvContainerEntry* output,std::uint32_t count) noexcept {
    return guarded([&]{
        require(handle,"Missing container handle");const auto& entries=static_cast<ContainerIndex*>(handle)->entries;
        require(count==entries.size()&&(output||!count),"Container entry buffer count mismatch");
        if(count)std::copy(entries.begin(),entries.end(),output);
    });
}
SPV_API void* spv_graph_load(const std::uint8_t* data,std::uint32_t count) noexcept {
    std::unique_ptr<spvhost::GraphHandle> result;
    if(!guarded([&]{result=std::make_unique<spvhost::GraphHandle>();
        result->graph=std::make_shared<spvhost::ResourceGraph>(data,count);}))return nullptr;
    return result.release();
}
SPV_API void* spv_graph_load_with_trace(const std::uint8_t* data,std::uint32_t count) noexcept {
    std::unique_ptr<spvhost::GraphHandle> result;
    if(!guarded([&]{result=std::make_unique<spvhost::GraphHandle>();
        result->graph=std::make_shared<spvhost::ResourceGraph>(data,count,true);}))return nullptr;
    return result.release();
}
SPV_API int spv_graph_reference_trace_info(void* handle,std::uint32_t* references,
    std::uint32_t* payloads,std::uint32_t* origin) noexcept {
    return guarded([&]{require(handle&&references&&payloads&&origin,"Invalid reference trace output");
        const auto& trace=static_cast<spvhost::GraphHandle*>(handle)->graph->readTrace;
        if(!trace.valid||!trace.complete)throw std::runtime_error(trace.diagnostic.empty()
            ?"SPARKPLUG_REFERENCE_TRACE: successful traced file load required":trace.diagnostic);
        *references=static_cast<std::uint32_t>(trace.referenceReads.size());
        *payloads=static_cast<std::uint32_t>(trace.payloadReads.size());*origin=trace.dataPhysicalOrigin;});
}
SPV_API int spv_graph_reference_reads(void* handle,SpvReferenceRead* output,std::uint32_t count) noexcept {
    static_assert(sizeof(SpvReferenceRead)==28);
    return guarded([&]{require(handle,"Missing graph trace handle");
        const auto& trace=static_cast<spvhost::GraphHandle*>(handle)->graph->readTrace;
        require(trace.valid&&trace.complete,"SPARKPLUG_REFERENCE_TRACE: incomplete or unproven file trace");
        require(count==trace.referenceReads.size()&&(output||!count),"Reference trace count mismatch");
        for(std::size_t i=0;i<count;++i){const auto& r=trace.referenceReads[i];
            output[i]={r.consumerId,r.id,r.inlineSize,r.idPhysicalOffset,r.sizePhysicalOffset,
                static_cast<std::uint32_t>(r.resolution),r.success?1u:0u};}});
}
SPV_API int spv_graph_payload_reads(void* handle,SpvPayloadRead* output,std::uint32_t count) noexcept {
    static_assert(sizeof(SpvPayloadRead)==24);
    return guarded([&]{require(handle,"Missing graph trace handle");
        const auto& trace=static_cast<spvhost::GraphHandle*>(handle)->graph->readTrace;
        require(trace.valid&&trace.complete,"SPARKPLUG_REFERENCE_TRACE: incomplete or unproven file trace");
        require(count==trace.payloadReads.size()&&(output||!count),"Payload trace count mismatch");
        for(std::size_t i=0;i<count;++i){const auto& p=trace.payloadReads[i];
            output[i]={p.objectId,p.wireClassId,p.physicalOffset,p.size,static_cast<std::uint32_t>(p.kind),p.complete?1u:0u};}});
}
SPV_API void spv_graph_destroy(void* handle) noexcept {guarded([&]{delete static_cast<spvhost::GraphHandle*>(handle);});}
SPV_API int spv_graph_info(void* handle,std::uint32_t* objects,std::uint32_t* nodes,std::uint32_t* rootID) noexcept {
    return guarded([&]{require(handle&&objects&&nodes&&rootID,"Invalid graph metadata output");
        const auto& graph=*static_cast<spvhost::GraphHandle*>(handle)->graph;
        *objects=static_cast<std::uint32_t>(graph.entries.size());*nodes=static_cast<std::uint32_t>(graph.nodeIDs.size());*rootID=graph.rootID;});
}
SPV_API int spv_graph_object(void* handle,std::uint32_t ordinal,char* name,std::uint32_t capacity,SpvGraphObject* output) noexcept {
    return guarded([&]{require(handle&&name&&capacity&&output,"Invalid graph object output");
        const auto& graph=*static_cast<spvhost::GraphHandle*>(handle)->graph;
        require(ordinal<graph.entries.size(),"Invalid graph object ordinal");const auto& entry=graph.entries[ordinal];
        require(entry.name.size()<capacity,"Graph name output too small");std::memcpy(name,entry.name.c_str(),entry.name.size()+1);
        *output={entry.id,entry.wireClassID,entry.object->vfunc_18().classID,entry.offset,entry.size,
            dynamic_cast<spNode*>(entry.object)?1u:0u};});
}
SPV_API int spv_graph_node(void* handle,std::uint32_t id,SpvGraphNode* output) noexcept {
    return guarded([&]{require(handle&&output,"Invalid graph node output");
        const auto& graph=*static_cast<spvhost::GraphHandle*>(handle)->graph;const auto node=graph.Node(id);
        output->parentID=graph.ID(node->GetParentForAnalysis());output->flags=node->GetFlagsForAnalysis();
        output->children=static_cast<std::uint32_t>(node->GetChildCountForAnalysis());output->collisions=static_cast<std::uint32_t>(node->GetCollisionCountForAnalysis());
        const auto& p=node->GetPositionForAnalysis();const auto& r=node->GetOrientationForAnalysis();const auto& s=node->GetScaleForAnalysis();
        const auto q=sparkplug::evidence::pc::animation_math::FromMatrix(r);
        std::copy(p.begin(),p.end(),output->position);std::copy(r.begin(),r.end(),output->orientation);std::copy(s.begin(),s.end(),output->scale);
        std::copy(q.begin(),q.end(),output->rotation);});
}
namespace {
const spvhost::ResourceGraph& graphForView(void* handle) {
    require(handle,"Missing graph handle");return *static_cast<spvhost::GraphHandle*>(handle)->graph;
}
template<class T> const T& graphResource(const spvhost::ResourceGraph& graph,std::uint32_t id) {
    const auto* value=dynamic_cast<const T*>(graph.Find(id));
    require(value,"Graph resource has an unexpected runtime class");return *value;
}
const spMaterialPassLayer& graphPass(const spvhost::ResourceGraph& graph,std::uint32_t id,std::uint32_t index) {
    const auto& material=graphResource<spMaterial>(graph,id);
    require(index<material.GetPassCountForAnalysis(),"Graph material pass index out of range");
    const auto* pass=dynamic_cast<const spMaterialPassLayer*>(material.GetPassForAnalysis(index));
    require(pass,"Unsupported runtime material pass");return *pass;
}
SpvOctreeFields octreeFields(const spOctreeNode& node) {
    SpvOctreeFields output{};output.pivotKnown=node.HasGeometryForAnalysis()?1u:0u;
    if(output.pivotKnown)std::copy(node.GetPivotForAnalysis().begin(),node.GetPivotForAnalysis().end(),output.pivot);
    std::copy(node.GetMinsForAnalysis().begin(),node.GetMinsForAnalysis().end(),output.mins);
    std::copy(node.GetMaxsForAnalysis().begin(),node.GetMaxsForAnalysis().end(),output.maxs);return output;
}
}
static_assert(sizeof(SpvOctreeFields)==40&&sizeof(SpvGraphOctree)==76);
SPV_API int spv_octree_fields_read(const std::uint8_t* bytes,std::uint32_t size,SpvOctreeFields* output) noexcept {
    return guarded([&]{require(bytes&&size&&size<=1024u*1024u&&output,"Invalid bounded Octree fields input/output");
        spOctreeNode node;spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);BorrowedInput source(bytes,size);std::string error;
        if(!spOctreeNodeSerializer().ReadOctreeFieldsForAnalysis(context,source,size,node,&error))throw std::runtime_error(error);
        *output=octreeFields(node);});
}
SPV_API int spv_fog_payload_read(const std::uint8_t* bytes,std::uint32_t size,SpvFogFields* output) noexcept {
    static_assert(sizeof(SpvFogFields)==20);
    return guarded([&]{require(bytes&&size==20&&output,"Fog inspection requires one twenty-byte payload");
        spFog fog;spMemoryStream stream;require(stream.Open("tool.fog.inspection"),"Cannot open Fog inspection stream");
        spDataBlockSerializer fields;
        require(fields.BeginObjectForAnalysis(stream,&fog)&&fields.WriteFieldForAnalysis(stream,0,bytes,size)&&fields.FinalizeObjectForAnalysis(),"Cannot envelope Fog inspection payload");
        std::uint32_t extent=0;require(stream.GetSize(&extent)&&stream.Seek(spStream::SeekSource::essStart,0),"Cannot rewind Fog inspection stream");
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);std::string error;
        if(!spFogSerializer().ReadPayloadForAnalysis(context,stream,extent,fog,&error))throw std::runtime_error(error);
        *output={static_cast<std::uint32_t>(fog.GetTypeForAnalysis()),fog.GetColorARGBForAnalysis(),fog.GetStartForAnalysis(),fog.GetEndForAnalysis(),fog.GetDensityForAnalysis()};});
}
SPV_API int spv_graph_navigation_json(void* handle,std::uint8_t* output,std::uint32_t capacity,std::uint32_t* size) noexcept {
    return guarded([&]{require(size,"Missing navigation snapshot size");*size=0;
        require(output||!capacity,"Missing navigation snapshot buffer");
        const auto& json=graphForView(handle).NavigationJSON();*size=static_cast<std::uint32_t>(json.size());
        if(!output)return;
        require(capacity>=json.size(),"Navigation snapshot buffer is too small");std::memcpy(output,json.data(),json.size());});
}
SPV_API int spv_graph_spatial_json(void* handle,std::uint8_t* output,std::uint32_t capacity,std::uint32_t* size) noexcept {
    return guarded([&]{require(size,"Missing spatial snapshot size");*size=0;
        require(output||!capacity,"Missing spatial snapshot buffer");
        const auto& json=graphForView(handle).SpatialJSON();*size=static_cast<std::uint32_t>(json.size());
        if(!output)return;
        require(capacity>=json.size(),"Spatial snapshot buffer is too small");std::memcpy(output,json.data(),json.size());});
}
SPV_API int spv_light_fields_read(const std::uint8_t* bytes,std::uint32_t size,SpvLightFields* output) noexcept {
    return guarded([&]{require(output,"Missing Light inspection output");*output=spvhost::ReadLightInspection(bytes,size);});
}
SPV_API int spv_font_read(const std::uint8_t* bytes,std::uint32_t size,SpvFontInfo* output,
    SpvFontGlyph* glyphs,std::uint32_t count) noexcept {
    static_assert(sizeof(SpvFontInfo)==24&&sizeof(SpvFontGlyph)==20);
    return guarded([&]{require(bytes&&size&&size<=16u*1024u*1024u&&output&&glyphs&&count==spFont::GlyphCount,
            "Invalid bounded Font inspection input/output");
        BorrowedInput input(bytes,size);spFont partial;spFontSerializer::InspectionForAnalysis observed;std::string error;
        if(!spFontSerializer{}.InspectPayloadForAnalysis(input,size,partial,observed,&error))
            throw std::runtime_error(error.empty()?"Cannot read Font inspection section":error);
        const auto& baseline=partial.GetBaselineForAnalysis();
        *output={partial.GetHeightForAnalysis(),baseline.value_or(0),baseline.has_value(),observed.hasImage,
            observed.image.offset,observed.image.size};
        const auto& values=partial.GetGlyphsForAnalysis();
        for(std::size_t i=0;i<values.size();++i) {
            glyphs[i].width=values[i].width;
            std::memcpy(glyphs[i].uv0,values[i].uv0.data(),8);std::memcpy(glyphs[i].uv1,values[i].uv1.data(),8);
        }
    });
}
SPV_API int spv_bv_scalar_read(std::uint32_t classID,std::uint32_t field,
    const std::uint8_t* bytes,std::uint32_t size,SpvBVField* output) noexcept {
    static_assert(sizeof(SpvBVField)==32);
    return guarded([&]{require(bytes&&output,"Missing bounding volume scalar input/output");
        SpvBVField result{};
        if(classID==spSphereBV::ClassID){
            require(field<=1&&size==(field?4u:12u),"Invalid SphereBV scalar field/extent");
            spSphereBV object;BorrowedInput input(bytes,size);
            require(spSphereBVSerializer::ReadScalarFieldForAnalysis(field,input,object),"Cannot read SphereBV scalar");
            if(field)result.values[0]=object.GetRadiusForAnalysis();
            else std::copy(object.GetPositionForAnalysis().begin(),object.GetPositionForAnalysis().end(),result.values);
        }else if(classID==spBoxBV::ClassID){
            require(field<=1&&size==12,"Invalid BoxBV scalar field/extent");
            spBoxBV object;BorrowedInput input(bytes,size);
            require(spBoxBVSerializer::ReadScalarFieldForAnalysis(field,input,object),"Cannot read BoxBV scalar");
            const auto& value=field?object.GetSizeForAnalysis():object.GetPositionForAnalysis();
            std::copy(value.begin(),value.end(),result.values);
            if(field){const auto derived=spBoxBVSerializer::DecodeSizeForAnalysis({value[0],value[1],value[2]});
                result.halfExtents[0]=derived.halfExtents.x;result.halfExtents[1]=derived.halfExtents.y;result.halfExtents[2]=derived.halfExtents.z;
                result.boundingRadius=object.GetBoundingRadiusForAnalysis();}
        }else if(classID==spOBBBV::ClassID){
            require(field<=2&&size==(field==2?16u:12u),"Invalid OBBBV scalar field/extent");
            spOBBBV object;std::array<float,4> quaternion{};std::string error;
            if(!spOBBBVSerializer::ReadScalarFieldForAnalysis(object,field,bytes,size,&quaternion,&error))throw std::runtime_error(error);
            if(field==2)std::copy(quaternion.begin(),quaternion.end(),result.values);
            else{const auto& value=field?object.GetSizeForAnalysis():object.GetPositionForAnalysis();
                std::copy(value.begin(),value.end(),result.values);
                if(field){const auto derived=spOBBBVSerializer::DecodeSizeForAnalysis({value[0],value[1],value[2]});
                    result.halfExtents[0]=derived.halfExtents.x;result.halfExtents[1]=derived.halfExtents.y;result.halfExtents[2]=derived.halfExtents.z;
                    result.boundingRadius=object.GetBoundingRadiusForAnalysis();}}
        }else throw std::runtime_error("Unsupported bounding volume scalar class");
        *output=result;});
}
SPV_API int spv_graph_renderable(void* handle,std::uint32_t id,SpvGraphRenderable* output) noexcept {
    return guarded([&]{require(output,"Missing Renderable output");const auto& graph=graphForView(handle);const auto& value=graphResource<spRenderable>(graph,id);
        *output={graph.ID(value.GetMaterialForAnalysis().get()),graph.ID(value.GetFogForAnalysis().get()),value.IsAlphaSortEnabledForAnalysis()?1u:0u,value.GetPriorityForAnalysis()};});
}
SPV_API int spv_graph_lens_flare(void* handle,std::uint32_t id,SpvLensFlareInfo* output) noexcept {
    return guarded([&]{require(output,"Missing LensFlare output");const auto& graph=graphForView(handle);const auto& flare=graphResource<spLensFlare>(graph,id);
        *output={static_cast<std::uint32_t>(flare.GetElementsForAnalysis().size()),graph.ID(flare.GetRenderNodeForAnalysis()),flare.GetOcclusionRadiusForAnalysis(),flare.GetOcclusionSpeedForAnalysis()};});
}
SPV_API int spv_graph_lens_flare_element(void* handle,std::uint32_t id,std::uint32_t ordinal,SpvLensFlareElement* output) noexcept {
    return guarded([&]{require(output,"Missing LensFlare element output");const auto& graph=graphForView(handle);const auto& flare=graphResource<spLensFlare>(graph,id);
        const auto* element=&flare.GetPrimaryForAnalysis();if(ordinal!=0xffffffffu){require(ordinal<flare.GetElementsForAnalysis().size(),"LensFlare element index out of range");element=flare.GetElementsForAnalysis()[ordinal].get();}
        *output={graph.ID(element->quad.GetMaterialForAnalysis().get()),element->color,element->distance,element->scale};});
}
SPV_API int spv_graph_particle(void* handle,std::uint32_t id,SpvParticleInfo* output) noexcept {
    static_assert(sizeof(SpvParticleInfo)==172);
    return guarded([&]{require(output,"Missing ParticleSystem output");const auto& graph=graphForView(handle);
        const auto& particle=graphResource<spParticleSystem>(graph,id);const auto& p=particle.Parameters();
        require(p.region.size()<=8,"Particle region exceeds projection capacity");*output={};
        for(std::size_t i=0;i<2;++i)std::copy(p.acceleration[i].begin(),p.acceleration[i].end(),output->acceleration+i*3);
        std::copy(p.direction.begin(),p.direction.end(),output->direction);std::copy(p.velocity.begin(),p.velocity.end(),output->velocity);
        std::copy(p.angle.begin(),p.angle.end(),output->angle);std::copy(p.scale.begin(),p.scale.end(),output->scale);
        std::copy(p.colors.begin(),p.colors.end(),output->colors);std::copy(p.times.begin(),p.times.end(),output->times);
        std::copy(p.sphere.begin(),p.sphere.end(),output->sphere);output->rate=p.rate;
        std::copy(p.flags.begin(),p.flags.end(),output->flags);output->regionType=p.regionType;
        output->regionValues=static_cast<std::uint32_t>(p.region.size());output->renderNode=graph.ID(particle.GetRenderNodeForAnalysis().get());
        std::copy(p.region.begin(),p.region.end(),output->region);const auto& pool=particle.GetPoolStateForAnalysis();std::copy(pool.begin(),pool.end(),output->pool);});
}
SPV_API int spv_graph_octree(void* handle,std::uint32_t id,SpvGraphOctree* output) noexcept {
    return guarded([&]{require(output,"Missing graph Octree output");const auto& graph=graphForView(handle);
        auto* loaded=dynamic_cast<spOctreeNode*>(graph.Find(id));require(loaded,"Expected loaded Octree node");
        auto& node=*loaded;*output={};output->parent=graph.ID(node.GetParentForAnalysis());
        require(node.GetChildCountForAnalysis()==8,"Octree factory must own eight child slots");
        for(std::size_t i=0;i<8;++i)output->children[i]=graph.ID(node.GetChildForAnalysis(i));
        output->fields=octreeFields(node);});
}
SPV_API int spv_graph_model(void* handle,std::uint32_t id,SpvGraphModel* output) noexcept {
    return guarded([&]{require(output,"Missing graph model output");const auto& graph=graphForView(handle);
        const auto& model=graphResource<spModel>(graph,id);
        *output={graph.ID(model.GetBaseMeshForAnalysis().get()),graph.ID(model.GetMaterialForAnalysis().get()),
            graph.ID(model.GetFogForAnalysis().get()),model.IsAlphaSortEnabledForAnalysis()?1u:0u,
            model.GetPriorityForAnalysis(),model.GetProjectionGroupForAnalysis()};});
}
SPV_API int spv_graph_material(void* handle,std::uint32_t id,SpvGraphMaterial* output) noexcept {
    return guarded([&]{require(output,"Missing graph material output");const auto& graph=graphForView(handle);
        const auto& material=graphResource<spMaterial>(graph,id);*output={};
        const auto& states=material.GetRenderStatesForAnalysis();std::copy(states.begin(),states.end(),output->states);
        output->vertexAlpha=material.GetVertexAlphaByteForAnalysis();
        output->powerInitialized=material.HasInitializedSpecularPowerForAnalysis()?1u:0u;
        if(output->powerInitialized)output->power=material.GetSpecularPowerForAnalysis();
        const std::array colors{material.GetAmbientColorForAnalysis(),material.GetDiffuseColorForAnalysis(),
            material.GetSpecularColorForAnalysis(),material.GetEmissiveColorForAnalysis()};
        for(std::size_t i=0;i<colors.size();++i)std::copy(colors[i].begin(),colors[i].end(),output->colors+4*i);
        output->colorController=graph.ID(material.GetMaterialColorControllerForAnalysis());
        output->passes=static_cast<std::uint32_t>(material.GetPassCountForAnalysis());});
}
SPV_API int spv_graph_pass(void* handle,std::uint32_t id,std::uint32_t index,SpvGraphPass* output) noexcept {
    return guarded([&]{require(output,"Missing graph pass output");const auto& pass=graphPass(graphForView(handle),id,index);
        *output={pass.GetFinalBlendOperationForAnalysis(),static_cast<std::uint32_t>(pass.GetLayerCountForAnalysis())};});
}
SPV_API int spv_graph_layer(void* handle,std::uint32_t id,std::uint32_t passIndex,std::uint32_t index,SpvGraphLayer* output) noexcept {
    return guarded([&]{require(output,"Missing graph layer output");const auto& graph=graphForView(handle);
        const auto& pass=graphPass(graph,id,passIndex);require(index<pass.GetLayerCountForAnalysis(),"Graph layer index out of range");
        const auto& layer=pass.GetLayerForAnalysis(index);require(bool(layer),"Missing runtime material layer");
        const auto& texture=layer->GetMaterialTextureForAnalysis();require(bool(texture),"Missing runtime material texture holder");
        *output={};output->classID=layer->vfunc_18().classID;output->texture=graph.ID(texture->GetTextureForAnalysis());
        output->animation=graph.ID(texture->GetAnimTextureControllerForAnalysis());output->uvController=graph.ID(texture->GetUVControllerForAnalysis());
        output->uvEnabled=texture->HasStaticTransformForAnalysis()?1u:0u;
        const auto* animation=dynamic_cast<const spAnimTexController*>(texture->GetAnimTextureControllerForAnalysis());
        const auto* uv=dynamic_cast<const spUVController*>(texture->GetUVControllerForAnalysis());
        output->animationBoundHere=animation&&animation->GetMaterialForAnalysis()==texture.get()?1u:0u;
        output->uvBoundHere=uv&&uv->GetMaterialForAnalysis()==texture.get()?1u:0u;
        const auto& states=texture->GetTextureStatesForAnalysis();std::copy(states.begin(),states.end(),output->states);
        const auto& matrix=texture->GetUVTransformForAnalysis();std::copy(matrix.begin(),matrix.end(),output->uv);});
}
SPV_API int spv_graph_texture(void* handle,std::uint32_t id,SpvGraphTexture* output) noexcept {
    return guarded([&]{require(output,"Missing graph texture output");
        const auto& texture=graphResource<spDXTexture>(graphForView(handle),id);
        require(texture.IsInitializedForAnalysis(),"Runtime texture is not initialized");
        *output={texture.GetWidthForAnalysis(),texture.GetHeightForAnalysis(),texture.GetSurfaceFormatForAnalysis(),
            static_cast<std::uint32_t>(texture.GetMipsForAnalysis().size())};});
}
SPV_API int spv_graph_texture_bgra(void* handle,std::uint32_t id,std::uint8_t* output,std::uint32_t count) noexcept {
    return guarded([&]{const auto& texture=graphResource<spDXTexture>(graphForView(handle),id);
        require(texture.IsInitializedForAnalysis()&&!texture.GetMipsForAnalysis().empty(),"Runtime texture has no initialized mip");
        const auto& mip=texture.GetMipsForAnalysis().front();const auto format=texture.GetSurfaceFormatForAnalysis();
        require(format==3||format==4,"Runtime texture surface format is not exposed by the BGRA upload adapter");
        require(count<=16u*1024u*1024u&&count==std::uint64_t(mip.width)*mip.height*4&&output,"Runtime texture output size mismatch");
        require(mip.rowBytes==mip.width*4&&mip.rows==mip.height&&mip.packedBytes.size()==count,"Runtime BGRA mip layout mismatch");
        if(format==3)std::memcpy(output,mip.packedBytes.data(),count);
        else for(std::uint32_t i=0;i<count;i+=4) {
            const auto color=sparkplug::evidence::pc::texture_mips::DecodeRawPixel(mip.packedBytes.data()+i,1);
            for(unsigned c=0;c<4;++c)output[i+c]=static_cast<std::uint8_t>(sparkplug::evidence::pc::texture_mips::EncodeRawChannel(color[c],255,.5));
        }});
}
SPV_API int spv_graph_texture_track(void* handle,std::uint32_t id,std::uint32_t* keys,float* duration) noexcept {
    return guarded([&]{require(keys&&duration,"Missing texture track output");
        const auto& track=graphResource<spAnimTexController>(graphForView(handle),id).GetTextureTrackForAnalysis();
        *keys=static_cast<std::uint32_t>(track.GetTimesForAnalysis().size());*duration=track.GetDurationForAnalysis();});
}
SPV_API int spv_graph_texture_keys(void* handle,std::uint32_t id,SpvGraphTextureKey* output,std::uint32_t count) noexcept {
    return guarded([&]{const auto& graph=graphForView(handle);const auto& track=graphResource<spAnimTexController>(graph,id).GetTextureTrackForAnalysis();
        const auto& times=track.GetTimesForAnalysis();const auto& textures=track.GetTexturesForAnalysis();
        require(count==times.size()&&(output||!count),"Texture track output count mismatch");
        for(std::uint32_t i=0;i<count;++i)output[i]={times[i],graph.ID(textures[i].get())};});
}
static_assert(sizeof(SpvGraphModel)==24&&sizeof(SpvGraphMaterial)==128&&sizeof(SpvGraphLayer)==112&&sizeof(SpvGraphTextureKey)==8);
static_assert(sizeof(SpvGraphControllerClock)==24&&sizeof(SpvGraphUVSubmission)==40);
SPV_API int spv_graph_controller_clock(void* handle,std::uint32_t id,SpvGraphControllerClock* output) noexcept {
    return guarded([&]{require(output,"Missing controller clock output");
        const auto& controller=graphResource<spRenderController>(graphForView(handle),id);
        *output={controller.vfunc_18().classID,controller.GetAccumulatedTimeForAnalysis(),controller.GetAppliedTimeForAnalysis(),0,0,
            controller.IsEnabledForAnalysis()?1u:0u};
        if(const auto* animation=dynamic_cast<const spAnimTexController*>(&controller)) {
            output->playback=animation->GetPlaybackTimeForAnalysis();output->hasPlayback=1;
        }});
}
SPV_API int spv_graph_apply_controllers(void* handle,const std::uint32_t* ids,std::uint32_t count,float elapsed) noexcept {
    return guarded([&]{require(std::isfinite(elapsed)&&count<=4096&&(ids||!count),"Invalid bounded controller application");
        const auto& graph=graphForView(handle);
        std::vector<spRenderController*> controllers;controllers.reserve(count);
        std::unordered_map<std::uint32_t,bool> seen;
        for(std::uint32_t i=0;i<count;++i) {
            require(seen.emplace(ids[i],true).second,"Repeated controller in one elapsed-time application");
            auto* controller=dynamic_cast<spRenderController*>(graph.Find(ids[i]));
            require(controller,"Expected a loaded render controller");
            require(std::isfinite(controller->GetAccumulatedTimeForAnalysis()+elapsed),"Controller clock would become non-finite");
            controllers.push_back(controller);
        }
        for(auto* controller:controllers)controller->ApplyForAnalysis(elapsed);
    });
}
SPV_API int spv_graph_update_material_color(void* handle,std::uint32_t id,std::uint32_t frame,std::uint32_t force,std::uint32_t* evaluated) noexcept {
    return guarded([&]{require(evaluated&&force<=1,"Invalid material color update output or force flag");*evaluated=0;
        auto* material=dynamic_cast<spDXMaterial*>(graphForView(handle).Find(id));require(material,"Expected loaded PC material");
        bool changed=false;
        const bool ok=material->UpdateColorForFrameForAnalysis(frame,force!=0,&changed);*evaluated=changed?1u:0u;
        require(ok,"Loaded material color controller update failed");});
}
SPV_API int spv_graph_update_material_pass(void* handle,std::uint32_t id,std::uint32_t passIndex,SpvGraphUVSubmission* output,std::uint32_t capacity,std::uint32_t* count) noexcept {
    return guarded([&]{require(count,"Missing material pass submission count");*count=0;
        const auto& graph=graphForView(handle);const auto& material=graphResource<spMaterial>(graph,id);
        require(passIndex<material.GetPassCountForAnalysis(),"Graph material pass index out of range");
        auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(passIndex));
        require(pass,"Unsupported runtime material pass");
        require(capacity>=pass->GetLayerCountForAnalysis()&&capacity<=8&&(output||!capacity),"Material UV submission capacity mismatch");
        struct Capture {SpvGraphUVSubmission* output;std::uint32_t* count;};Capture capture{output,count};
        const auto submit=+[](void* context,std::uint32_t stage,const std::array<float,9>& matrix) {
            auto& value=*static_cast<Capture*>(context);auto& entry=value.output[(*value.count)++];entry.stage=stage;
            std::copy(matrix.begin(),matrix.end(),entry.matrix);return true;
        };
        require(pass->UpdateForRenderForAnalysis(0xffffffffu,submit,&capture),"Loaded material pass update failed; preceding layer mutations are retained");
    });
}
SPV_API void* spv_graph_scene(void* handle,const std::uint32_t* ids,std::uint32_t count) noexcept {
    std::unique_ptr<Scene> result;
    if(!guarded([&]{
        require(handle,"Missing graph scene handle");
        result=makeGraphScene(static_cast<spvhost::GraphHandle*>(handle)->graph,ids,count);
    }))return nullptr;
    return result.release();
}
SPV_API void* spv_graph_scene_all(void* handle) noexcept {
    std::unique_ptr<Scene> result;
    if(!guarded([&]{
        const auto& graph=graphForView(handle);std::vector<std::uint32_t> ids;
        for(auto id:graph.nodeIDs)if(graph.ID(graph.Find(id))==id)ids.push_back(id);
        result=makeGraphScene(static_cast<spvhost::GraphHandle*>(handle)->graph,ids.data(),static_cast<std::uint32_t>(ids.size()));
    }))return nullptr;
    return result.release();
}
SPV_API int spv_scene_node_count(void* handle,std::uint32_t* count) noexcept {
    return guarded([&]{require(count,"Missing scene node count output");*count=static_cast<std::uint32_t>(scene(handle).nodes.size());});
}
SPV_API int spv_scene_graph_node_ids(void* handle,std::uint32_t* output,std::uint32_t count) noexcept {
    return guarded([&]{const auto& value=scene(handle);require(bool(value.graph),"Scene has no loaded resource graph");
        require(count==value.nodes.size()&&(output||!count),"Scene node ID output size mismatch");
        for(std::uint32_t i=0;i<count;++i)output[i]=value.graph->ID(value.nodes[i].get());});
}
SPV_API int spv_scene_graph_skin_info(void* handle,std::uint32_t id,std::uint32_t* weights,std::uint32_t* bones) noexcept {
    return guarded([&]{const auto& value=scene(handle);require(value.graph&&weights&&bones,"Missing loaded scene/skin outputs");
        const auto& skin=graphResource<spSkin>(*value.graph,id);*weights=skin.GetWeightCountForAnalysis();*bones=static_cast<std::uint32_t>(skin.GetBoneCountForAnalysis());});
}
SPV_API int spv_scene_graph_skin_palette(void* handle,std::uint32_t id,float* output,std::uint32_t count) noexcept {
    return guarded([&]{const auto& value=scene(handle);require(bool(value.graph),"Scene has no loaded resource graph");
        const auto& skin=graphResource<spSkin>(*value.graph,id);const auto& bones=skin.GetBoneBindingsForAnalysis();
        require(count==bones.size()*16&&(output||!count),"Loaded skin palette output size mismatch");
        for(std::size_t i=0;i<bones.size();++i) {
            const auto node=bones[i].GetBoneForAnalysis();require(bool(node),"Loaded Skin bone is no longer owned");
            require(value.nodeOrdinals.find(node.get())!=value.nodeOrdinals.end(),"Loaded Skin bone is outside this scene selection");
            const auto matrix=spSkin::ComposePaletteMatrixForAnalysis(bones[i].inverseBindMatrix,node->GetWorldMatrixForAnalysis());
            for(auto component:matrix)require(std::isfinite(component),"Non-finite loaded Skin palette result");
            std::copy(matrix.begin(),matrix.end(),output+i*16);
        }});
}
SPV_API int spv_graph_render_containers(void* handle,SpvGraphRenderContainer* output,std::uint32_t capacity,std::uint32_t* count) noexcept {
    return guarded([&]{require(count,"Missing render container count output");const auto& graph=graphForView(handle);
        std::vector<SpvGraphRenderContainer> containers;
        for(const auto& entry:graph.entries) {
            if(graph.ID(entry.object)!=entry.id)continue;
            SpvGraphRenderContainer value{};value.id=entry.id;
            if(auto* node=dynamic_cast<spRenderNode*>(entry.object)) {
                value.kind=node->IsExactly(spSkyBox::ClassID)?3u:0u;value.renderables=static_cast<std::uint32_t>(node->GetRenderableCountForAnalysis());
                if(output) {
                    node->UpdateRenderMatricesForAnalysis();
                    const auto& world=node->GetCachedRenderMatrixForAnalysis();const auto& inverse=node->GetCachedRenderInverseForAnalysis();
                    std::copy(world.begin(),world.end(),value.world);std::copy(inverse.begin(),inverse.end(),value.inverse);
                }
            } else if(const auto* object=dynamic_cast<const spStaticRenderObject*>(entry.object)) {
                value.kind=1;value.renderables=static_cast<std::uint32_t>(object->GetRenderableCountForAnalysis());
                const auto& world=object->GetWorldMatrixForAnalysis();const auto& inverse=object->GetWorldInverseMatrixForAnalysis();
                std::copy(world.begin(),world.end(),value.world);std::copy(inverse.begin(),inverse.end(),value.inverse);
            } else if(const auto* partition=dynamic_cast<const spPartitionRenderable*>(entry.object)) {
                value.kind=2;value.renderables=static_cast<std::uint32_t>(partition->GetRenderablesForAnalysis().size());
                const auto& world=partition->GetWorldMatrixForAnalysis();const auto& inverse=partition->GetWorldInverseMatrixForAnalysis();
                std::copy(world.begin(),world.end(),value.world);std::copy(inverse.begin(),inverse.end(),value.inverse);
            } else continue;
            containers.push_back(value);
        }
        *count=static_cast<std::uint32_t>(containers.size());
        if(!output){require(capacity==0,"Missing render container output array");return;}
        require(capacity==containers.size(),"Render container output count mismatch");
        std::copy(containers.begin(),containers.end(),output);
    });
}
SPV_API int spv_graph_render_members(void* handle,std::uint32_t id,std::uint32_t* output,std::uint32_t count) noexcept {
    return guarded([&]{const auto& graph=graphForView(handle);const auto* object=graph.Find(id);
        const auto* node=dynamic_cast<const spRenderNode*>(object);
        const auto* fixed=dynamic_cast<const spStaticRenderObject*>(object);
        const auto* partition=dynamic_cast<const spPartitionRenderable*>(object);
        require(node||fixed||partition,"Expected an actual render support container");
        const auto size=node?node->GetRenderableCountForAnalysis():fixed?fixed->GetRenderableCountForAnalysis():partition->GetRenderablesForAnalysis().size();
        require(count==size&&(output||!count),"Render membership output count mismatch");
        for(std::uint32_t i=0;i<count;++i)output[i]=graph.ID(node?node->GetRenderableForAnalysis(i):
            fixed?fixed->GetRenderableForAnalysis(i):partition->GetRenderablesForAnalysis()[i].get());
    });
}
static_assert(sizeof(SpvGraphRenderContainer)==140);
SPV_API int spv_graph_render_occurrence(void* handle,std::uint32_t id,std::uint32_t slot,SpvGraphRenderOccurrence* output) noexcept {
    return guarded([&]{
        require(output,"Missing render occurrence output");const auto& graph=graphForView(handle);
        auto* object=graph.Find(id);const spRenderable* member=nullptr;
        const spSkin::Matrix4* world=nullptr;std::uint32_t rigidNode=0;
        if(auto* node=dynamic_cast<spRenderNode*>(object)) {
            require(slot<node->GetRenderableCountForAnalysis(),"Render occurrence slot out of range");
            member=node->GetRenderableForAnalysis(slot);node->UpdateRenderMatricesForAnalysis();
            world=&node->GetCachedRenderMatrixForAnalysis();rigidNode=graph.ID(node);
        } else if(const auto* fixed=dynamic_cast<const spStaticRenderObject*>(object)) {
            require(slot<fixed->GetRenderableCountForAnalysis(),"Render occurrence slot out of range");
            member=fixed->GetRenderableForAnalysis(slot);world=&fixed->GetWorldMatrixForAnalysis();
        } else if(const auto* partition=dynamic_cast<const spPartitionRenderable*>(object)) {
            require(slot<partition->GetRenderablesForAnalysis().size(),"Render occurrence slot out of range");
            member=partition->GetRenderablesForAnalysis()[slot].get();world=&partition->GetWorldMatrixForAnalysis();
        } else throw std::runtime_error("Expected an actual render support container");
        require(dynamic_cast<const spModel*>(member),"Render occurrence is not a supported Model/Skin");
        *output={};output->renderable=graph.ID(member);
        if(const auto* skin=dynamic_cast<const spSkin*>(member)) {
            const auto skinWorld=skin->GetRenderWorldMatrixForAnalysis();
            std::copy(skinWorld.begin(),skinWorld.end(),output->world);
        } else {
            output->rigidNode=rigidNode;std::copy(world->begin(),world->end(),output->world);
        }
    });
}
static_assert(sizeof(SpvGraphRenderOccurrence)==72);
SPV_API void* spv_material_read(const std::uint8_t* bytes,std::uint32_t count) noexcept {
    std::unique_ptr<MaterialView> result;
    if(!guarded([&]{result=std::make_unique<MaterialView>(bytes,count);}))return nullptr;
    return result.release();
}
SPV_API void* spv_material_patch_scalars(const std::uint8_t* bytes,std::uint32_t count,
    const std::uint32_t* states,std::uint32_t stateCount,std::uint32_t blend,
    const std::uint32_t* textureStates,std::uint32_t textureStateCount,std::uint32_t kind) noexcept {
    std::unique_ptr<SerializedBytes> result;
    if(!guarded([&]{
        require(bytes&&count&&count<=16u*1024u*1024u&&states&&stateCount==11&&kind<=1&&
            (kind==0?(textureStates&&textureStateCount==9):(!textureStates&&textureStateCount==0)),
            "MATERIAL_SCALAR_SHAPE: invalid bounded scalar edit input");
        BorrowedInput input(bytes,count);spDXMaterial material;
        spMaterialSerializer::InspectionForAnalysis observed;std::string error;
        if(!spMaterialDataSerializer{}.InspectPayloadForAnalysis(input,count,material,observed,&error))
            throw std::runtime_error(error);
        std::uint32_t end=0;
        require(input.GetCurrentPosition(end)&&end==count,"MATERIAL_SCALAR_SHAPE: unread trailing material bytes");
        using Field=spMaterialSerializer::Field;
        using Observation=spMaterialSerializer::InspectedScalarFieldForAnalysis;
        std::vector<const Observation*> stateFields,passFields,textureFields;
        for(const auto& field:observed.scalarFields) {
            if(field.field==Field::RenderStates) {
                require(field.applied&&field.payloadSize==44,"MATERIAL_SCALAR_SHAPE: unsupported render-state extent");
                stateFields.push_back(&field);
            } else if(field.field==Field::Pass) {
                require(field.applied&&field.pass&&field.payloadSize==4&&
                    material.GetPassForAnalysis(field.passIndex)==field.pass,
                    "MATERIAL_SCALAR_SHAPE: pass assignment has no exact owner");
                passFields.push_back(&field);
            } else if(kind==0&&(field.field==Field::LegacyTextureStates||field.field==Field::TextureStates)) {
                require(field.field==Field::TextureStates&&field.applied&&field.layer&&field.texture&&field.payloadSize==36,
                    "MATERIAL_SCALAR_SHAPE: legacy or orphan texture states require a separate authoring decision");
                textureFields.push_back(&field);
            }
        }
        require(!stateFields.empty()&&!passFields.empty()&&passFields.size()==material.GetPassCountForAnalysis(),
            "MATERIAL_SCALAR_SHAPE: missing authored render states or pass blend");
        spMaterialTexture* texture=nullptr;
        if(kind==0) {
            require(stateFields.size()==1&&passFields.size()==1&&textureFields.size()==1,
                "MATERIAL_SCALAR_SHAPE: expected one render-state, pass and effective field17 assignment");
            auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(0));
            require(pass&&pass->GetLayerCountForAnalysis()==1,"MATERIAL_SCALAR_SHAPE: expected one actual standard layer");
            auto* layer=dynamic_cast<spStdLayer*>(pass->GetLayerForAnalysis(0).get());
            require(layer&&layer->GetMaterialTextureForAnalysis(),"MATERIAL_SCALAR_SHAPE: missing actual texture holder");
            texture=layer->GetMaterialTextureForAnalysis().get();
            const auto& observedTexture=*textureFields.front();
            require(observedTexture.pass==pass&&observedTexture.layer==layer&&observedTexture.texture==texture&&
                observedTexture.passIndex==0&&observedTexture.layerIndex==0,
                "MATERIAL_SCALAR_SHAPE: field17 belongs to another layer");
            for(std::size_t i=0;i<9;++i)texture->SetTextureStateForAnalysis(i,textureStates[i]);
        }
        for(std::size_t i=0;i<11;++i)(void)material.SetRenderStateForAnalysis(i,states[i]);
        for(std::size_t i=0;i<material.GetPassCountForAnalysis();++i) {
            auto* pass=dynamic_cast<spMaterialPassLayer*>(material.GetPassForAnalysis(i));
            require(pass!=nullptr,"MATERIAL_SCALAR_SHAPE: unsupported actual pass");
            pass->SetFinalBlendOperationForAnalysis(blend);
        }
        // This byte copy is a host edit of observed extents, not another field
        // serializer. The original helper owns the written scalar payload.
        spMemoryStream output;
        require(output.Open("tool.material.patch")&&output.WriteData(bytes,count),"Cannot copy material template");
        const auto patch=[&](const Observation& destination,spMemoryStream& encoded) {
            PatchObservedFieldPayload(output,count,static_cast<std::uint32_t>(destination.field),
                destination.payloadOffset,destination.payloadSize,encoded);
        };
        spMemoryStream writtenStates;
        require(writtenStates.Open("tool.material.states"),"Cannot open scalar writer stream");
        if(!spMaterialSerializer::WriteRenderStatesFieldForAnalysis(writtenStates,material,&error))throw std::runtime_error(error);
        for(const auto* field:stateFields)patch(*field,writtenStates);
        for(const auto* field:passFields) {
            spMemoryStream writtenBlend;
            require(writtenBlend.Open("tool.material.blend"),"Cannot open scalar writer stream");
            if(!spMaterialSerializer::WritePassBlendFieldForAnalysis(writtenBlend,*field->pass,&error))throw std::runtime_error(error);
            patch(*field,writtenBlend);
        }
        if(texture) {
            spMemoryStream writtenTexture;
            require(writtenTexture.Open("tool.material.texture.states"),"Cannot open scalar writer stream");
            if(!spMaterialSerializer::WriteTextureStatesFieldForAnalysis(writtenTexture,*texture,&error))throw std::runtime_error(error);
            patch(*textureFields.front(),writtenTexture);
        }
        result=std::make_unique<SerializedBytes>(output);
    }))return nullptr;
    return result.release();
}
SPV_API void* spv_skin_patch_sort_scalars(const std::uint8_t* bytes,std::uint32_t count,
    std::uint32_t alphaSort,std::uint32_t priority) noexcept {
    std::unique_ptr<SerializedBytes> result;
    if(!guarded([&]{
        require(bytes&&count&&count<=16u*1024u*1024u&&alphaSort<=1,
            "RENDERABLE_SCALAR_SHAPE: invalid bounded Skin sort edit input");
        BorrowedInput input(bytes,count);spSkin skin;
        spSkinSerializer::InspectionForAnalysis observed;std::string error;
        if(!spSkinSerializer{}.InspectPayloadForAnalysis(input,count,skin,observed,&error))throw std::runtime_error(error);
        std::uint32_t end=0;
        require(input.GetCurrentPosition(end)&&end==count,"RENDERABLE_SCALAR_SHAPE: unread trailing Skin bytes");
        using Field=spRenderableSerializer::Field;
        using Observation=spRenderableSerializer::InspectedScalarFieldForAnalysis;
        const Observation* alphaField=nullptr;const Observation* priorityField=nullptr;
        for(const auto& field:observed.model.renderable.scalarFields) {
            require(field.owner==static_cast<const spRenderable*>(&skin)&&field.payloadSize==4,
                "RENDERABLE_SCALAR_SHAPE: scalar assignment has no exact Skin owner");
            auto& selected=field.field==Field::AlphaSortEnable?alphaField:priorityField;
            require((field.field==Field::AlphaSortEnable||field.field==Field::AlphaSortPriority)&&!selected,
                "RENDERABLE_SCALAR_SHAPE: repeated or unknown Renderable scalar assignment");
            selected=&field;
        }
        require(alphaField&&priorityField,"RENDERABLE_SCALAR_SHAPE: missing authored Renderable alpha or priority");
        skin.SetAlphaSortEnabledForAnalysis(alphaSort!=0);skin.SetPriorityForAnalysis(priority);
        spMemoryStream output,writtenAlpha,writtenPriority;
        require(output.Open("tool.skin.sort.patch")&&output.WriteData(bytes,count)&&
            writtenAlpha.Open("tool.renderable.alpha")&&writtenPriority.Open("tool.renderable.priority"),
            "Cannot open Renderable scalar writer streams");
        if(!spRenderableSerializer::WriteAlphaSortEnableFieldForAnalysis(writtenAlpha,skin,&error)||
            !spRenderableSerializer::WritePriorityFieldForAnalysis(writtenPriority,skin,&error))throw std::runtime_error(error);
        PatchObservedFieldPayload(output,count,static_cast<std::uint32_t>(alphaField->field),
            alphaField->payloadOffset,alphaField->payloadSize,writtenAlpha);
        PatchObservedFieldPayload(output,count,static_cast<std::uint32_t>(priorityField->field),
            priorityField->payloadOffset,priorityField->payloadSize,writtenPriority);
        result=std::make_unique<SerializedBytes>(output);
    }))return nullptr;
    return result.release();
}
SPV_API void* spv_skin_write_palette(void* handle,std::uint32_t weights,
    const SpvSkinPaletteBinding* bindings,std::uint32_t count) noexcept {
    std::unique_ptr<SerializedBytes> result;
    if(!guarded([&]{
        require(handle&&count<=1024&&(!count||bindings),"SKIN_PALETTE_INPUT: invalid bounded palette input");
        const auto& graph=graphForView(handle);
        const auto& trace=graph.readTrace;
        require(trace.valid&&trace.complete,"SKIN_PALETTE_INPUT: actual graph read trace is required");
        // Host selection/ownership only. The original FAT indexer below builds
        // both maps; no placeholder Node or substitute reference writer exists.
        std::map<std::uint32_t,std::shared_ptr<spNode>> selected;
        std::map<std::uint32_t,const spvhost::ResourceGraph::Entry*> retained;
        for(std::uint32_t i=0;i<count;++i) {
            const auto id=bindings[i].nodeID;
            require(id&&id!=0xFFFFFFFFu,"SKIN_PALETTE_INPUT: invalid retained Node ID");
            if(selected.find(id)!=selected.end())continue;
            auto node=graph.Node(id);
            const spvhost::ResourceGraph::Entry* entry=nullptr;
            for(const auto& candidate:graph.entries) {
                require(candidate.object!=node.get()||candidate.id==id,
                    "SKIN_PALETTE_INPUT: multiple file IDs alias the selected Node");
                if(candidate.id==id)entry=&candidate;
            }
            require(entry&&entry->wireClassID==spNode::ClassID&&entry->size>=8,
                "SKIN_PALETTE_INPUT: selected bone is not an exact catalogued Node");
            const auto physical=std::uint64_t(trace.dataPhysicalOrigin)+entry->offset;
            using Kind=spSerializerReadContextForAnalysis::PayloadReadKindForAnalysis;
            const bool covered=std::any_of(trace.payloadReads.begin(),trace.payloadReads.end(),[&](const auto& row) {
                return row.complete&&(row.kind==Kind::Outer||row.kind==Kind::Inline)&&
                    row.objectId==id&&row.wireClassId==entry->wireClassID&&
                    row.physicalOffset==physical&&row.size==entry->size;
            });
            require(covered,"SKIN_PALETTE_INPUT: retained Node payload was not completely read at its FAT extent");
            selected.emplace(id,std::move(node));retained.emplace(id,entry);
        }
        spSerializerManager manager;
        manager.SetDispatchContextForAnalysis(spSerializerManager::PlatformPC,spSerializerManager::OperationSave);
        require(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),
            spSerializerManager::PlatformPC,spSerializerManager::OperationSave),"Cannot register actual Node writer");
        auto* fat=manager.GetFATForAnalysis();require(fat!=nullptr,"Cannot create isolated palette writer FAT");
        for(const auto& item:selected) {
            const auto* original=retained.at(item.first);
            require(fat->SetNextResourceIDForAnalysis(item.first)&&
                fat->IndexObjectForAnalysis(spNode::ClassID,*item.second),"Cannot index retained palette Node");
            auto* entry=fat->FindByObjectForAnalysis(*item.second);
            require(entry&&entry->id==item.first&&fat->FindByIDForAnalysis(item.first)==entry,
                "Palette writer FAT identity mismatch");
            // Explicit retained payload input, confirmed against PC466FA0 +
            // PC467350 in authoring-palette-prebind. This is not a whole save.
            entry->offset=original->offset;entry->size=original->size;entry->payloadWritten=true;
        }
        std::vector<spSkin::BoneBinding> palette;palette.reserve(count);
        for(std::uint32_t i=0;i<count;++i) {
            spSkin::Matrix4 matrix{};std::memcpy(matrix.data(),bindings[i].inverseBind,sizeof(matrix));
            palette.push_back(spSkin::BoneBinding::BorrowedForAnalysis(selected.at(bindings[i].nodeID),matrix));
        }
        spSkin skin;require(skin.SetPaletteForAnalysis(weights,std::move(palette)),"Invalid actual Skin palette");
        spMemoryStream output;require(output.Open("tool.skin.palette"),"Cannot open Skin palette writer stream");
        std::string error;
        if(!spSkinSerializer::WritePaletteFieldWithContextForAnalysis(manager,output,skin,&error))throw std::runtime_error(error);
        result=std::make_unique<SerializedBytes>(output);
    }))return nullptr;
    return result.release();
}
SPV_API void spv_material_destroy(void* handle) noexcept { (void)guarded([&]{delete static_cast<MaterialView*>(handle);}); }
SPV_API int spv_material_info(void* handle,SpvMaterialInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Invalid material view");*output=static_cast<MaterialView*>(handle)->info;});
}
SPV_API int spv_material_layers(void* handle,SpvMaterialLayer* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle,"Invalid material view");const auto& layers=static_cast<MaterialView*>(handle)->layers;
        require(count==layers.size()&&(!count||output),"Material layer output size mismatch");std::copy(layers.begin(),layers.end(),output);});
}
SPV_API int spv_material_passes(void* handle,SpvMaterialPass* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle,"Invalid material view");const auto& passes=static_cast<MaterialView*>(handle)->passes;
        require(count==passes.size()&&(!count||output),"Material pass output size mismatch");if(count)std::copy(passes.begin(),passes.end(),output);});
}
SPV_API void* spv_model_read(const std::uint8_t* bytes,std::uint32_t count,std::uint32_t kind) noexcept {
    std::unique_ptr<ModelView> result;
    if(!guarded([&]{result=std::make_unique<ModelView>(bytes,count,kind);}))return nullptr;
    return result.release();
}
SPV_API void spv_model_destroy(void* handle) noexcept {(void)guarded([&]{delete static_cast<ModelView*>(handle);});}
SPV_API void* spv_anim_texture_read(const std::uint8_t* bytes,std::uint32_t count) noexcept {
    std::unique_ptr<AnimTextureView> result;
    if(!guarded([&]{result=std::make_unique<AnimTextureView>(bytes,count);}))return nullptr;
    return result.release();
}
SPV_API void spv_anim_texture_destroy(void* handle) noexcept {(void)guarded([&]{delete static_cast<AnimTextureView*>(handle);});}
SPV_API int spv_anim_texture_info(void* handle,SpvAnimTextureInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Invalid animated texture view");*output=static_cast<AnimTextureView*>(handle)->info;});
}
SPV_API int spv_anim_texture_frames(void* handle,SpvAnimTextureFrame* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle,"Invalid animated texture view");const auto& frames=static_cast<AnimTextureView*>(handle)->frames;
        require(count==frames.size()&&(!count||output),"Animated texture frame output size mismatch");if(count)std::copy(frames.begin(),frames.end(),output);});
}
SPV_API int spv_anim_texture_index(void* handle,float time,std::int32_t* output) noexcept {
    return guarded([&]{require(handle&&output,"Invalid animated texture index request");std::optional<std::size_t> index;
        require(static_cast<AnimTextureView*>(handle)->track.SelectKeyIndexForAnalysis(time,index),"Texture timeline is outside the verified runtime domain");
        *output=index?static_cast<std::int32_t>(*index):-1;});
}
SPV_API int spv_uv_functions_read(const std::uint8_t* bytes,std::uint32_t count,SpvUvFunctions* output) noexcept {
    return guarded([&]{
        require(bytes&&count&&count<=16u*1024u*1024u&&output,"Invalid bounded UV function payload");
        BorrowedInput source(bytes,count);spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);spTransFunctionEval transform;
        spTransFunctionEvalSerializer serializer;std::string error;
        if(!serializer.ReadPayloadForAnalysis(context,source,count,transform,&error))throw std::runtime_error(error);
        SpvUvFunctions result{};
        for(std::size_t i=0;i<7;++i)result.functions[i]=functionForView(transform.GetFunctionsForAnalysis()[i]);
        std::copy(transform.GetPivotForAnalysis().begin(),transform.GetPivotForAnalysis().end(),result.pivot);
        std::copy(transform.GetAxisForAnalysis().begin(),transform.GetAxisForAnalysis().end(),result.axis);*output=result;
    });
}
SPV_API int spv_color_functions_read(const std::uint8_t* bytes,std::uint32_t count,SpvColorFunctions* output) noexcept {
    return guarded([&]{
        require(bytes&&count&&count<=16u*1024u*1024u&&output,"Invalid bounded color function payload");
        BorrowedInput source(bytes,count);spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);std::array<spColorFuncEval,4> colors;spFunctionEval alpha;
        std::string error;
        if(!spMatColorControllerSerializer::ReadEvaluatorsForAnalysis(context,source,count,colors,alpha,&error))throw std::runtime_error(error);
        SpvColorFunctions result{};
        for(std::size_t i=0;i<4;++i)result.colors[i]={colors[i].GetColor1ForAnalysis(),colors[i].GetColor2ForAnalysis(),functionForView(colors[i].GetFunctionForAnalysis())};
        result.alpha=functionForView(alpha);*output=result;
    });
}
SPV_API int spv_model_info(void* handle,SpvModelInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Invalid Model/Skin view");*output=static_cast<ModelView*>(handle)->info;});
}
SPV_API int spv_model_palette_fields(void* handle,SpvSkinPaletteField* output,std::uint32_t capacity,
    std::uint32_t* count) noexcept {
    return guarded([&]{
        require(handle&&count,"Invalid Model/Skin palette observation output");
        const auto& fields=static_cast<ModelView*>(handle)->paletteFields;
        *count=static_cast<std::uint32_t>(fields.size());
        if(!output&&capacity==0)return;
        require(capacity==fields.size()&&(!capacity||output),"Palette observation output size mismatch");
        if(capacity)std::copy(fields.begin(),fields.end(),output);
    });
}
SPV_API int spv_model_bones(void* handle,SpvSkinBone* output,std::uint32_t count) noexcept {
    return guarded([&]{require(handle,"Invalid Model/Skin view");const auto& bones=static_cast<ModelView*>(handle)->bones;
        require(count==bones.size()&&(!count||output),"Skin bone output size mismatch");if(count)std::copy(bones.begin(),bones.end(),output);});
}
SPV_API int spv_static_matrices(const std::uint8_t* bytes,std::uint32_t count,
    const SpvNodeField* fields,std::uint32_t fieldCount,SpvStaticMatrices* output) noexcept {
    return guarded([&]{
        require(bytes&&count&&output&&(!fieldCount||fields)&&fieldCount<=65536,"Invalid bounded StaticRenderObject matrix input");
        spStaticRenderObject object;std::uint32_t mask=0;
        for(std::uint32_t i=0;i<fieldCount;++i) {
            const auto& f=fields[i];
            require((f.field==1||f.field==2)&&f.size==64,"Unsupported StaticRenderObject matrix descriptor");
            require(f.offset<=count&&f.size<=count-f.offset,"StaticRenderObject matrix exceeds input extent");
            BorrowedInput input(bytes+f.offset,f.size);
            require(spStaticRenderObjectSerializer::ReadMatrixFieldForAnalysis(f.field,input,object),"Invalid StaticRenderObject matrix payload");
            mask|=1u<<f.field;
        }
        SpvStaticMatrices result{};
        const auto& world=object.GetWorldMatrixForAnalysis();const auto& inverse=object.GetWorldInverseMatrixForAnalysis();
        std::copy(world.begin(),world.end(),result.world);std::copy(inverse.begin(),inverse.end(),result.inverse);
        result.fieldMask=mask;*output=result;
    });
}
static_assert(sizeof(SpvStaticMatrices)==132);
SPV_API void* spv_static_write_matrix_fields(const float* world,const float* inverse) noexcept {
    std::unique_ptr<SerializedBytes> result;
    if(!guarded([&]{
        require(world&&inverse,"STATIC_MATRIX_INPUT: both matrix values are required");
        // Existing editor finite-input policy only. The original stores two
        // independent raw matrices; no affine/inverse calculation is added.
        for(std::size_t i=0;i<16;++i)
            require(std::isfinite(world[i])&&std::isfinite(inverse[i]),
                "STATIC_MATRIX_INPUT: authoring matrices must be finite");
        spStaticRenderObject object;
        spStaticRenderObject::Matrix4 worldValue{},inverseValue{};
        std::memcpy(worldValue.data(),world,sizeof(worldValue));
        std::memcpy(inverseValue.data(),inverse,sizeof(inverseValue));
        object.SetWorldMatrixForAnalysis(worldValue);
        object.SetWorldInverseMatrixForAnalysis(inverseValue);
        spMemoryStream output;
        require(output.Open("tool.static.matrices"),"Cannot open StaticRenderObject matrix writer stream");
        std::string error;
        // PC450140's empty actual renderable list is independently captured;
        // use the existing writer as-is, without a parallel matrix serializer.
        if(!spStaticRenderObjectSerializer{}.WritePayloadForAnalysis(output,object,&error))
            throw std::runtime_error(error.empty()?"Cannot write StaticRenderObject matrix fields":error);
        result=std::make_unique<SerializedBytes>(output);
    }))return nullptr;
    return result.release();
}
SPV_API int spv_collision_info_values(const std::uint8_t* bytes,std::uint32_t count,
    const SpvNodeField* fields,std::uint32_t fieldCount,SpvCollisionInfoValues* output) noexcept {
    return guarded([&]{
        require(bytes&&count&&output&&(!fieldCount||fields)&&fieldCount<=65536,"Invalid bounded CollisionInfo scalar input");
        spCollisionInfo info;std::array<float,4> quaternion{0,0,0,1};std::uint32_t mask=0;
        for(std::uint32_t i=0;i<fieldCount;++i) {
            const auto& f=fields[i];
            require((f.field==1&&f.size==4)||(f.field==2&&f.size==40),"Unsupported CollisionInfo scalar descriptor");
            require(f.offset<=count&&f.size<=count-f.offset,"CollisionInfo scalar exceeds input extent");
            BorrowedInput input(bytes+f.offset,f.size);
            require(spCollisionInfoSerializer::ReadScalarFieldForAnalysis(f.field,input,info,&quaternion),"Invalid CollisionInfo scalar payload");
            mask|=1u<<f.field;
        }
        SpvCollisionInfoValues result{};
        std::copy(info.GetPositionForAnalysis().begin(),info.GetPositionForAnalysis().end(),result.position);
        std::copy(quaternion.begin(),quaternion.end(),result.rotation);
        std::copy(info.GetScaleForAnalysis().begin(),info.GetScaleForAnalysis().end(),result.scale);
        std::copy(info.GetOrientationForAnalysis().begin(),info.GetOrientationForAnalysis().end(),result.orientation);
        result.group=info.GetGroupForAnalysis();result.fieldMask=mask;*output=result;
    });
}
static_assert(sizeof(SpvCollisionInfoValues)==84);
SPV_API int spv_node_values(const std::uint8_t* bytes,std::uint32_t count,const SpvNodeField* fields,
    std::uint32_t fieldCount,SpvNodeValues* output) noexcept {
    return guarded([&]{
        require(bytes&&count&&output&&(!fieldCount||fields)&&fieldCount<=65536,"Invalid Node field observation input");
        spNode node;spNodeSerializer::ScalarObservationForAnalysis observed;
        for(std::uint32_t i=0;i<fieldCount;++i) {
            const auto& field=fields[i];
            require(field.offset<=count&&field.size<=count-field.offset,"Node scalar field exceeds input");
            BorrowedInput source(bytes+field.offset,field.size);
            spDataBlockHeaderForAnalysis header;header.fieldID=field.field;header.payloadSize=field.size;
            bool handled=false;std::string error;
            if(!spNodeSerializer::ReadScalarFieldForAnalysis(source,header,node,handled,&observed,&error))throw std::runtime_error(error);
            require(handled,"Node scalar observation received a non-scalar field");
        }
        require(node.UpdateWorldForAnalysis(2),"Cannot update isolated Node scalar state");
        *output={};const auto& p=node.GetPositionForAnalysis();const auto& s=node.GetScaleForAnalysis();
        const auto& r=node.GetOrientationForAnalysis();
        std::copy(p.begin(),p.end(),output->position);std::copy(s.begin(),s.end(),output->scale);
        std::copy(r.begin(),r.end(),output->orientation);std::copy(observed.rotation.begin(),observed.rotation.end(),output->rotation);
        output->flags=node.GetFlagsForAnalysis();output->billboard=node.GetBillboardAxisForAnalysis();
        output->bone=observed.bone;output->isStatic=observed.isStatic;output->animated=observed.animated;
    });
}
SPV_API int spv_reference_prefix(const std::uint8_t* bytes,std::uint32_t count,std::uint32_t payloadSize,
    std::uint32_t kind,SpvReferencePrefix* output) noexcept {
    return guarded([&]{
        require(bytes&&output&&kind<=2&&count>=4&&payloadSize>=4,"Invalid reference prefix input");
        *output={}; BorrowedInput source(bytes,std::min(count,payloadSize));
        spSerializer::ReferencePrefixForAnalysis prefix;std::string error;
        if(!spSerializer::ReadReferencePrefixForAnalysis(source,source,prefix,&error))throw std::runtime_error(error);
        require(kind==2?(prefix.id?(payloadSize>=8&&prefix.inlineSize<=payloadSize-8):payloadSize>=4):
            (prefix.id?(payloadSize>=8&&prefix.inlineSize==payloadSize-8):payloadSize==4),
            "Reference extent differs from enclosing field");
        require(!prefix.inlineSize||prefix.inlineSize>=8,"Inline object header is truncated");
        output->id=prefix.id;output->inlineSize=prefix.inlineSize;
        output->encoding=prefix.id?(prefix.inlineSize?2:1):0;
        if(kind>=1) {
            require(count==payloadSize,"Full reference inspection requires the entire payload");
            if(prefix.inlineSize) {
                spSerializerObjectHeaderForAnalysis header;
                require(spSerializer::ReadObjectHeaderForAnalysis(source,header),"Cannot inspect inline object header");
                // Canonical marker is a raw inspector guard, not a factory rule.
                require(header.marker==0x4F4F4253u,"Inline object marker is not SBOO");
                output->classID=header.classID;
            }
        }
    });
}
SPV_API int spv_write_field_header(std::uint32_t field,std::uint32_t payloadSize,std::uint32_t preferredCode,
    std::uint32_t preferExtended,std::uint8_t* output,std::uint32_t capacity,std::uint32_t* size) noexcept {
    return guarded([&]{
        require(field<=spDataBlockSerializer::ExtendedFieldIDLimit&&output&&size&&capacity>=6,"Invalid field header output");
        using Code=spDataBlockSerializer::SizeCode;
        // Host capacity guard for retaining a caller-observed reservation.
        // The original low-level writer trusts it and can truncate a size.
        const bool fits=preferredCode==0?payloadSize==0:preferredCode<=4?payloadSize==(1u<<(preferredCode-1))
            :preferredCode==5?payloadSize<=255:preferredCode==6?payloadSize<=65535:preferredCode==7;
        require(!(fits&&preferExtended&&field<31),"FIELD_HEADER_ESCAPE_UNSUPPORTED: preserving a forced extended ID is not supported");
        HeaderOutput destination(output,capacity);
        if(payloadSize==0) {
            require(!fits||preferredCode==0,"FIELD_HEADER_EMPTY_UNSUPPORTED: original WriteHeader omits real zero-size fields");
            require(spDataBlockSerializer::WriteTerminatorForAnalysis(destination),"Cannot write section terminator");
        } else {
            require(field!=31,"FIELD_HEADER_ID31_UNSUPPORTED: original WriteHeader encodes ID31 incorrectly");
            const auto code=fits?static_cast<Code>(preferredCode):spDataBlockSerializer::SelectSizeCodeForAnalysis(payloadSize);
            require(spDataBlockSerializer::WriteHeaderWithCodeForAnalysis(destination,field,payloadSize,code),"Cannot write original field header");
        }
        require(destination.GetCurrentPosition(*size),"Cannot observe header size");
    });
}
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
            // The original Node reader converts authored quaternions as-is.
            // Finite input validation above is a host guard; unit length is
            // not a game precondition (original PC scalar probe transforms).
            require(input[i].billboard<=2,"Unsupported billboard axis");
            result->orientations.push_back(sparkplug::evidence::pc::animation_math::ToMatrix(q));
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
SPV_API int spv_clip_tag_count(void* handle,std::uint32_t* count) noexcept {
    return guarded([&]{require(count!=nullptr,"Null tag count output");
        *count=static_cast<std::uint32_t>(clip(handle).animation->GetTagsForAnalysis().size());});
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
SPV_API int spv_clip_axis_info(void* handle,std::uint32_t ordinal,std::uint32_t role,std::uint32_t axis,SpvAxisInfo* output) noexcept {
    return guarded([&]{
        auto* track=clip(handle).animation->GetTrackForAnalysis(ordinal);
        require(track&&role<3&&axis<3&&output,"Invalid axis metadata output");*output={};
        const auto* keys=track->GetKeysForAnalysis();require(keys!=nullptr,"Missing prepared track");
        if(const auto& key=(*keys)[role][axis]) {
            output->representation=key->representation;output->keys=static_cast<std::uint32_t>(key->times.size());
            output->values=static_cast<std::uint32_t>(key->values.size());
            output->stride=output->keys?output->values/output->keys:0;
        }
    });
}
SPV_API int spv_clip_axis_values(void* handle,std::uint32_t ordinal,std::uint32_t role,std::uint32_t axis,float* output,std::uint32_t count) noexcept {
    return guarded([&]{
        auto* track=clip(handle).animation->GetTrackForAnalysis(ordinal);
        require(track&&role<3&&axis<3,"Invalid prepared axis");
        const auto* keys=track->GetKeysForAnalysis();require(keys!=nullptr,"Missing prepared track");
        const auto& key=(*keys)[role][axis];const auto size=key?key->values.size():0;
        require(count==size&&(output||!count),"Invalid prepared-value output length");
        if(count)std::memcpy(output,key->values.data(),count*sizeof(float));
    });
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
        auto& s=scene(handle);
        require(floats==s.nodes.size()*16 && (output||!floats),"Invalid world output length");
        s.sample(time);
        for(std::size_t i=0;i<s.nodes.size();++i) {
            auto matrix=s.nodes[i]->GetWorldMatrixForAnalysis();
            for(auto value:matrix) require(std::isfinite(value),"Non-finite world result");
            std::memcpy(output+i*16,matrix.data(),64);
        }
    });
}
SPV_API int spv_scene_pose(void* handle,float time,SpvSample* output,std::uint32_t count) noexcept {
    return guarded([&]{
        auto& s=scene(handle);require(count==s.nodes.size()&&(output||!count),"Invalid world-pose output length");
        s.sample(time);
        for(std::size_t i=0;i<s.nodes.size();++i) {
            const auto& node=*s.nodes[i];const auto& p=node.GetWorldPositionForAnalysis();const auto& scale=node.GetWorldScaleForAnalysis();
            const auto q=sparkplug::evidence::pc::animation_math::FromMatrix(node.GetWorldOrientationForAnalysis());
            for(float v:p)require(std::isfinite(v),"Non-finite world position");
            for(float v:q)require(std::isfinite(v),"Non-finite world rotation");
            for(float v:scale)require(std::isfinite(v),"Non-finite world scale");
            std::copy(p.begin(),p.end(),output[i].position);std::copy(q.begin(),q.end(),output[i].rotation);
            std::copy(scale.begin(),scale.end(),output[i].scale);output[i].validRoles=7;
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

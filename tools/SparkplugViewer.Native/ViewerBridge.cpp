#include "ViewerBridge.h"
#include "ResourceGraph.h"
#include "RenderMeshView.h"
#include "Code/Sparkplug/spAnimationSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spNodeController.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spStaticRenderObject.h"
#include "Code/Sparkplug/spStaticRenderObjectSerializer.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spSkin.h"
#include "Code/Sparkplug/spSkinSerializer.h"
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
static_assert(sizeof(SpvTextureSectionInfo)==32&&sizeof(SpvTextureMip)==28);
struct SerializedBytes {
    std::vector<std::uint8_t> bytes;
    explicit SerializedBytes(spMemoryStream& source) {
        std::uint32_t size=0;require(source.GetSize(&size)&&size&&source.GetBuffer(),"Empty serialized output");
        const auto* data=static_cast<const std::uint8_t*>(source.GetBuffer());bytes.assign(data,data+size);
    }
};
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
SPV_API void* spv_graph_scene(void* handle,const std::uint32_t* ids,std::uint32_t count) noexcept {
    std::unique_ptr<Scene> result;
    if(!guarded([&]{
        require(handle&&count<=16384&&(ids||!count),"Invalid graph scene selection");
        result=std::make_unique<Scene>();result->graph=static_cast<spvhost::GraphHandle*>(handle)->graph;
        std::unordered_map<const spNode*,std::int32_t> indices;
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
    }))return nullptr;
    return result.release();
}
SPV_API void* spv_material_read(const std::uint8_t* bytes,std::uint32_t count) noexcept {
    std::unique_ptr<MaterialView> result;
    if(!guarded([&]{result=std::make_unique<MaterialView>(bytes,count);}))return nullptr;
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
SPV_API void* spv_model_read(const std::uint8_t* bytes,std::uint32_t count,std::uint32_t kind) noexcept {
    std::unique_ptr<ModelView> result;
    if(!guarded([&]{result=std::make_unique<ModelView>(bytes,count,kind);}))return nullptr;
    return result.release();
}
SPV_API void spv_model_destroy(void* handle) noexcept {(void)guarded([&]{delete static_cast<ModelView*>(handle);});}
SPV_API int spv_model_info(void* handle,SpvModelInfo* output) noexcept {
    return guarded([&]{require(handle&&output,"Invalid Model/Skin view");*output=static_cast<ModelView*>(handle)->info;});
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
        require(bytes&&output&&kind<=1&&count>=4&&payloadSize>=4,"Invalid reference prefix input");
        *output={}; BorrowedInput source(bytes,std::min(count,payloadSize));
        spSerializer::ReferencePrefixForAnalysis prefix;std::string error;
        if(!spSerializer::ReadReferencePrefixForAnalysis(source,source,prefix,&error))throw std::runtime_error(error);
        require(prefix.id?(payloadSize>=8&&prefix.inlineSize==payloadSize-8):payloadSize==4,
            "Reference extent differs from enclosing field");
        require(!prefix.inlineSize||prefix.inlineSize>=8,"Inline object header is truncated");
        output->id=prefix.id;output->inlineSize=prefix.inlineSize;
        output->encoding=prefix.id?(prefix.inlineSize?2:1):0;
        if(kind==1) {
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

#pragma once
// Bounded whole-file comparison frontend. Original behavior lives in the
// common engine classes; this does not add a second serializer implementation.
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spRenderNodeSerializer.h"
#include "Code/Sparkplug/spModelSerializer.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spLightDataSerializer.h"
#include "Code/SparkplugDX/spDXLight.h"
#include "Code/Sparkplug/spDXMeshDataSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spMaterialTextureLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/SparkplugDX/spDXMesh.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/SparkplugDX/spDXIndexBuffer.h"
#include "Code/SparkplugDX/spDXVertexBuffer.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace sparkplug::reconstruction::scene_file_test
{
    using Bytes=std::vector<std::uint8_t>;
    inline void Require(bool value,const std::string& message)
    {if(!value)throw std::runtime_error(message);}
    template<class T>void Add(Bytes& bytes,const T& value)
    {const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
    inline std::string Hex(const void* data,std::size_t size)
    {constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(size*2);auto* p=static_cast<const std::uint8_t*>(data);for(std::size_t i=0;i<size;++i){result+=digits[p[i]>>4];result+=digits[p[i]&15];}return result;}
    template<class T>std::string Hex(const T& values){return Hex(values.data(),values.size()*sizeof(values[0]));}
    inline std::uint32_t Identity(const spBaseObject* object){return object?object->vfunc_18().classID:0;}

    inline std::string Capture(const char* path,bool byFileID=false)
    {
        std::ifstream file(path,std::ios::binary|std::ios::ate);Require(bool(file),"Cannot open scene fixture");
        const auto length=file.tellg();Require(length>=36&&length<=65536,"Scene fixture must fit 64 KiB");
        Bytes bytes(static_cast<std::size_t>(length));file.seekg(0);Require(bool(file.read(reinterpret_cast<char*>(bytes.data()),length)),"Cannot read scene fixture");
        spSerializerManager manager;spResourceManager resources;spPCRenderer renderer;
        Require(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),255,3),"Node binding");
        Require(manager.RegisterForAnalysis(spRenderNode::ClassID,std::make_shared<spRenderNodeSerializer>(),255,3),"RenderNode binding");
        Require(manager.RegisterForAnalysis(spModel::ClassID,std::make_shared<spModelSerializer>(),255,3),"Model binding");
        Require(manager.RegisterForAnalysis(spMaterialDataSerializer::TargetClassID,std::make_shared<spMaterialDataSerializer>(),255,3),"MaterialData binding");
        Require(manager.RegisterForAnalysis(spFog::ClassID,std::make_shared<spFogSerializer>(),255,3),"Fog binding");
        Require(manager.RegisterForAnalysis(spLightDataSerializer::TargetClassID,std::make_shared<spLightDataSerializer>(),255,3),"LightData binding");
        Require(manager.RegisterForAnalysis(spMeshDataSerializer::TargetClassID,std::make_shared<spDXMeshDataSerializer>(),2,1),"DX mesh binding");
        Require(manager.RegisterForAnalysis(spMeshDataSerializer::TargetClassID,std::make_shared<spMeshDataSerializer>(),1,1),"Common mesh binding");
        spSerializerReadContextForAnalysis context(manager,resources);context.pcRenderer=&renderer;
        context.captureFileObjectIDsForAnalysis=byFileID;
        spMemoryStream input;Require(input.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"Fixture stream capacity");
        std::memcpy(input.GetBuffer(),bytes.data(),bytes.size());Require(input.Seek(spStream::SeekSource::essStart,0),"Fixture rewind");
        std::string error;auto* root=manager.LoadResourcesForAnalysis(input,context,&error);Require(root&&!context.failed,error);
        std::uint32_t position=0;Require(input.GetCurrentPosition(position)&&position+input.GetLogicalOriginForAnalysis()==bytes.size(),"Whole file extent");
        Require(context.createdObjects.size()<=32,"Bounded snapshot object count");
        std::map<std::uint32_t,const spBaseObject*> objects;
        if(byFileID)
        {
            Require(context.fileObjectsForAnalysis.size()<=32,"Bounded FAT snapshot count");
            for(const auto& entry:context.fileObjectsForAnalysis)
                Require(entry.id&&entry.object&&objects.emplace(entry.id,entry.object).second,"Complete distinct FAT IDs");
        }
        else for(const auto& object:context.createdObjects)
            Require(objects.emplace(Identity(object.get()),object.get()).second,"This directed capture requires distinct runtime classes");
        std::map<const spBaseObject*,std::uint32_t> references;
        for(const auto& [key,object]:objects)references.emplace(object,key); // lowest ID for cache aliases
        const auto Reference=[&](const spBaseObject* object)->std::uint32_t
        {
            if(!object)return 0;auto found=references.find(object);
            Require(found!=references.end(),"Graph edge must refer to a captured object");return found->second;
        };
        for(const auto& object:context.createdObjects)Require(references.count(object.get())!=0,"Every created object is captured");
        std::ostringstream out;out<<"{\"rootClass\":"<<Identity(root);
        if(byFileID)out<<",\"rootID\":"<<Reference(root);
        out<<",\"objects\":{";bool first=true;
        for(const auto& [identity,object]:objects)
        {
            if(!first)out<<',';first=false;
            const auto* named=object->IsKindOf(spNamedObject::ClassID)?dynamic_cast<const spNamedObject*>(object):nullptr;
            const auto* name=named?named->GetName():nullptr;
            out<<'"'<<identity<<"\":{";if(byFileID)out<<"\"classID\":"<<Identity(object)<<',';
            out<<"\"nameHex\":\""<<(name?Hex(name,std::strlen(name)):"")<<"\",\"stateHex\":\"";
            Bytes state;std::vector<std::uint32_t> edges;std::vector<std::string> buffers,layers;
            if(const auto* node=dynamic_cast<const spNode*>(object))
            {
                Add(state,node->GetFlagsForAnalysis());Add(state,node->GetPositionForAnalysis());Add(state,node->GetScaleForAnalysis());Add(state,node->GetOrientationForAnalysis());
                Add(state,node->GetWorldPositionForAnalysis());Add(state,node->GetWorldScaleForAnalysis());Add(state,node->GetWorldOrientationForAnalysis());
                for(std::size_t i=0;i<node->GetChildCountForAnalysis();++i)edges.push_back(Reference(node->GetChildForAnalysis(i)));
                if(const auto* render=dynamic_cast<const spRenderNode*>(node))
                    for(std::size_t i=0;i<render->GetRenderableCountForAnalysis();++i)edges.push_back(Reference(render->GetRenderableForAnalysis(i)));
                if(const auto* light=dynamic_cast<const spDXLight*>(node))
                {
                    Add(state,std::uint32_t(light->GetTypeForAnalysis()));Add(state,light->GetColorForAnalysis());
                    Add(state,std::uint8_t(light->ProjectsShadowVolumeForAnalysis()));
                    Add(state,std::uint8_t(light->UsesAttenuationForAnalysis()));Add(state,std::uint8_t(light->IsLightEnabledForAnalysis()));
                    Add(state,light->GetIntensityForAnalysis());Add(state,light->GetRangeForAnalysis());
                    Add(state,light->GetHotspotAngleForAnalysis());Add(state,light->GetFalloffAngleForAnalysis());
                    // Opaque DC and untouched device-cache words have no serialized value.
                }
            }
            else if(const auto* model=dynamic_cast<const spModel*>(object))
            {
                Add(state,std::uint8_t(model->IsAlphaSortEnabledForAnalysis()));Add(state,model->GetPriorityForAnalysis());Add(state,model->GetProjectionGroupForAnalysis());
                edges={Reference(model->GetMaterialForAnalysis().get()),Reference(model->GetFogForAnalysis().get()),Reference(model->GetBaseMeshForAnalysis().get())};
            }
            else if(const auto* material=dynamic_cast<const spDXMaterial*>(object))
            {
                Add(state,material->GetRenderStatesForAnalysis());Add(state,material->GetVertexAlphaByteForAnalysis());
                Add(state,material->GetDiffuseColorForAnalysis());Add(state,material->GetAmbientColorForAnalysis());
                Add(state,material->GetSpecularColorForAnalysis());Add(state,material->GetEmissiveColorForAnalysis());
                Require(material->HasInitializedSpecularPowerForAnalysis(),"Cannot compare uninitialized native power");Add(state,material->GetSpecularPowerForAnalysis());
                Add(state,std::uint32_t(material->GetPassCountForAnalysis()));
                for(std::size_t i=0;i<material->GetPassCountForAnalysis();++i)
                {
                    const auto* pass=dynamic_cast<spMaterialPassLayer*>(material->GetPassForAnalysis(i));Require(pass!=nullptr,"Supported pass type");
                    Bytes row;Add(row,pass->GetFinalBlendOperationForAnalysis());Add(row,std::uint32_t(pass->GetLayerCountForAnalysis()));
                    for(std::size_t j=0;j<pass->GetLayerCountForAnalysis();++j)
                    {
                        const auto& layer=pass->GetLayerForAnalysis(j);Require(bool(layer)&&bool(layer->GetMaterialTextureForAnalysis()),"Supported material layer");
                        const auto& texture=layer->GetMaterialTextureForAnalysis();Add(row,Identity(layer.get()));
                        for(std::size_t k=0;k<spMaterialTexture::PCTextureStateCount;++k)Add(row,texture->GetTextureStatesForAnalysis()[k]);
                        Add(row,texture->GetUVTransformForAnalysis());Add(row,std::uint8_t(texture->HasStaticTransformForAnalysis()));
                        Require(!texture->GetTextureForAnalysis()&&!texture->GetAnimTextureControllerForAnalysis()&&!texture->GetUVControllerForAnalysis(),"This capture has no external texture/controller edges");
                    }
                    layers.push_back(Hex(row));
                }
            }
            else if(const auto* fog=dynamic_cast<const spFog*>(object))
            {Add(state,std::uint32_t(fog->GetTypeForAnalysis()));Add(state,fog->GetColorARGBForAnalysis());Add(state,fog->GetStartForAnalysis());Add(state,fog->GetEndForAnalysis());Add(state,fog->GetDensityForAnalysis());}
            else if(const auto* mesh=dynamic_cast<const spDXMesh*>(object))
            {
                Add(state,mesh->GetVertexComponentFlagsForAnalysis());Add(state,mesh->GetPrimitiveCountForAnalysis());Add(state,mesh->GetVertexCountForAnalysis());
                Add(state,mesh->GetIndexByteSizeForAnalysis());Add(state,mesh->GetVertexByteSizeForAnalysis());Add(state,mesh->GetFVFCodeForAnalysis());Add(state,mesh->GetVertexStrideForAnalysis());Add(state,mesh->GetIndexBeginForAnalysis());Add(state,mesh->GetVertexBeginForAnalysis());
                Require(mesh->GetDXIndexBufferForAnalysis()&&mesh->GetDXVertexBufferForAnalysis()&&mesh->GetVertexDeclarationForAnalysis(),"Complete mesh buffers and declaration");
                buffers={Hex(mesh->GetDXIndexBufferForAnalysis()->GetDataForAnalysis()),Hex(mesh->GetDXVertexBufferForAnalysis()->GetDataForAnalysis()),Hex(mesh->GetVertexDeclarationForAnalysis()->GetElementsForAnalysis())};
            }
            else throw std::runtime_error("Uncaptured runtime class");
            out<<Hex(state)<<"\",\"edges\":[";for(std::size_t i=0;i<edges.size();++i){if(i)out<<',';out<<edges[i];}
            out<<"],\"buffers\":[";for(std::size_t i=0;i<buffers.size();++i){if(i)out<<',';out<<'"'<<buffers[i]<<'"';}
            out<<"],\"layers\":[";for(std::size_t i=0;i<layers.size();++i){if(i)out<<',';out<<'"'<<layers[i]<<'"';}out<<"]}";
        }
        out<<"}}";return out.str();
    }
}

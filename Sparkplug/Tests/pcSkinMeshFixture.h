#pragma once
#include "Code/Sparkplug/spDXMeshDataSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkplugDX/spDXMesh.h"
#include "Code/SparkplugDX/spDXVertexBuffer.h"
#include "Code/SparkplugDX/spDXIndexBuffer.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <sstream>
#include <stdexcept>
namespace sparkplug::tests::skin
{
    inline std::string MeshHex(const void* data,std::size_t count)
    {
        const auto* bytes=static_cast<const unsigned char*>(data);constexpr char digits[]="0123456789abcdef";std::string result;
        for(std::size_t i=0;i<count;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;
    }
    inline std::shared_ptr<reconstruction::spDXMesh> ReadMesh(const std::string& input,
        reconstruction::spPCRenderer& renderer,std::string& capture)
    {
        using namespace reconstruction;spMemoryStream stream;
        if(input.size()%2||!stream.ResizeAndSetSize(static_cast<unsigned>(input.size()/2)))throw std::runtime_error("bounded mesh input");
        auto* bytes=static_cast<unsigned char*>(stream.GetBuffer());
        for(std::size_t i=0;i<input.size();i+=2)bytes[i/2]=static_cast<unsigned char>(std::stoul(input.substr(i,2),nullptr,16));
        spSerializerManager manager;spResourceManager resources;manager.SetDispatchContextForAnalysis(2,1);
        spSerializerReadContextForAnalysis context(manager,resources);context.pcRenderer=&renderer;spDXMeshDataSerializer serializer;
        auto object=serializer.ReadObjectHeaderAndCreateForAnalysis(stream);auto* mesh=dynamic_cast<spDXMesh*>(object.get());std::string error;
        if(!mesh||!serializer.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(input.size()/2-8),*mesh,&error))throw std::runtime_error(error);
        unsigned cursor=0;if(!stream.GetCurrentPosition(cursor)||cursor!=input.size()/2)throw std::runtime_error("complete mesh field consumption");
        const auto& vertices=mesh->GetDXVertexBufferForAnalysis()->GetDataForAnalysis();
        const auto& indices=mesh->GetDXIndexBufferForAnalysis()->GetDataForAnalysis();
        const auto& elements=mesh->GetVertexDeclarationForAnalysis()->GetElementsForAnalysis();
        std::ostringstream out;out<<"[["<<mesh->GetVertexComponentFlagsForAnalysis()<<','<<mesh->GetFVFCodeForAnalysis()<<','<<mesh->GetVertexStrideForAnalysis()<<','
            <<mesh->GetPrimitiveCountForAnalysis()<<','<<mesh->GetVertexCountForAnalysis()<<','<<mesh->GetIndexBeginForAnalysis()<<','<<mesh->GetVertexBeginForAnalysis()<<','<<mesh->GetComponentWeightCountForAnalysis()
            <<"],\""<<MeshHex(vertices.data(),vertices.size())<<"\",\""<<MeshHex(indices.data(),indices.size())<<"\",[\""<<MeshHex(elements.data(),elements.size()*sizeof(elements[0]))<<"\"]]";
        capture=out.str();return std::shared_ptr<spDXMesh>(static_cast<spDXMesh*>(object.release()));
    }
}

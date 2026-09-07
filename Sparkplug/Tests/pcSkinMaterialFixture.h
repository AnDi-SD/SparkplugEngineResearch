#pragma once
#include "pcSkinMeshFixture.h"
#include "Code/Sparkplug/spMaterialDataSerializer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialPassLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
namespace sparkplug::tests::skin
{
    inline std::string CaptureMaterial(const reconstruction::spDXMaterial& value)
    {
        using namespace reconstruction;const auto* material=&value;
        if(material->GetPassCountForAnalysis()!=1)throw std::runtime_error("one decoded material pass");
        auto* pass=dynamic_cast<spMaterialPassLayer*>(material->GetPassForAnalysis(0));
        if(!pass||pass->GetLayerCountForAnalysis()!=2)throw std::runtime_error("two actual decoded Std layers");
        std::ostringstream out;out<<"[[";bool first=true;
        for(auto value:material->GetRenderStatesForAnalysis()){if(!first)out<<',';first=false;out<<value;}out<<"],[";first=true;
        const auto word=[&](float value){unsigned bits;std::memcpy(&bits,&value,4);if(!first)out<<',';first=false;out<<bits;};
        for(const auto* color:{&material->GetDiffuseColorForAnalysis(),&material->GetAmbientColorForAnalysis(),&material->GetSpecularColorForAnalysis(),&material->GetEmissiveColorForAnalysis()})for(float value:*color)word(value);
        word(material->GetSpecularPowerForAnalysis());out<<"],"<<pass->GetFinalBlendOperationForAnalysis()<<",[";
        for(unsigned i=0;i<2;++i)
        {
            auto* layer=dynamic_cast<const spStdLayer*>(pass->GetLayerForAnalysis(i).get());
            if(!layer)throw std::runtime_error("actual Std layer type");
            const auto& states=layer->GetMaterialTextureForAnalysis()->GetTextureStatesForAnalysis();
            // PC material field17 serializes nine words; the host carrier
            // also has three analytical slots outside this wire prefix.
            if(i)out<<',';out<<'"'<<MeshHex(states.data(),36)<<'"';
        }
        out<<"]]";return out.str();
    }
    inline std::unique_ptr<reconstruction::spDXMaterial> ReadMaterial(const std::string& input,std::string& capture)
    {
        using namespace reconstruction;spMemoryStream stream;
        if(input.size()<16||input.size()%2||!stream.ResizeAndSetSize(static_cast<unsigned>(input.size()/2)))throw std::runtime_error("bounded material input");
        auto* bytes=static_cast<unsigned char*>(stream.GetBuffer());
        for(std::size_t i=0;i<input.size();i+=2)bytes[i/2]=static_cast<unsigned char>(std::stoul(input.substr(i,2),nullptr,16));
        (void)spStdLayer::StaticRTTI();spSerializerManager manager;spResourceManager resources;
        manager.SetDispatchContextForAnalysis(2,1);spSerializerReadContextForAnalysis context(manager,resources);
        spMaterialDataSerializer serializer;auto object=serializer.ReadObjectHeaderAndCreateForAnalysis(stream);
        auto* material=dynamic_cast<spDXMaterial*>(object.get());std::string error;
        if(!material||!serializer.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(input.size()/2-8),*material,&error))throw std::runtime_error(error);
        unsigned cursor=0;if(!stream.GetCurrentPosition(cursor)||cursor!=input.size()/2)throw std::runtime_error("complete material fields");
        capture=CaptureMaterial(*material);return std::unique_ptr<spDXMaterial>(static_cast<spDXMaterial*>(object.release()));
    }
}

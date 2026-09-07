#pragma once
#include "Code/Sparkplug/spDXTextureDataSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <sstream>
#include <stdexcept>
namespace sparkplug::tests::skin
{
    inline std::shared_ptr<reconstruction::spDXTexture> ReadTexture(const std::string& input,std::string& capture)
    {
        using namespace reconstruction;spMemoryStream stream;
        if(input.size()%2||!stream.ResizeAndSetSize(static_cast<unsigned>(input.size()/2)))throw std::runtime_error("bounded texture input");
        auto* bytes=static_cast<unsigned char*>(stream.GetBuffer());
        for(std::size_t i=0;i<input.size();i+=2)bytes[i/2]=static_cast<unsigned char>(std::stoul(input.substr(i,2),nullptr,16));
        spSerializerManager manager;spResourceManager resources;manager.SetDispatchContextForAnalysis(2,1);
        spSerializerReadContextForAnalysis context(manager,resources);
        context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t row)noexcept{return row+4;};
        auto texture=std::make_shared<spDXTexture>();std::string error;
        if(!spDXTextureDataSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(input.size()/2),*texture,&error))throw std::runtime_error(error);
        unsigned cursor=0;
        if(!stream.GetCurrentPosition(cursor)||cursor!=input.size()/2||texture->GetMipsForAnalysis().size()!=1||
            texture->HasInitializedRuntimeFormatForAnalysis()||texture->GetNativeByteCountForAnalysis())throw std::runtime_error("complete native texture reader state");
        const auto& mip=texture->GetMipsForAnalysis()[0];constexpr char digits[]="0123456789abcdef";std::string pixels;
        for(auto value:mip.packedBytes){const auto b=static_cast<unsigned char>(value);pixels+=digits[b>>4];pixels+=digits[b&15];}
        std::ostringstream out;out<<"[["<<texture->GetField18ForAnalysis()<<','<<unsigned(texture->GetField1CForAnalysis())<<','
            <<texture->GetTextureFlagsForAnalysis()<<','<<texture->IsInitializedForAnalysis()<<','<<texture->GetWidthForAnalysis()<<','<<texture->GetHeightForAnalysis()
            <<"],\""<<pixels<<"\"]";capture=out.str();return texture;
    }
}

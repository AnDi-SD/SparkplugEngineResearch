#include "Code/SparkplugDX/spDXShaderLayer.h"
#include "Code/Sparkplug/spMaterialTexture.h"
#include "Code/Sparkplug/spStdLayer.h"
#include "Code/Sparkplug/spMaterialSerializer.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
    std::string Snapshot(const spDXShaderLayer& object)
    {
        std::ostringstream out;out<<'['<<object.GetShaderWordForAnalysis().value_or(0)<<','<<std::boolalpha<<bool(object.GetMaterialTextureForAnalysis())<<",[";
        bool firstPair=true;for(const auto& pair:object.GetParameterPairsForAnalysis())
        {
            if(!firstPair)out<<',';firstPair=false;out<<'[';bool firstArray=true;
            for(const auto* array:{&pair.first,&pair.second})
            {if(!firstArray)out<<',';firstArray=false;out<<'[';bool firstWord=true;for(const auto& row:*array)for(auto word:row){if(!firstWord)out<<',';firstWord=false;out<<word;}out<<']';}
            out<<']';
        }
        out<<"]]";return out.str();
    }
    std::string Run(const std::string& mode)
    {
        spDXShaderLayer object;Check(!object.GetShaderWordForAnalysis().has_value(),"factory leaves shader word unknown, NOT NULL");
        Check(!object.GetMaterialTextureForAnalysis()&&object.GetParameterPairsForAnalysis().empty(),"actual factory no nested texture or arrays");
        Check(object.IsKindOf(spMaterialTextureLayer::ClassID)&&!object.IsKindOf(spStdLayer::ClassID),"actual shader RTTI is not StdLayer");
        object.SetShaderWordForAnalysis(0x12345678);if(mode=="clone-texture")object.SetMaterialTextureForAnalysis(std::make_unique<spMaterialTexture>());
        const unsigned count=mode=="clone-multi"?3:mode=="clone-one"||mode=="clear"?1:0;
        for(unsigned i=0;i<count;++i)
        {
            spDXShaderLayer::ParameterPairForAnalysis pair;pair.first.resize(i+1);pair.second.resize(3-i);
            for(unsigned side=0;side<2;++side)
            {auto& array=side?pair.second:pair.first;for(unsigned j=0;j<array.size()*4;++j)array[j/4][j%4]=0x3f000000u+i*64+side*8+j;}
            object.AppendParameterPairForAnalysis(std::move(pair));
        }
        std::ostringstream states;states<<'['<<Snapshot(object);
        if(mode.rfind("clone-",0)==0)
        {
            auto result=object.Clone();auto* clone=dynamic_cast<spDXShaderLayer*>(result.get());
            Check(clone&&Snapshot(*clone)==Snapshot(object),"native shader-word/nested/parameter clone values");
            if(count)
            {
                Check(clone->GetParameterPairsForAnalysis().data()!=object.GetParameterPairsForAnalysis().data(),"independent parameter vector");
                for(unsigned i=0;i<count;++i)Check(clone->GetParameterPairsForAnalysis()[i].first.data()!=object.GetParameterPairsForAnalysis()[i].first.data()&&clone->GetParameterPairsForAnalysis()[i].second.data()!=object.GetParameterPairsForAnalysis()[i].second.data(),"both arrays independently owned");
            }
            if(mode=="clone-texture")Check(clone->GetMaterialTextureForAnalysis().get()!=object.GetMaterialTextureForAnalysis().get(),"inherited deep texture copy");
            states<<','<<Snapshot(*clone);
        }
        if(mode=="clear")
        {object.ClearParameterPairsForAnalysis();Check(object.GetParameterPairsForAnalysis().empty()&&object.GetParameterPairsForAnalysis().capacity()==0,"clear releases vector storage too");states<<','<<Snapshot(object);}
        if(mode=="stubs")Check(object.Slot8ForAnalysis(0x123,0x456)&&object.Slot9ForAnalysis()&&object.ReaderScalarStubForAnalysis(0xdeadbeef),"actual no-op success stubs");
        states<<']';return "[\""+mode+"\","+states.str()+']';
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--shader"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const char* mode:{"factory","clone-empty","clone-one","clone-multi","clone-texture","clear","stubs"})Run(mode);
        spMaterialSerializer::MaterialWriteShape shape;spMaterialSerializer::PassWriteShape pass;spMaterialSerializer::LayerWriteShape layer;
        layer.classID=spDXShaderLayer::ClassID;pass.layers.push_back(layer);shape.passes.push_back(pass);
        Check(!spMaterialSerializer::BuildStandardWritePlanForAnalysis(shape).valid,"shader must NOT become accepted standard layer");
        auto factory=spRTTIManager::Instance().Create(spDXShaderLayer::ClassID);Check(dynamic_cast<spDXShaderLayer*>(factory.get())!=nullptr,"actual factory has portable counterpart");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": shader lifetime/storage; no shader codec/GPU claim\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

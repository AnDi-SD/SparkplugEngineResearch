#include "Code/Sparkplug/spSkyBox.h"
#include "Code/Sparkplug/spModel.h"
#include "Code/Sparkplug/spFog.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
    std::string World(const spNode& node)
    {
        const auto position=node.GetWorldPositionForAnalysis(),scale=node.GetWorldScaleForAnalysis();
        const auto orientation=node.GetWorldOrientationForAnalysis();
        std::ostringstream out;
        const auto bytes=[&out](const auto& array){static const char hex[]="0123456789abcdef";
            const auto* p=reinterpret_cast<const unsigned char*>(array.data());
            for(std::size_t i=0;i<sizeof(array);++i)out<<hex[p[i]>>4]<<hex[p[i]&15];};
        bytes(position);bytes(scale);bytes(orientation);return out.str();
    }
    std::string Capture()
    {
        spNode root;auto sky=std::make_shared<spSkyBox>();
        Check(root.AttachChildForAnalysis(sky),"actual Node owns SkyBox");
        root.SetPositionForAnalysis({10,20,30});root.SetScaleForAnalysis({2,3,4});
        root.SetOrientationForAnalysis({0,1,0,-1,0,0,0,0,1});
        sky->SetPositionForAnalysis({1,2,3});sky->SetOrientationForAnalysis({1,0,0,0,0,1,0,-1,0});
        root.MarkLocalTransformDirtyForAnalysis();sky->MarkLocalTransformDirtyForAnalysis();
        Check(root.UpdateWorldForAnalysis()&&sky->UpdateWorldForAnalysis(),"inherited actual world update");
        const auto first=World(*sky);
        sky->SetOrientationForAnalysis({1,0,0,0,1,0,0,0,1});
        Check((sky->GetFlagsForAnalysis()&1u)==0,"raw local-field setter leaves clean dirty bit unchanged");
        Check(sky->UpdateWorldForAnalysis(),"clean base still copies local orientation");
        return "[\""+first+"\",\""+World(*sky)+"\"]";
    }
    struct DrawState{spSkyBox* sky;spFog* fog;bool called=false;};
    bool Draw(void* opaque,spRenderNode& node,bool force)
    {
        auto& state=*static_cast<DrawState*>(opaque);state.called=true;
        Check(&node==state.sky&&!force,"dedicated pass uses BASE support force=false");
        Check(!node.GetRenderableForAnalysis(0)->GetFogForAnalysis()&&
            !node.GetRenderableForAnalysis(1)->GetFogForAnalysis(),"fog cleared before base gate");
        Check(node.GetRenderableForAnalysis(0)->IsAlphaSortEnabledForAnalysis(),"alpha-sort not changed");
        return false;
    }
    void Render()
    {
        spSkyBox sky;auto first=std::make_shared<spModel>(),second=std::make_shared<spModel>();
        auto fog=std::make_shared<spFog>();first->SetFogForAnalysis(fog);second->SetFogForAnalysis(fog);
        first->SetAlphaSortEnabledForAnalysis(true);
        Check(sky.AttachRenderableForAnalysis(first)&&sky.AttachRenderableForAnalysis(second),"real model membership");
        Check(sky.RenderNormalForAnalysis()&&first->GetFogForAnalysis()==fog,"ordinary draw true/no-op");
        sky.SetEnabledForAnalysis(false);DrawState state{&sky,fog.get()};
        Check(!sky.RenderSkyForAnalysis(&Draw,&state)&&state.called,"disabled sky still clears fog and propagates base result");
        Check(fog.use_count()==1,"two fog references released, external reference retained");
        Check(!sky.RenderSkyForAnalysis(nullptr,nullptr),"missing host backend is unavailable");
        Check(sky.IsExactly(spSkyBox::ClassID)&&sky.IsKindOf(spRenderNode::ClassID),"actual RTTI hierarchy");
    }
}
int main(int argc,char** argv)
{
    try{const auto capture=Capture();if(argc==2&&std::string(argv[1])=="--world"){std::cout<<capture<<'\n';return 0;}
        Render();std::cout<<"PASS "<<checks<<'/'<<checks<<": SkyBox world and separate draw\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

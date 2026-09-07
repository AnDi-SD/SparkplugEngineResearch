#include "Code/Sparkplug/spSceneManager.h"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using M=spSceneManager; int checks=0;
    void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
    unsigned Bits(float value){unsigned bits;std::memcpy(&bits,&value,4);return bits;}
    template<class T>void Array(std::ostream& out,const T& data)
    {out<<'[';bool first=true;for(const auto& v:data){if(!first)out<<',';first=false;out<<v;}out<<']';}
    struct Root:spNode
    {
        std::function<bool(unsigned)> callback;
        bool UpdateWorldForAnalysis(unsigned flags,const Matrix3* camera)noexcept override
        {return callback?callback(flags):spNode::UpdateWorldForAnalysis(flags,camera);}
    };
    std::string Run(const std::string& mode)
    {
        auto manager=std::make_unique<M>();std::array<Root,3> roots;std::array<M::SceneForAnalysis,3> scenes;
        std::vector<std::array<unsigned,3>> trace;bool callbackValid=true;std::vector<unsigned> cloneCapture,remaining;
        Check(M::GetInstance()==manager.get()&&manager->GetCurrentSceneForAnalysis()==nullptr,"constructor singleton/current");
        for(unsigned i=0;i<3;++i)
        {
            auto& root=roots[i];root.SetPositionForAnalysis({float(i+1),float(2*i+2),float(3*i+3)});
            root.MarkLocalTransformDirtyForAnalysis();root.SetEnabledForAnalysis(false);scenes[i].systemRoot=&root;
            root.callback=[&,i](unsigned flags)
            {
                callbackValid&=flags==0&&manager->GetCurrentSceneForAnalysis()==&scenes[i];
                trace.push_back({i+1,i+1,flags});
                if(trace.size()==1&&mode=="append")callbackValid&=manager->RegisterSceneForAnalysis(scenes[2]);
                if(trace.size()==1&&mode=="remove-next")callbackValid&=manager->UnregisterSceneForAnalysis(scenes[1]);
                if(mode=="world-three")return roots[i].spNode::UpdateWorldForAnalysis(flags);
                return mode!="false-root";
            };
        }
        const unsigned initial=mode=="empty"?0:mode=="append"?2:3;
        for(unsigned i=0;i<initial;++i)Check(manager->RegisterSceneForAnalysis(scenes[i]),"prepared borrowed scene list");
        if(mode=="clone")
        {
            spCloneManager clones;auto cloneBase=clones.Clone(*manager);auto* clone=dynamic_cast<M*>(cloneBase.get());
            Check(clone!=nullptr,"actual manager clone type");
            cloneCapture={unsigned(M::GetInstance()==clone),unsigned(clone->GetSceneCountForAnalysis()),unsigned(clone->GetCurrentSceneForAnalysis()==nullptr)};
            manager.reset();cloneCapture.push_back(unsigned(M::GetInstance()==nullptr));cloneBase.reset();
        }
        else
        {
            Check(manager->UpdateWorldForAnalysis()==(mode!="false-root"),"host reports root safety failure without short-circuit");
            Check(callbackValid&&manager->GetCurrentSceneForAnalysis()==nullptr,"current scene during and after callbacks");
            for(auto* scene:manager->GetScenesForAnalysis())remaining.push_back(unsigned(scene-scenes.data())+1);
            manager.reset();Check(M::GetInstance()==nullptr,"destructor clears singleton");
        }
        std::ostringstream out;out<<"[\""<<mode<<"\",[";
        for(std::size_t i=0;i<trace.size();++i){if(i)out<<',';Array(out,trace[i]);}out<<"],[";
        for(unsigned i=0;i<3;++i)
        {
            if(i)out<<',';std::vector<unsigned> words;
            for(const auto& value:roots[i].GetWorldPositionForAnalysis())words.push_back(Bits(value));
            for(const auto& value:roots[i].GetWorldScaleForAnalysis())words.push_back(Bits(value));Array(out,words);
        }
        out<<"],";Array(out,remaining);out<<",0,";Array(out,cloneCapture);out<<']';return out.str();
    }
    void Guards()
    {
        M manager;Root root;M::SceneForAnalysis scene{&root},missing;
        Check(manager.StaticRTTI().classID==M::ClassID,"original class identity");
        Check(!manager.RegisterSceneForAnalysis(missing),"host null-root guard");
        Check(manager.RegisterSceneForAnalysis(scene)&&!manager.RegisterSceneForAnalysis(scene),"host duplicate guard");
        bool guarded=false;root.callback=[&](unsigned){guarded=!manager.UnregisterSceneForAnalysis(scene)&&!manager.UpdateWorldForAnalysis();return true;};
        Check(manager.UpdateWorldForAnalysis()&&guarded,"current-removal and reentry guards preserve outer traversal");
        Check(manager.UnregisterSceneForAnalysis(scene)&&!manager.UnregisterSceneForAnalysis(scene),"unregister membership guard");
        Check(manager.UpdateWorldForAnalysis(),"empty traversal after unregister");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--case"){std::cout<<Run(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"empty","world-three","append","remove-next","false-root","clone"})(void)Run(mode);
        Guards();std::cout<<"PASS "<<checks<<'/'<<checks<<": scene manager borrowed-list/world/clone\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

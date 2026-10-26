#include "Code/wxGameFlowNavigation.h"
#include "Code/SparkBase/spNotification.h"
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
using namespace winx::reconstruction;

namespace
{
    void Check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
    float Float(std::uint32_t word) { float value;std::memcpy(&value,&word,4);return value; }
    struct Widget
    {
        const char* name;
        std::uint8_t enabled=1;
        std::array<std::array<char,64>,4> links{};
    };
    struct Host final : wxGameFlowStateHost, wxGameFlowNavigationHost
    {
        wxGameFlowResourcesForAnalysis resources;
        std::array<Widget,5> widgets{{{"start"},{"d0"},{"d1"},{"d2"},{"d3"}}};
        Widget* initial=&widgets[0];
        Widget* selected=&widgets[0];
        std::vector<std::string> trace;
        std::uint32_t now=1200;
        float magnitude=1,angle=0;
        int inputToken=1,windowToken=2,receiverToken=3;
        void* receiver=nullptr;
        std::function<void()> onNotify,onSelect;
        std::vector<spNotificationForAnalysis> notifications;
        unsigned findCount=0;
        std::function<Widget*(const char*)> findOverride;
        Host() { Mode("directions"); }
        void Mode(const std::string& mode)
        {
            widgets[0].enabled=mode=="self-cycle"?0:1;
            for(unsigned d=0;d<4;++d)
            {
                const std::string value=mode=="directions"?widgets[d+1].name:
                    mode=="self-cycle"?"start":mode=="empty-substring"?"xemptyx":
                    mode=="missing"?"absent":mode=="empty"?"empty":throw std::invalid_argument("mode");
                widgets[0].links[d].fill(0);
                std::memcpy(widgets[0].links[d].data(),value.c_str(),value.size()+1);
            }
        }
        void* BorrowInputForAnalysis() override { return &inputToken; }
        spTaskTimer::ClockReadingForAnalysis ReadClockForAnalysis() override { return {now,1}; }
        wxGameFlowResourcesForAnalysis& ResourcesForAnalysis() override { return resources; }
        float ReadNavigationMagnitudeForAnalysis(void* input) override
        { Check(input==&inputToken,"Magnitude uses cached input18");return magnitude; }
        float ReadNavigationAngleForAnalysis(void* input) override
        { Check(input==&inputToken,"Angle uses cached input18");return angle; }
        void* ResolveNavigationWindowForAnalysis(std::uint32_t view) override
        { trace.push_back("W:"+std::to_string(view));return &windowToken; }
        void* ReadInitialNavigationWidgetForAnalysis(void* window) override
        { Check(window==&windowToken,"Initial widget window");return initial; }
        void* ReadSelectedNavigationWidgetForAnalysis(void* window) override
        { Check(window==&windowToken,"Selected widget window");return selected; }
        const char* ReadNavigationLinkForAnalysis(void* widget,unsigned direction) override
        { return static_cast<Widget*>(widget)->links.at(direction).data(); }
        void* FindNavigationWidgetForAnalysis(void* window,const char* name) override
        {
            Check(window==&windowToken,"Lookup window");
            ++findCount;trace.push_back(std::string("F:")+name);
            if(findOverride)return findOverride(name);
            for(auto& widget:widgets)if(std::strcmp(widget.name,name)==0)return &widget;
            return nullptr;
        }
        std::uint8_t ReadNavigationWidgetEnabledForAnalysis(void* widget) override
        { return static_cast<Widget*>(widget)->enabled; }
        void* ResolveNavigationNotificationReceiverForAnalysis() override { return receiver; }
        void DispatchNotificationForAnalysis(void* target,const spNotificationForAnalysis& message) override
        {
            Check(target==receiver,"Opaque notification receiver");notifications.push_back(message);
            if(onNotify)onNotify();
        }
        void SelectNavigationWidgetForAnalysis(void* window,void* widget) override
        {
            Check(window==&windowToken,"Selection window");
            selected=static_cast<Widget*>(widget);trace.push_back(std::string("S:")+selected->name);
            if(onSelect)onSelect();
        }
    };
    void Emit()
    {
        std::string line;
        while(std::getline(std::cin,line))
        {
            std::istringstream row(line);
            std::uint32_t angleBits,magnitudeBits,elapsed,deadline,released;
            std::string mode;
            if(!(row>>std::hex>>angleBits>>magnitudeBits>>std::dec>>elapsed>>deadline>>released>>mode))
                throw std::invalid_argument("Expected angle/magnitude hex, elapsed/deadline/released and mode");
            Host host;
            wxGameFlowState object(host);
            host.Mode(mode);host.angle=Float(angleBits);host.magnitude=Float(magnitudeBits);
            host.now=1200+elapsed;
            object.MutableStateForAnalysis().field34=deadline;
            object.MutableStateForAnalysis().field38=static_cast<std::uint8_t>(released);
            NavigateGameFlowPCForAnalysis(object,3,host,[&]{return host.ReadClockForAnalysis();});
            std::cout<<object.TimerForAnalysis().GetStateForAnalysis().currentMilliseconds<<' '
                <<object.GetStateForAnalysis().field34<<' '<<unsigned(object.GetStateForAnalysis().field38)<<' '
                <<host.selected->name;
            for(const auto& event:host.trace)std::cout<<' '<<event;
            std::cout<<'\n';
        }
    }
    void TestCallbackOrderAndFallback()
    {
        Host host;
        wxGameFlowState object(host);
        host.initial=nullptr;
        host.receiver=&host.receiverToken;
        host.onNotify=[&]
        {
            auto state=object.TimerForAnalysis().GetStateForAnalysis();
            state.currentMilliseconds=800;
            object.TimerForAnalysis().SetStateForAnalysis(state);
        };
        host.onSelect=[&]
        {
            auto state=object.TimerForAnalysis().GetStateForAnalysis();
            state.currentMilliseconds=1700;
            object.TimerForAnalysis().SetStateForAnalysis(state);
        };
        host.now=2200;
        NavigateGameFlowPCForAnalysis(object,3,host,[&]{return host.ReadClockForAnalysis();});
        Check(host.trace==std::vector<std::string>({"W:3","W:3","W:3","F:d0","W:3","S:d0"}),
            "Fallback and selection resolve fresh windows in native order");
        Check(object.GetStateForAnalysis().field34==2200,"Deadline reads timer after both callbacks");
        Check(host.notifications.size()==1,"One directed notification");
        const auto& n=host.notifications[0];
        Check(n.code==0x27D1&&n.sender==&object&&n.payload==0x10&&n.tail==0
            &&n.word04==0&&n.word08==0&&n.word0C==0&&n.word14==0,"Exact eight logical notification words");
    }
    void TestFollowingLinksAndGuards()
    {
        Host host;
        wxGameFlowState object(host);
        host.widgets[1].enabled=0;
        std::strcpy(host.widgets[1].links[0].data(),"d1");
        host.now=2200;
        NavigateGameFlowPCForAnalysis(object,3,host,[&]{return host.ReadClockForAnalysis();});
        Check(host.selected==&host.widgets[2]&&host.findCount==3,"Disabled candidate causes a second lookup then follows its link");
        Host cycle;
        wxGameFlowState guarded(cycle);
        cycle.widgets[1].enabled=0;
        std::strcpy(cycle.widgets[1].links[0].data(),"d0");
        cycle.now=2200;
        bool rejected=false;
        try { NavigateGameFlowPCForAnalysis(guarded,3,cycle,[&]{return cycle.ReadClockForAnalysis();}); }
        catch(const std::logic_error&){rejected=true;}
        Check(rejected&&cycle.findCount==8192,"Foreign noninitial cycle hits only the declared host guard");
        Check(guarded.GetStateForAnalysis().field38==1,"Guard does not manufacture native completed flag write");
        Check(ClassifyGameFlowDirectionPCForAnalysis(Float(0x4016CBE4))==3,
            "Positive three-q float store lies inside PC direction3");
        Check(ClassifyGameFlowDirectionPCForAnalysis(Float(0xC016CBE4))==2,
            "Negative three-q float store lies inside PC direction2");
        Check(!ClassifyGameFlowDirectionPCForAnalysis(0.5f,{0.25f,1.0f}),"Independent global bounds can leave a gap");
        bool bad=false;
        try { (void)ClassifyGameFlowDirectionPCForAnalysis(std::numeric_limits<float>::quiet_NaN()); }
        catch(const std::domain_error&){bad=true;}
        Check(bad,"Unqualified angle is an explicit portable refusal");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--emit"){Emit();return 0;}
        TestCallbackOrderAndFallback();TestFollowingLinksAndGuards();
        std::cout<<"GameFlow navigation checks passed\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

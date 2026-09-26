#include "Code/wxAppHelper.h"
#include "Analysis/PC/wxAppHelperAbi.h"
#include "Analysis/PS2/wxAppHelperAbi.h"
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace winx::reconstruction;
namespace
{
    constexpr std::uint32_t services[] = {0x74e060,0x755270,0x755274,0x755278,0x75527c,0x755280,
        0x755284,0x75528c,0x755290,0x755294,0x755298,0x75529c,0x7552a0,0x75ac60,
        0x75db78,0x75db8c,0x75db9c,0x764e8c,0x765acc,0x765ad4,0x765ad8,0x765adc,
        0x765ae0,0x765ae4,0x765ae8,0x765aec,0x765af0,0x765af4,0x765af8,0x765b00,
        0x765b04,0x765b68,0x765bc0,0x765bcc,0x765bf4,0x765bf8,0x765c04,0x765c08};
    constexpr std::uintptr_t Self = 0x3403f000, File = 0x3403e000;
    void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
    struct Host : wxAppHelperHost
    {
        std::map<std::uint32_t,Handle> objects;
        std::map<std::pair<Handle,std::uint32_t>,std::uint32_t> words;
        std::map<std::pair<Handle,std::uint32_t>,Handle> pointers;
        std::ostringstream trace;
        std::optional<std::string> ini;
        bool open = true, ready = true, tree = true;
        std::uint32_t language = 3;
        bool assets = false;
        bool recreateGame = false;
        wxAppHelper* destroyHelper = nullptr;
        Host() { for (std::size_t i=0;i<std::size(services);++i) objects[services[i]]=0x34000000+i*0x1000; objects[0x755290]=Self; }
        Handle ResolveForAnalysis(std::uint32_t id, bool create) override
        {
            auto it=objects.find(id); Check(it!=objects.end(),"unknown service");
            if (!it->second && create) throw std::logic_error("fixture service absent");
            return it->second;
        }
        void ClearServiceForAnalysis(std::uint32_t id) override { objects[id]=0; }
        Handle Getter(std::uint32_t op)
        {
            switch(op) {
            case 0x408000:return objects[0x74e060];case 0x4e4150:return objects[0x765af0];
            case 0x40dff0:return objects[0x75528c];case 0x4e4090:return objects[0x765ad8];
            case 0x4ef6c0:return objects[0x765b04];case 0x40e050:return objects[0x7552a0];
            case 0x4db290:return objects[0x765ad4];case 0x4e40d0:return objects[0x765ae0];
            case 0x4e4170:return objects[0x765af4];case 0x4e4130:return objects[0x765aec];
            case 0x512150:return objects[0x765b00];case 0x4e40f0:return objects[0x765ae4];
            case 0x4f4e90:return objects[0x765af8];
            default:return 0; }
        }
        void Log(std::uint32_t op,Handle h,std::initializer_list<Handle> args)
        { trace<<op<<':'<<h;for(auto a:args)trace<<':'<<a;trace<<';'; }
        Handle InvokeForAnalysis(std::uint32_t op,Handle h,std::initializer_list<Handle> args) override
        {
            if(auto v=Getter(op))return v;
            if(op==0x5975b0)h=Self;
            if(op==0x40fb60){Log(op,h,{Self});return 0;}
            Log(op,h,args);
            if(op==0x592b70)language=static_cast<std::uint32_t>(*args.begin());
            if(op==0x419e10)return File+0x100;
            if(op==0x422b50)return tree?File+0x200:0;
            if(op==0x599120)return File+0x300;
            if(op==0x45d930)return File+0x600;
            return 0;
        }
        Handle InvokeVirtualForAnalysis(std::uint32_t slot,Handle h,std::initializer_list<Handle> args) override
        {
            Log(0x80000000|slot,h,args);
            if(slot==0&&h==objects[0x765ad4]&&recreateGame)objects[0x755294]=0x34009000;
            if(slot==0&&h==Self&&destroyHelper){auto* dying=destroyHelper;destroyHelper=nullptr;delete dying;}
            return 0;
        }
        Handle InvokeTextForAnalysis(std::uint32_t op,Handle h,std::string_view s,std::initializer_list<Handle> args) override
        {
            trace<<op<<':'<<h<<':'<<s;for(auto a:args)trace<<':'<<a;trace<<';';
            return op==0x592e00&&open?File:0;
        }
        Handle ReadHandleForAnalysis(Handle h,std::uint32_t off) override
        {
            if(auto it=pointers.find({h,off});it!=pointers.end())return it->second;
            if(h==File+0x600&&off==0x1d4)return File+0x700;
            if(h==objects[0x755294]&&off>=0xe8&&off<=0x12c)return h+0x800+(off-0xe8)*4;
            if(h==objects[0x765ad4]&&off==0x2b4)return h+0x800;
            return 0;
        }
        std::uint32_t ReadWordForAnalysis(Handle h,std::uint32_t off) override
        {
            if(auto it=words.find({h,off});it!=words.end())return it->second;
            if(h==objects[0x755280]&&off==0x18)return language;
            if(h==objects[0x755294]&&off==0x1ac)return 0xffffffff;
            if(h==objects[0x755274]&&off==0x7c)return 0x3c800000;
            return words[{h,off}];
        }
        std::uint8_t ReadByteForAnalysis(Handle h,std::uint32_t off) override
        { return h==objects[0x75528c]&&off==0x345?static_cast<std::uint8_t>(ready):static_cast<std::uint8_t>(words[{h,off}]); }
        void WriteWordForAnalysis(Handle h,std::uint32_t off,std::uint32_t v) override { words[{h,off}]=v; }
        void WriteByteForAnalysis(Handle h,std::uint32_t off,std::uint8_t v) override { words[{h,off}]=v; }
        Handle InteriorForAnalysis(Handle h,std::uint32_t off) override { return h+off; }
        std::optional<std::string> ReadConfigurationForAnalysis(std::string_view name) override
        { Check(name=="winx.ini","ini filename");return ini; }
        std::uint32_t GetAssetKindForAnalysis(std::uint32_t i) override { return assets&&i==7?79:0; }
        std::string GetAssetNameForAnalysis(std::uint32_t i) override { Check(i==7,"asset index");return "line7"; }
        std::string ResolveAssetPathForAnalysis(std::string_view name,std::uint32_t c,std::uint32_t g,std::uint32_t cap) override
        { Check(c==17&&g==0&&cap==500,"asset resolver arguments");trace<<"path:"<<name<<';';return std::string(name)+"/"+std::to_string(language); }
        bool CheckFileForAnalysis(std::string_view path) override { trace<<"check:"<<path<<';';return open; }
        void ReportMissingFileForAnalysis(std::string_view path) override { trace<<"missing:"<<path<<';'; }
        void ReportStateMemoryForAnalysis(std::int32_t state,double m) override { trace<<"memory:"<<state<<':'<<m<<';'; }
    };
    std::string Decode(std::string s)
    {
        if(s=="-")return {};
        std::string result;
        for(std::size_t i=0;i<s.size();i+=2)result.push_back(static_cast<char>(std::stoul(s.substr(i,2),nullptr,16)));
        return result;
    }
    void Config(std::ostream& out,const wxAppHelper::Configuration& c)
    {
        out<<c.showCinematics<<','<<c.fullScreen<<','<<c.useGamePad<<','<<c.enableSound<<','<<c.enableDialog<<','<<c.enableShadows<<','
            <<c.loadFromPCK<<','<<c.buildPCK<<','<<c.firstPCKLevel<<','<<c.loadFromCD<<','<<c.language<<','<<c.displayType<<','
            <<c.territory<<','<<c.startLevel<<','<<c.unknown64<<','<<c.unknown68<<','<<c.testCinematic<<','<<c.cinematicToTest;
    }
    std::string Run(const std::string& line)
    {
        Host host;wxAppHelper helper(host);wxAppHelper::Configuration config;helper.SetConfigurationForAnalysis(config);
        std::istringstream in(line);std::ostringstream result;std::string op;
        while(in>>op)
        {
            std::uint32_t a,b;std::string text;
            if(op=="c") {in>>text;helper.ParseConfigurationForAnalysis(Decode(text));Config(result,helper.GetConfigurationForAnalysis());result<<'|';}
            else if(op=="C") {helper.LoadConfigurationForAnalysis();Config(result,helper.GetConfigurationForAnalysis());result<<'|';}
            else if(op=="p") {in>>a;Config(result,wxAppHelper::PS2ConfigurationForAnalysis(a));result<<'|';}
            else if(op=="t") {in>>a>>text;text=Decode(text);result<<(a==0?helper.ParseBooleanForAnalysis(text):a==1?helper.ParseLanguageForAnalysis(text):a==2?helper.ParseDisplayTypeForAnalysis(text):helper.ParseTerritoryForAnalysis(text))<<'|';}
            else if(op=="f") {in>>a>>b;config.buildPCK=a!=0;config.loadFromPCK=b!=0;helper.SetConfigurationForAnalysis(config);}
            else if(op=="n") {in>>text;text=Decode(text);std::string out="sentinel";helper.FormatPackagePathForAnalysis(text,out);result<<out<<'|';}
            else if(op=="o") {in>>a>>text;helper.OpenPackageForAnalysis(Decode(text),a);}
            else if(op=="k") {in>>text;helper.ClosePackageForAnalysis(Decode(text));}
            else if(op=="s") {in>>a;result<<helper.RunStageForAnalysis(a)<<'|';}
            else if(op=="i")result<<helper.InitializeForAnalysis()<<'|';
            else if(op=="m")helper.InitializeMenuForAnalysis();
            else if(op=="r")helper.ResetMenuForAnalysis();
            else if(op=="e")result<<helper.ResetSessionForAnalysis()<<'|';
            else if(op=="u") {in>>a>>b;helper.GetRuntimeForAnalysis().active=a!=0;helper.GetRuntimeForAnalysis().countdown=b;result<<helper.UpdateForAnalysis()<<'|';}
            else if(op=="v") {in>>a;host.ready=a!=0;}
            else if(op=="x") {in>>a;host.open=a!=0;}
            else if(op=="y") {in>>a;host.tree=a!=0;}
            else if(op=="l") {in>>a;std::vector<std::string> names;while(a--){in>>text;names.push_back(Decode(text));}helper.LoadResourceTreesForAnalysis(names);}
            else if(op=="b")helper.BuildInGamePackageForAnalysis();
            else if(op=="a")helper.CheckOneLinersForAnalysis();
            else if(op=="A")host.assets=true;
            else if(op=="R")host.recreateGame=true;
            else if(op=="h") {in>>a;for(auto id:{0x755274u,0x75db8cu,0x755284u})host.pointers[{host.objects[id],id==0x755284?0x2d4:0x18}]=a?File+0x500:0;}
            else if(op=="g") {in>>a;host.pointers[{host.objects[0x755294],0x18+a*4}]=File+0x400;host.words[{File+0x400,0x14}]=1;}
            else if(op=="d")
            {
                std::uint32_t current,loops;in>>a>>b>>current>>loops;
                config.firstPCKLevel=a;config.buildPCK=true;helper.SetConfigurationForAnalysis(config);helper.GetRuntimeForAnalysis().active=true;
                auto game=host.objects[0x755294];host.words[{game,0x1b0}]=b;host.words[{game,0x1ac}]=current;
                if(current!=0xffffffff)host.pointers[{game,0x15c+4*current}]=File+0x500;
                while(loops--)helper.BuildPackagesStepForAnalysis();
            }
            else if(op=="z")helper.ShutdownForAnalysis();
            else throw std::runtime_error("bad protocol");
        }
        const auto& s=helper.GetRuntimeForAnalysis();
        result<<s.field14<<','<<s.field18<<','<<s.field7C<<','<<s.active<<','<<s.countdown<<','<<s.menuReady<<'|'
            <<host.words[{0x34013000,0x514}]<<','<<host.words[{0x34017000,0x2c}]<<','<<host.words[{0,0x741654}]<<'|'<<host.trace.str();
        auto output=result.str();
        for(std::size_t i=0;(i=output.find('\n',i))!=std::string::npos;i+=2)output.replace(i,1,"\\n");
        return output;
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--protocol") {std::string line;while(std::getline(std::cin,line))std::cout<<Run(line)<<'\n';return 0;}
        Host host;wxAppHelper helper(host);
        Check(helper.IsExactly(wxAppHelper::ClassID),"RTTI");
        bool guarded=false;try{helper.GetConfigurationForAnalysis();}catch(const std::logic_error&){guarded=true;}Check(guarded,"uninitialized settings");
        helper.ParseConfigurationForAnalysis("language=FR\nshowCinematics=false\nloadFromPCK=True\n");
        Check(helper.GetConfigurationForAnalysis().language==1&&!helper.GetConfigurationForAnalysis().showCinematics,"configuration");
        std::string path;helper.FormatPackagePathForAnalysis("CT1_test",path);Check(path=="DATA\\PCK\\1\\CT1_test.pck","path");
        sparkplug::reconstruction::spCloneManager clones;
        auto clone=helper.vfunc_10(clones);Check(clone&&wxAppHelper::GetInstance()==clone.get(),"clone singleton");
        clone.reset();Check(!wxAppHelper::GetInstance(),"unconditional singleton clear");
        Check(Run("m r").find(",0|")!=std::string::npos,"menu lifecycle");
        Check(Run("u 1 2").find(",1,1,0|")!=std::string::npos,"countdown");
        {
            Host shutdownHost;shutdownHost.destroyHelper=new wxAppHelper(shutdownHost);
            shutdownHost.destroyHelper->SetConfigurationForAnalysis({});
            shutdownHost.destroyHelper->ShutdownForAnalysis();
            Check(!shutdownHost.destroyHelper&&!wxAppHelper::GetInstance(),"self deletion completes shutdown");
            Check(!shutdownHost.objects[0x765bf8],"shutdown continues after helper deletion");
        }
        std::cout<<"wxAppHelper checks passed\n";
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

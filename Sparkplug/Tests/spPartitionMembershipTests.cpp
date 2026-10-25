#include "Code/Sparkplug/spPartitionNode.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spOcclusionVolume.h"
#include "Code/Sparkplug/spOctreeNode.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace sparkplug::reconstruction;
namespace
{
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class T> struct Fixture
{
    static constexpr bool Dynamic=std::is_same_v<T,spRenderNode>;
    std::array<std::unique_ptr<spPartitionNode>,2> roots;
    std::array<std::unique_ptr<T>,3> items;
    Fixture(){for(auto& root:roots)root=std::make_unique<spPartitionNode>();for(auto& item:items)item=std::make_unique<T>();}
    ~Fixture()
    {
        // Explicitly repair the deliberately broken notify=false test state
        // through real non-notifying storage bodies before ordinary teardown.
        for(auto& item:items)if(item)item->ClearPartitionsForAnalysis(false);
        for(auto& root:roots)if(root)Clear(*root,false);
    }
    static const auto& Values(const spPartitionNode& root)
    {if constexpr(Dynamic)return root.GetDynamicObjectsForAnalysis();else return root.GetOccludersForAnalysis();}
    static bool Storage(const spPartitionNode& root)
    {if constexpr(Dynamic)return root.HasDynamicStorageForAnalysis();else return root.HasOccluderStorageForAnalysis();}
    static void Clear(spPartitionNode& root,bool notify)
    {if constexpr(Dynamic)root.ClearDynamicsForAnalysis(notify);else root.ClearOccludersForAnalysis(notify);}
    static bool Add(spPartitionNode& root,T* item)
    {if constexpr(Dynamic)return root.InsertDynamicForAnalysis(item);else return root.InsertOccluderForAnalysis(item);}
    static void Remove(spPartitionNode& root,T* item,bool notify)
    {if constexpr(Dynamic)root.RemoveDynamicForAnalysis(item,notify);else root.RemoveOccluderForAnalysis(item,notify);}
    template<class P,std::size_t N> static int Id(const std::array<std::unique_ptr<P>,N>& values,const P* pointer)
    {for(unsigned i=0;i<N;++i)if(values[i].get()==pointer)return int(i);throw std::runtime_error("unrepresented pointer");}
    void State() const
    {
        std::cout<<"{\"roots\":[";
        for(unsigned i=0;i<roots.size();++i){if(i)std::cout<<',';if(!roots[i])std::cout<<"null";else{std::cout<<'[';bool first=true;for(auto* value:Values(*roots[i])){if(!first)std::cout<<',';first=false;std::cout<<Id(items,value);}std::cout<<']';}}
        std::cout<<"],\"items\":[";
        for(unsigned i=0;i<items.size();++i){if(i)std::cout<<',';if(!items[i])std::cout<<"null";else{std::cout<<'[';bool first=true;for(auto* value:items[i]->GetPartitionsForAnalysis()){if(!first)std::cout<<',';first=false;std::cout<<Id(roots,value);}std::cout<<']';}}
        std::cout<<"],\"rootStorage\":[";for(unsigned i=0;i<roots.size();++i){if(i)std::cout<<',';if(roots[i])std::cout<<Storage(*roots[i]);else std::cout<<"null";}
        std::cout<<"],\"itemStorage\":[";for(unsigned i=0;i<items.size();++i){if(i)std::cout<<',';if(items[i])std::cout<<items[i]->HasPartitionStorageForAnalysis();else std::cout<<"null";}std::cout<<"]}";
    }
    void Command(const std::string& name,unsigned a,unsigned b,bool notify)
    {
        Check(a<3&&b<3,"bounded membership indices");
        if(name=="add"){Check(a<2&&roots[a]&&items[b],"live add input");Check(Add(*roots[a],items[b].get()),"actual reciprocal insertion");}
        else if(name=="remove"){Check(a<2&&roots[a]&&items[b],"live remove input");Remove(*roots[a],items[b].get(),notify);}
        else if(name=="clear"){Check(a<2&&roots[a],"live clear input");Clear(*roots[a],notify);}
        else if(name=="reverse-remove"){Check(b<2&&items[a]&&roots[b],"live reverse-remove input");items[a]->RemovePartitionForAnalysis(roots[b].get(),notify);}
        else if(name=="reverse-clear"){Check(bool(items[a]),"live reverse-clear input");items[a]->ClearPartitionsForAnalysis(notify);}
        else if(name=="delete-item")items[a].reset();
        else if(name=="delete-root"){Check(a<2,"root id");roots[a].reset();}
        else throw std::runtime_error("unknown membership operation");
    }
};
template<class T> void Batch()
{
    std::size_t count;Check(bool(std::cin>>count)&&count<=64,"bounded operation count");Fixture<T> fixture;std::cout<<'[';
    for(std::size_t i=0;i<count;++i){std::string op;unsigned a,b,notify;Check(bool(std::cin>>op>>a>>b>>notify)&&notify<=1,"operation input");fixture.Command(op,a,b,notify!=0);if(i)std::cout<<',';fixture.State();}std::cout<<"]\n";
}
template<class T> void Directed()
{
    Fixture<T> f;auto* a=f.items[0].get();auto* b=f.items[1].get();auto* c=f.items[2].get();auto* root=f.roots[0].get();
    for(auto* value:{a,b,c,a})Check(f.Add(*root,value),"duplicates are actual reciprocal borrowed links");
    Check(f.Values(*root)==std::vector<T*>{a,b,c,a}&&a->GetPartitionsForAnalysis()==std::vector<spPartitionNode*>{root,root},"both list orders retained");
    f.Remove(*root,b,true);Check(f.Values(*root)==std::vector<T*>{a,a,c}&&b->GetPartitionsForAnalysis().empty(),"first-match swap-last then opposite remove");
    f.Clear(*root,true);Check(f.Values(*root).empty()&&f.Storage(*root)&&a->GetPartitionsForAnalysis().empty()&&c->GetPartitionsForAnalysis().empty(),"notify clear drains both sides and retains own storage");
    Check(f.Add(*root,a),"new relation after notifying clear");f.Clear(*root,false);
    Check(f.Values(*root).empty()&&!f.Storage(*root)&&a->GetPartitionsForAnalysis()==std::vector<spPartitionNode*>{root},"nonnotify frees own storage while leaving stale reverse link");a->ClearPartitionsForAnalysis(false);
    Check(f.Add(*root,b),"destructor relation");f.items[1].reset();Check(f.Values(*root).empty(),"actual source item teardown unregisters itself");
    Check(f.Add(*root,a),"partition destructor relation");f.roots[0].reset();Check(a->GetPartitionsForAnalysis().empty(),"actual partition teardown unregisters borrowed item");
    spOctreeNode unsupported;Check(!f.Add(unsupported,a),"missing derived insertion is refused, not base success");
}
}
int main(int argc,char** argv){try{
    if(argc==3&&std::string(argv[1])=="--membership-batch")
    {if(std::string(argv[2])=="dynamic")Batch<spRenderNode>();else if(std::string(argv[2])=="occluder")Batch<spOcclusionVolume>();else return 2;return 0;}
    Directed<spRenderNode>();Directed<spOcclusionVolume>();std::cout<<"PASS "<<checks<<'/'<<checks<<": reciprocal partition membership\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

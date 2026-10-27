#include "Code/Sparkplug/spGUIManager.h"
#include "Code/Sparkplug/spEditBox.h"
#include "Code/Sparkplug/spTextNode.h"
#include "Code/Sparkplug/spTextRenderable.h"
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spFontManager.h"
#include "Code/Sparkplug/spFontSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks=0,comparisons=0,nativeCases=0,focusCalls=0,emptyDtors=0;
    void Check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
    void Match(std::uint32_t got,std::uint32_t expected,const char* reason)
    {++comparisons;if(got!=expected)std::cerr<<reason<<" actual="<<std::hex<<got<<" expected="<<expected<<std::dec<<'\n';Check(got==expected,reason);}
    std::uint32_t Bits(float value){std::uint32_t word;std::memcpy(&word,&value,4);return word;}
    struct Reader
    {
        std::ifstream input;
        explicit Reader(const char* path):input(path,std::ios::binary){Check(bool(input),"Open GUIManager fixture");}
        std::uint32_t U32(){std::uint32_t value=0;input.read(reinterpret_cast<char*>(&value),4);Check(bool(input),"GUI fixture word");return value;}
        std::string Blob(){const auto size=U32();Check(size<=65536,"GUI fixture blob bound");std::string value(size,0);if(size)input.read(value.data(),size);Check(bool(input),"GUI fixture blob extent");return value;}
    };
    struct Host final : spGUIHost
    {
        spGUIManager* manager=nullptr;
        std::map<spNode*,spEditBox*> nodeOwners;
        std::array<spBaseObject*,3> targets{};
        unsigned controlCode=0,controlFocus=0,referenceOperations=0,unresolved=0;
        bool running=false,controlFired=false;
        bool entryMode=false,entryDisable=false,entryMutateMessage=false;
        unsigned entryOwner=0;
        spEditBox* entryReceiver=nullptr;
        spNotificationForAnalysis* entryFrame=nullptr;
        spGUIObject** entryOutput=nullptr;
        std::array<spGUIObject*,3> entryOwners{};
        std::map<const spGUIObject*,spGUIObject*> ownerBindings;
        std::vector<spGUIObject*> ownerCalls;
        std::vector<const void*> ownerFrames;
        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void DestroyEntityForAnalysis(spEntity&) noexcept override {}
        std::uint16_t GetReferenceCountForAnalysis(void*) override {++referenceOperations;return 4;}
        void SetReferenceCountForAnalysis(void*,std::uint16_t) override {++referenceOperations;}
        void DeleteReferenceForAnalysis(void*) override {++referenceOperations;}
        void SubscribeGUINodeForAnalysis(spNode&,spGUIObject&) override {}
        void UnsubscribeGUINodeForAnalysis(spNode&,spGUIObject&) noexcept override {}
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override {}
        bool IsGUINodeEnabledForAnalysis(const spNode& node) override {return node.IsEnabledForAnalysis();}
        void SetGUINodeEnabledForAnalysis(spNode& node,std::uint8_t value,bool recursive) override
        {
            if(running&&controlCode&&!controlFired)
            {
                const auto found=nodeOwners.find(&node);Check(found!=nodeOwners.end(),"Known Node-effect receiver");
                const unsigned code=found->second->GetHighlightByteForAnalysis()?0x16:0x17;
                if(code==controlCode)
                {
                    if(entryMode)
                    {
                        if(entryOwner)ownerBindings[entryReceiver]=entryOwners.at(entryOwner);
                        if(entryDisable)entryReceiver->SetEnabledByteForAnalysis(0);
                        if(entryMutateMessage){entryFrame->code=0x18;entryFrame->payload=reinterpret_cast<std::uintptr_t>(entryOutput);}
                    }
                    else manager->SetFocusStateForFixture(targets.at(controlFocus));
                    controlFired=true;
                }
            }
            node.SetEnabledForAnalysis(value!=0,recursive);
        }
        void* GetGUINotificationTargetForAnalysis(const spGUIObject& object) override
        {const auto found=ownerBindings.find(&object);return found==ownerBindings.end()?nullptr:found->second;}
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t) override {throw std::logic_error("No input event in Focus profile");}
        void DispatchNotificationForAnalysis(void* target,const spNotificationForAnalysis& notification) override
        {
            if(!entryMode)throw std::logic_error("Focus directly calls the real receiver");
            auto* owner=static_cast<spGUIObject*>(target);Check(owner&& (owner==entryOwners[1]||owner==entryOwners[2]),"Actual typed GUIObject owner");
            ownerCalls.push_back(owner);ownerFrames.push_back(&notification);owner->vfunc_0C(&notification);
        }
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&,const void*,const char*) noexcept override {++unresolved;}
    };
    std::shared_ptr<spFont> ReadFont(const std::string& bytes,spFontManager& manager)
    {
        spSerializerManager serializers;spResourceManager resources;spSerializerReadContextForAnalysis context(serializers,resources);context.fontManager=&manager;
        auto font=std::make_shared<spFont>();spMemoryStream stream;std::string error;
        Check(stream.ResizeAndSetSize(static_cast<unsigned>(bytes.size())),"Font stream allocate");std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart,0),"Font stream rewind");
        Check(spFontSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<unsigned>(bytes.size()),*font,&error),error.c_str());
        manager.SetLayoutDefaultFontForAnalysis(font.get());return font;
    }
    struct Graph final
    {
        std::array<std::shared_ptr<spNode>,4> gui{};
        std::array<std::shared_ptr<spTextNode>,4> nodes{};
        std::array<std::shared_ptr<spTextRenderable>,4> texts{};
        std::shared_ptr<spNode> cursor=std::make_shared<spNode>();
        std::unique_ptr<spEditBox> editor;
        Graph(Host& host,spFontManager& manager,const std::shared_ptr<spFont>& font,unsigned enabled,unsigned highlight)
        {
            std::string error;
            for(unsigned i=1;i<=3;++i)
            {
                gui[i]=std::make_shared<spNode>();nodes[i]=std::make_shared<spTextNode>();nodes[i]->SetPositionForAnalysis({float(10*i),float(2*i),float(3*i)});
                texts[i]=std::make_shared<spTextRenderable>();Check(texts[i]->SetFontForAnalysis(font,&manager,&error),error.c_str());
                Check(texts[i]->SetTextForAnalysis("AB",&manager,&error),error.c_str());Check(nodes[i]->AttachRenderableForAnalysis(texts[i]),"Actual shared TextNode attachment");
            }
            cursor->SetPositionForAnalysis({99,8,77});editor=std::make_unique<spEditBox>(host);
            editor->SetSelectionStateForAnalysis(1,1,0,nullptr,{gui[1].get(),gui[2].get(),gui[3].get()},0);
            editor->SetTextGraphForFixture({nodes[1].get(),nodes[2].get(),nodes[3].get()},nodes[1].get(),cursor.get());
            Check(editor->SetTextForAnalysis("A",&error),error.c_str());const unsigned selected=highlight?3:1;
            editor->SetSelectionStateForAnalysis(static_cast<std::uint8_t>(enabled),1,static_cast<std::uint8_t>(highlight),gui[selected].get(),{gui[1].get(),gui[2].get(),gui[3].get()},selected-1);
            editor->SetTextGraphForFixture({nodes[1].get(),nodes[2].get(),nodes[3].get()},nodes[selected].get(),cursor.get());
            for(unsigned i=1;i<=3;++i){host.nodeOwners[gui[i].get()]=editor.get();host.nodeOwners[nodes[i].get()]=editor.get();}host.nodeOwners[cursor.get()]=editor.get();
        }
        std::vector<std::uint32_t> State() const
        {
            auto role=[](auto* value,const auto& array){for(unsigned i=0;i<array.size();++i)if(array[i].get()==value)return i;throw std::logic_error("Known GUI graph role");};
            std::vector<std::uint32_t> result{editor->GetEnabledByteForAnalysis(),editor->GetHighlightByteForAnalysis(),role(editor->GetSelectedNodeForAnalysis(),gui),editor->GetSelectionIndexForAnalysis(),role(editor->GetCurrentTextNodeForAnalysis(),nodes)};
            for(unsigned i=1;i<=3;++i)result.push_back(gui[i]->GetFlagsForAnalysis());for(unsigned i=1;i<=3;++i)result.push_back(nodes[i]->GetFlagsForAnalysis());
            for(auto value:cursor->GetPositionForAnalysis())result.push_back(Bits(value));result.push_back(cursor->GetFlagsForAnalysis());return result;
        }
        std::vector<long> References() const
        {std::vector<long> result;for(unsigned i=1;i<=3;++i){result.push_back(gui[i].use_count());result.push_back(nodes[i].use_count());result.push_back(texts[i].use_count());}result.push_back(cursor.use_count());return result;}
    };
    using Graphs=std::array<std::unique_ptr<Graph>,3>;
    unsigned Role(spBaseObject* pointer,const Graphs& graphs)
    {if(!pointer)return 0;for(unsigned i=1;i<=2;++i)if(graphs[i]&&graphs[i]->editor.get()==pointer)return i;throw std::logic_error("Known focus role");}
    void MatchEditors(Reader& in,const Graphs& graphs)
    {
        const auto count=in.U32();Check(count<=2,"Editor count bound");unsigned actual=0;for(unsigned i=1;i<=2;++i)if(graphs[i])++actual;Match(actual,count,"Actual Editor graph count");
        for(unsigned i=1;i<=count;++i)for(auto value:graphs[i]->State())Match(value,in.U32(),"Actual shared Editor state");
    }
    std::vector<long> References(const Graphs& graphs)
    {std::vector<long> result;for(unsigned i=1;i<=2;++i)if(graphs[i]){auto values=graphs[i]->References();result.insert(result.end(),values.begin(),values.end());}return result;}
    std::vector<std::uint32_t> States(const Graphs& graphs)
    {std::vector<std::uint32_t> result;for(unsigned i=1;i<=2;++i)if(graphs[i]){auto values=graphs[i]->State();result.insert(result.end(),values.begin(),values.end());}return result;}
    void ConfigureState(Graph& graph,unsigned visible,unsigned highlight)
    {
        auto& object=*graph.editor;
        object.SetSelectionStateForAnalysis(object.GetEnabledByteForAnalysis(),static_cast<std::uint8_t>(visible),static_cast<std::uint8_t>(highlight),object.GetSelectedNodeForAnalysis(),object.GetStateNodesForAnalysis(),object.GetSelectionIndexForAnalysis());
    }
    void FocusProfile(Reader& in,const std::string& fontBytes,unsigned family)
    {
        const auto old=in.U32();std::array<unsigned,2> enabled{},visible{1,1};unsigned oldHighlight=1,disableSelected=0,controlCode=0,controlFocus=0,factoryOnly=0;
        std::vector<unsigned> targets;
        if(family==0){targets.push_back(in.U32());for(auto& value:enabled)value=in.U32();factoryOnly=in.U32();}
        else
        {
            for(auto& value:enabled)value=in.U32();for(auto& value:visible)value=in.U32();oldHighlight=in.U32();disableSelected=in.U32();controlCode=in.U32();controlFocus=in.U32();
            const auto count=in.U32();Check(count<=16,"Focus sequence bound");for(unsigned i=0;i<count;++i)targets.push_back(in.U32());
        }
        Host host;spFontManager fontManager;auto font=ReadFont(fontBytes,fontManager);auto manager=std::make_unique<spGUIManager>();host.manager=manager.get();
        Match(manager->GetFocusForAnalysis()==nullptr,in.U32(),"Factory null focus");Match(spGUIManager::GetInstanceForAnalysis()==manager.get(),in.U32(),"Factory singleton publication");
        Graphs graphs;
        if(!factoryOnly)for(unsigned i=1;i<=2;++i){graphs[i]=std::make_unique<Graph>(host,fontManager,font,enabled[i-1],old==i?1:0);ConfigureState(*graphs[i],visible[i-1],old==i?oldHighlight:0);host.targets[i]=graphs[i]->editor.get();}
        Check(old<=2&&(!old||graphs[old]),"Known old focus input");manager->SetFocusStateForFixture(old?graphs[old]->editor.get():nullptr);
        if(disableSelected){Check(old!=0,"Disabled selected input requires old Editor");graphs[old]->gui[3]->SetEnabledForAnalysis(false,true);}
        const auto references=References(graphs);const auto refCalls=host.referenceOperations;host.controlCode=controlCode;host.controlFocus=controlFocus;
        auto invoke=[&](unsigned target)
        {
            Check(target<=2&&(!target||graphs[target]),"Known desired focus input");host.running=true;
            Check(manager->SetFocusForAnalysis(target?graphs[target]->editor.get():nullptr)==spGUIFocusResultForAnalysis::Completed,"Whole actual typed focus profile completed");
            host.running=false;Check(host.unresolved==0,"No suppressed Editor dependency refusal");++focusCalls;
        };
        if(family==0)
        {
            MatchEditors(in,graphs);if(!factoryOnly)invoke(targets[0]);MatchEditors(in,graphs);
            Match(Role(manager->GetFocusForAnalysis(),graphs),in.U32(),"Final focus role");
        }
        else
        {
            const auto steps=in.U32();Match(static_cast<unsigned>(targets.size()),steps,"Whole focus step count");
            for(auto target:targets){invoke(target);Match(Role(manager->GetFocusForAnalysis(),graphs),in.U32(),"Focus after controlled callback");MatchEditors(in,graphs);}
            Match(host.controlFired,in.U32(),"Controlled Node-entry mutation fired");
        }
        Match(References(graphs)==references&&host.referenceOperations==refCalls,in.U32(),"Borrowed ownership roles unchanged");
        Check(spGUIManager::GetInstanceForAnalysis()==manager.get(),"Focus preserves singleton");
        // Portable harness cleanup is outside the Native comparison window;
        // native graph teardown was not performed or promoted into evidence.
    }
    void LifetimeProfile(Reader& in,const std::string& fontBytes)
    {
        const auto mode=in.Blob();Host host;spFontManager fontManager;auto font=ReadFont(fontBytes,fontManager);
        auto a=std::make_unique<spGUIManager>();spGUIManager* first=a.get();host.manager=a.get();
        Match(a->GetFocusForAnalysis()==nullptr,in.U32(),"Lifecycle factory focus0");Match(spGUIManager::GetInstanceForAnalysis()==a.get(),in.U32(),"Lifecycle factory singleton");
        Graphs graphs;if(mode!="two-fresh-new-first"){graphs[1]=std::make_unique<Graph>(host,fontManager,font,1,1);a->SetFocusStateForFixture(graphs[1]->editor.get());}
        std::unique_ptr<spGUIManager> b;spGUIManager* second=nullptr;bool copyReturn=false;unsigned clonePair=0;
        if(mode=="copy-keep-target")
        {b=std::make_unique<spGUIManager>();graphs[2]=std::make_unique<Graph>(host,fontManager,font,1,0);b->SetFocusStateForFixture(graphs[2]->editor.get());spCloneManager manager;copyReturn=a->vfunc_14(*b,manager);Check(a->GetFocusForAnalysis()==graphs[1]->editor.get()&&b->GetFocusForAnalysis()==graphs[2]->editor.get(),"Inherited literal Copy retains both focus pointers");}
        else if(mode=="clone-clear-focus")
        {spCloneManager manager;auto base=a->vfunc_10(manager);auto* clone=dynamic_cast<spGUIManager*>(base.get());Check(clone&&clone->GetFocusForAnalysis()==nullptr,"Clone fresh focus0");clonePair=manager.FindClone(*a)==clone?1:0;b.reset(static_cast<spGUIManager*>(base.release()));}
        else if(mode=="two-fresh-new-first")b=std::make_unique<spGUIManager>();else Check(mode=="delete-focused-borrow","Known empty lifecycle mode");
        second=b.get();const auto before=States(graphs);const auto references=References(graphs);const auto refCalls=host.referenceOperations;
        auto singleton=[&](){auto* value=spGUIManager::GetInstanceForAnalysis();return !value?0u:value==first?1u:value==second?2u:99u;};
        const auto events=in.U32();Check(events<=4,"Lifecycle event count");unsigned destroyed=0;
        for(unsigned i=0;i<events;++i)
        {
            const auto stage=in.Blob();
            if(stage=="before-delete")
            {Match(singleton(),in.U32(),"Singleton before delete");Match(a->GetFocusForAnalysis()!=nullptr,in.U32(),"Source focus presence");Match(b&&b->GetFocusForAnalysis()!=nullptr,in.U32(),"Target focus presence");}
            else
            {if(stage=="delete-1")a.reset();else {Check(stage=="delete-2","Known destructor event");b.reset();}++destroyed;Match(singleton(),in.U32(),"Unconditional singleton clear");}
        }
        const auto hasCopy=in.U32(),expectedCopy=in.U32();Match(mode=="copy-keep-target",hasCopy,"Native Copy return presence");if(hasCopy)Match(copyReturn,expectedCopy,"Literal Copy return");
        Match(clonePair,in.U32(),"Clone pair registered");Match(States(graphs)==before,in.U32(),"Lifecycle retains actual Editor state");
        Match(References(graphs)==references&&host.referenceOperations==refCalls,in.U32(),"Lifecycle retains borrowed ownership");Match(destroyed,in.U32(),"Actual empty deleting destructor count");emptyDtors+=destroyed;
        Check(!a&&!b&&host.unresolved==0,"Complete empty manager lifetime");
    }
    void EscapeProfile(Reader& in,const std::string& fontBytes)
    {
        const auto old=in.U32(),receiver=in.U32(),payload=in.U32();
        std::array<unsigned,2> enabled{};for(auto& value:enabled)value=in.U32();
        const auto visible=in.U32(),controlFocus=in.U32();
        Check(old<=2&&receiver>=1&&receiver<=2&&(payload==0x1b||payload==0x54)&&controlFocus<=2,"Bounded key14 input");
        Host host;spFontManager fontManager;auto font=ReadFont(fontBytes,fontManager);spGUIManager manager;host.manager=&manager;
        Graphs graphs;for(unsigned i=1;i<=2;++i){graphs[i]=std::make_unique<Graph>(host,fontManager,font,enabled[i-1],old==i?1:0);host.targets[i]=graphs[i]->editor.get();}
        ConfigureState(*graphs[receiver],visible,graphs[receiver]->editor->GetHighlightByteForAnalysis());
        manager.SetFocusStateForFixture(old?graphs[old]->editor.get():nullptr);MatchEditors(in,graphs);
        const auto references=References(graphs);const auto refCalls=host.referenceOperations;
        host.controlCode=controlFocus?0x17:0;host.controlFocus=controlFocus;
        spGUINotificationForAnalysis notification{};notification.code=0x14;notification.word04=0x11111111;notification.word08=0x22222222;
        notification.word0C=0x33333333;notification.sender=&manager;notification.word14=0x55555555;notification.payload=payload;notification.tail=0x77777777;
        host.running=true;graphs[receiver]->editor->NotifyEditBoxForAnalysis(notification);host.running=false;
        Check(host.unresolved==0,"Whole Editor key14 has no suppressed dependency refusal");MatchEditors(in,graphs);
        Match(Role(manager.GetFocusForAnalysis(),graphs),in.U32(),"Focus after whole Editor key14");Match(host.controlFired,in.U32(),"Key14 controlled old callback fired");
        Match(notification.code==0x14&&notification.word04==0x11111111&&notification.word08==0x22222222&&notification.word0C==0x33333333&&
            notification.sender==&manager&&notification.word14==0x55555555&&notification.payload==payload&&notification.tail==0x77777777,
            in.U32(),"Original key14 logical frame unchanged");
        Match(References(graphs)==references&&host.referenceOperations==refCalls,in.U32(),"Key14 borrowed ownership retained");
    }
    void FocusEntryProfile(Reader& in,const std::string& fontBytes)
    {
        const auto old=in.U32(),receiver=in.U32(),owner=in.U32();std::array<unsigned,2> enabled{};for(auto& value:enabled)value=in.U32();
        const auto visible=in.U32(),controlCode=in.U32(),controlOwner=in.U32(),controlDisable=in.U32(),mutateMessage=in.U32(),clearSingleton=in.U32();
        Check(old<=2&&receiver>=1&&receiver<=2&&owner<=2&&controlOwner<=2,"Focus-entry input roles");
        Host host;host.entryMode=true;spFontManager fontManager;auto font=ReadFont(fontBytes,fontManager);spGUIManager manager;host.manager=&manager;
        Graphs graphs;for(unsigned i=1;i<=2;++i){graphs[i]=std::make_unique<Graph>(host,fontManager,font,enabled[i-1],old==i?1:0);host.targets[i]=graphs[i]->editor.get();}
        ConfigureState(*graphs[receiver],visible,graphs[receiver]->editor->GetHighlightByteForAnalysis());
        std::array<std::unique_ptr<spGUIObject>,3> owners;for(unsigned i=1;i<=2;++i)
        {owners[i]=std::make_unique<spGUIObject>(host);host.entryOwners[i]=owners[i].get();Check(owners[i]->vfunc_18().classID==spGUIObject::ClassID,"Typed owner actual source identity");}
        auto* object=graphs[receiver]->editor.get();host.ownerBindings[object]=owner?owners[owner].get():nullptr;
        manager.SetFocusStateForFixture(old?graphs[old]->editor.get():nullptr);
        if(clearSingleton){spGUIManager temporary;}
        auto ownerRole=[&](const spGUIObject* value){for(unsigned i=0;i<owners.size();++i)if(owners[i].get()==value)return i;throw std::logic_error("Known typed owner role");};
        auto ownerState=[&]()
        {std::vector<std::uintptr_t> result;for(unsigned i=1;i<=2;++i){result.push_back(owners[i]->GetEnabledByteForAnalysis());result.push_back(reinterpret_cast<std::uintptr_t>(owners[i]->GetReferenceForAnalysis()));result.push_back(owners[i]->GetSavedCollisionGroupsForAnalysis().size());result.push_back(reinterpret_cast<std::uintptr_t>(owners[i]->GetName()));}return result;};
        const auto references=References(graphs);const auto refCalls=host.referenceOperations;const auto beforeOwners=ownerState();MatchEditors(in,graphs);
        spGUIObject* output=nullptr;spGUINotificationForAnalysis notification{};
        notification.code=0x0f;notification.word04=0x11111111;notification.word08=0x22222222;notification.word0C=0x33333333;
        notification.sender=object;notification.word14=0x44444444;notification.payload=0x12345678;notification.tail=0x76543210;
        host.entryReceiver=object;host.entryFrame=&notification;host.entryOutput=&output;host.entryOwner=controlOwner;
        host.entryDisable=controlDisable!=0;host.entryMutateMessage=mutateMessage!=0;host.controlCode=controlCode;
        host.running=true;object->NotifyEditBoxForAnalysis(notification);host.running=false;Check(host.unresolved==0,"Whole focus-entry typed dependency path");
        MatchEditors(in,graphs);Match(Role(manager.GetFocusForAnalysis(),graphs),in.U32(),"Focus-entry final focus");
        Match(spGUIManager::GetInstanceForAnalysis()!=nullptr,in.U32(),"Focus-entry singleton input retained");
        Match(ownerRole(host.ownerBindings[object]),in.U32(),"Focus-entry fresh owner binding");
        Check(host.ownerCalls.size()<=1,"Focus-entry owner dispatch count");Match(host.ownerCalls.empty()?0:ownerRole(host.ownerCalls[0]),in.U32(),"Focus-entry actual owner notified");
        Match(ownerRole(output),in.U32(),"Real GUIObject Notify18 output");Match(host.controlFired,in.U32(),"Focus-entry declared Node input mutation fired");
        Match(notification.code,in.U32(),"Focus-entry forwarded current message code");const auto payloadChanged=in.U32();
        Match(notification.payload==reinterpret_cast<std::uintptr_t>(&output),payloadChanged,"Focus-entry declared payload mutation");
        const bool frame=notification.code==(mutateMessage?0x18u:0x0fu)&&notification.word04==0x11111111&&notification.word08==0x22222222&&notification.word0C==0x33333333&&
            notification.sender==object&&notification.word14==0x44444444&&notification.payload==(mutateMessage?reinterpret_cast<std::uintptr_t>(&output):std::uintptr_t(0x12345678))&&notification.tail==0x76543210;
        const bool borrowedFrame=host.ownerFrames.empty()||host.ownerFrames[0]==&notification;
        Match(frame&&borrowedFrame,in.U32(),"Focus-entry original mutable frame preserved apart from declared input writes");
        Match(ownerState()==beforeOwners,in.U32(),"Typed owner represented state retained");
        Match(References(graphs)==references&&host.referenceOperations==refCalls,in.U32(),"Focus-entry borrowed ownership retained");
    }
    void Basic()
    {
        (void)spGUIManager::StaticRTTI();auto created=spRTTIManager::Instance().Create(spGUIManager::ClassID);
        auto* typed=dynamic_cast<spGUIManager*>(created.get());Check(typed&&typed->IsKindOf(spBaseObject::ClassID)&&!typed->GetFocusForAnalysis(),"GUIManager portable profile RTTI/factory");created.reset();
        struct Protocol final
        {spGUIManager* manager=nullptr;spBaseObject* old=nullptr;spBaseObject* desired=nullptr;const void* frame=nullptr;bool valid=true;unsigned count=0;};
        struct Spy final : spBaseObject
        {
            Protocol& p;explicit Spy(Protocol& value):p(value){}
            void vfunc_0C(const void* raw) noexcept override
            {
                auto& n=*static_cast<const spNotificationForAnalysis*>(raw);++p.count;
                p.valid=p.valid&&n.sender==p.manager&&n.word04==0&&n.word08==0&&n.word0C==0&&n.word14==0&&n.payload==0&&n.tail==0;
                if(n.code==0x17)
                {p.valid=p.valid&&p.manager->GetFocusForAnalysis()==p.old;p.frame=raw;auto& mutableFrame=const_cast<spNotificationForAnalysis&>(n);mutableFrame.word04=0xDEADBEEF;mutableFrame.tail=0xA5;p.manager->SetFocusStateForFixture(nullptr);}
                else p.valid=p.valid&&n.code==0x16&&raw==p.frame&&p.manager->GetFocusForAnalysis()==p.desired;
            }
        };
        spGUIManager manager;Protocol protocol;Spy old(protocol),next(protocol);protocol.manager=&manager;protocol.old=&old;protocol.desired=&next;manager.SetFocusStateForFixture(&old);
        Check(manager.SetFocusForAnalysis(&next)==spGUIFocusResultForAnalysis::Completed&&protocol.valid&&protocol.count==2,"Source-only protocol witness: real virtual calls, same reset frame and publication order");
        struct Recursive final : spBaseObject
        {spGUIManager& manager;unsigned calls=0;explicit Recursive(spGUIManager& value):manager(value){}void vfunc_0C(const void*) noexcept override {++calls;(void)manager.SetFocusForAnalysis(nullptr);}} recursive(manager);
        manager.SetFocusStateForFixture(&recursive);Check(manager.SetFocusForAnalysis(nullptr)==spGUIFocusResultForAnalysis::ReentryLimit&&recursive.calls==spGUIManager::MaximumFocusDepthForAnalysis&&manager.GetFocusForAnalysis()==&recursive,"Analytical nested limit propagates without fake completion");
        manager.SetFocusStateForFixture(nullptr);Check(manager.SetFocusForAnalysis(nullptr)==spGUIFocusResultForAnalysis::Completed,"Independent root call resets analytical refusal state");
    }
    void Replay(const char* path)
    {
        Reader in(path);Check(in.U32()==0x314d4647,"GFM1 fixture");const auto fontBytes=in.Blob();const auto count=in.U32();Check(count<=64,"GUI replay case bound");
        for(unsigned i=0;i<count;++i)
        {const auto label=in.Blob();try{const auto family=in.U32();if(family<=1)FocusProfile(in,fontBytes,family);else if(family==2)LifetimeProfile(in,fontBytes);else if(family==3)EscapeProfile(in,fontBytes);else {Check(family==4,"GUI fixture family");FocusEntryProfile(in,fontBytes);}++nativeCases;}catch(const std::exception& error){throw std::runtime_error(label+": "+error.what());}}
        Check(in.input.peek()==std::char_traits<char>::eof(),"Complete GUI fixture read");
    }
}
int main(int argc,char** argv)
{
    try{Basic();for(int i=1;i<argc;++i)Replay(argv[i]);std::cout<<"PASS GUIManager bounded focus/singleton/empty lifecycle nativeCases="<<nativeCases<<" scalarComparisons="<<comparisons<<" focusCalls="<<focusCalls<<" emptyManagerDtors="<<emptyDtors<<" checks="<<checks<<'\n';return 0;}
    catch(const std::exception& error){std::cerr<<"FAIL GUIManager "<<error.what()<<'\n';return 1;}
}

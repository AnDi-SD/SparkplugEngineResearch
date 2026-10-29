#include "Analysis/Host/spProjectionIndexHost.h"
#include "Code/Sparkplug/spProjectionSerializer.h"
#include "Code/Sparkplug/spBoxProjectionSerializer.h"
#include "Code/Sparkplug/spPyramidProjectionSerializer.h"
#include "Code/Sparkplug/spBoxProjection.h"
#include "Code/Sparkplug/spPyramidProjection.h"
#include "Code/Sparkplug/spRenderNode.h"
#include "Code/Sparkplug/spFog.h"
#include "Code/Sparkplug/spMaterialSerializer.h"
#include "Code/Sparkplug/spFogSerializer.h"
#include "Code/Sparkplug/spRenderNodeSerializer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include <array>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    using Lifetime=spPCErrorManager::WarmEmptyContextForAnalysis;
    unsigned checks=0;
    void Check(bool value,const char* why)
    {++checks;if(!value)throw std::runtime_error(why);}
    void SaveContext(spSerializerManager& manager)
    {manager.SetDispatchContextForAnalysis(spSerializerManager::PlatformCommon,spSerializerManager::OperationSave);}
    std::vector<std::uint32_t> Target(const spProjection& object)
    {
        const auto& state=object.GetProjectionStateForAnalysis();
        std::vector<std::uint32_t> words;
        for(auto value:state.flags58)words.push_back(value);
        words.insert(words.end(),state.words5C.begin(),state.words5C.end());
        words.push_back(state.word6C);
        std::array<std::uint32_t,4> sphere{};std::memcpy(sphere.data(),state.sphere70.data(),16);
        words.insert(words.end(),sphere.begin(),sphere.end());
        words.push_back(state.word98);words.push_back(object.IsAlphaSortEnabledForAnalysis());
        words.push_back(object.GetPriorityForAnalysis());words.push_back(object.GetProjectionRuntimeModeForAnalysis());
        words.push_back(object.GetOpaqueRecordCountForAnalysis());
        words.push_back(object.GetBoundRenderNodeForAnalysis()!=nullptr);words.push_back(object.GetPCFxForAnalysis()!=nullptr);
        std::array<std::uint32_t,6> geometry{};
        if(const auto* box=dynamic_cast<const spBoxProjection*>(&object))
        {std::memcpy(geometry.data(),box->GetCenterForAnalysis().data(),12);std::memcpy(geometry.data()+3,box->GetSizeForAnalysis().data(),12);}
        else if(const auto* pyramid=dynamic_cast<const spPyramidProjection*>(&object))
        {
            std::memcpy(geometry.data(),pyramid->GetCenterForAnalysis().data(),12);std::memcpy(geometry.data()+3,pyramid->GetSizeForAnalysis().data(),12);
            const auto& own=pyramid->GetPyramidStateForAnalysis();words.push_back(own.flip9C);words.push_back(own.wordB8);words.push_back(own.byteCC);
        }
        words.insert(words.end(),geometry.begin(),geometry.end());return words;
    }
    std::array<std::size_t,3> Fat(const spSerializerManager& manager)
    {
        const auto* fat=manager.GetFATForAnalysis();Check(fat!=nullptr,"Supplied FAT exists");
        return {fat->GetResourceCountForAnalysis(),fat->GetFileCountForAnalysis(),fat->GetNextResourceIDForAnalysis()};
    }
    std::array<std::size_t,6> Error(const spPCErrorManager& manager)
    {
        return {manager.GetDataUsedForAnalysis(),manager.GetDataCapacityForAnalysis(),
            manager.GetHeadForAnalysis()!=nullptr,manager.HadFatalForAnalysis(),manager.HadDataOverflowForAnalysis(),
            spErrorManager::GetInstance()==&manager};
    }
    void Replay()
    {
        unsigned kind=0,repeats=0;std::uint32_t initial=0;
        while(std::cin>>kind>>initial>>repeats)
        {
            Check(kind<=2&&repeats==2,"Bounded native input sequence");
            spSerializerManager manager;SaveContext(manager);spPCErrorManager errors;auto raw=initial;
            spProjectionNullIndexContextForAnalysis context{manager,errors,raw,Lifetime::StableBorrowed};
            std::unique_ptr<spProjectionSerializer> serializer;
            if(kind==0)serializer=std::make_unique<spProjectionSerializer>();
            else if(kind==1)serializer=std::make_unique<spBoxProjectionSerializer>();
            else serializer=std::make_unique<spPyramidProjectionSerializer>();
            std::unique_ptr<spProjection> object;
            if(kind==2)object=std::make_unique<spPyramidProjection>(spProjectionConstructorDataForAnalysis{0});
            else object=std::make_unique<spBoxProjection>(spProjectionConstructorDataForAnalysis{0});
            for(unsigned repeat=0;repeat<repeats;++repeat)
            {
                const auto target=Target(*object);const auto fat=Fat(manager);
                const auto error=Error(errors);const auto before=raw;
                const bool result=serializer->IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,*object);
                std::cout<<result<<' '<<before<<' '<<raw<<' '<<errors.GetDataUsedForAnalysis()<<' '
                    <<(errors.GetHeadForAnalysis()!=nullptr)<<' '<<(target==Target(*object))<<' '
                    <<(fat==Fat(manager))<<' '<<(error==Error(errors))<<'\n';
            }
        }
        Check(std::cin.eof(),"Complete replay input");
    }
    void Contracts()
    {
        spSerializerManager manager;SaveContext(manager);spPCErrorManager errors;std::uint32_t raw=123;
        spProjectionNullIndexContextForAnalysis context{manager,errors,raw};
        spProjectionSerializer projection;spBoxProjectionSerializer box;spPyramidProjectionSerializer pyramid;
        spBoxProjection object(spProjectionConstructorDataForAnalysis{0});
        spPyramidProjection other(spProjectionConstructorDataForAnalysis{0});
        Check(!projection.IndexRelationshipsForAnalysis(object)&&!projection.IndexRelationshipsWithContextForAnalysis(manager,object),"Existing no-context entry points refuse");
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123,"Default lifetime refuses before raw write");
        context.lifetime=Lifetime::StableBorrowed;
        Check(projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==0,"Explicit warm null Projection profile");
        raw=123;Check(box.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==0,"Box delegates shared Projection index");
        raw=123;Check(pyramid.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,other)&&raw==0,"Pyramid shares the inherited native index body");
        raw=123;Check(!box.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,other)&&!pyramid.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123,"Concrete serializer target mismatch refuses before effects");
        spRenderNode node;object.SetBoundRenderNodeForAnalysis(&node);
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123&&object.GetBoundRenderNodeForAnalysis()==&node,"Nonnull borrowed Node recursion refuses before effects");
        object.SetBoundRenderNodeForAnalysis(nullptr);
        auto material=std::make_shared<spDXMaterial>();object.SetMaterialForAnalysis(material);const auto materialRefs=material.use_count();
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123&&material.use_count()==materialRefs,"Nonnull owning Material refuses without ownership changes");
        object.SetMaterialForAnalysis(nullptr);
        auto fog=std::make_shared<spFog>();object.SetFogForAnalysis(fog);const auto fogRefs=fog.use_count();
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123&&fog.use_count()==fogRefs,"Nonnull owning Fog refuses without ownership changes");
        object.SetFogForAnalysis(nullptr);
        manager.SetDispatchContextForAnalysis(spSerializerManager::PlatformCommon,spSerializerManager::OperationLoad);
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123,"Unqualified load context refuses");
        SaveContext(manager);Check(manager.SetSerializationPolicyForAnalysis(0),"Alternate serialization policy fixture");
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123,"Unqualified policy refuses");
        Check(manager.SetSerializationPolicyForAnalysis(2),"Restore admitted policy");
        Check(manager.GetFATForAnalysis()->SetNextResourceIDForAnalysis(7),"Noninitial FAT fixture");
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123,"Noninitial FAT refuses before effects");
        manager.GetFATForAnalysis()->ClearResourceEntriesForAnalysis();
        Check(manager.GetFATForAnalysis()->SetNextResourceIDForAnalysis(1),"Restore empty FAT");
        {
            spPCErrorManager current;
            Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123,"Supplied noncurrent error manager refuses without reacquisition");
        }
        Check(spErrorManager::GetInstance()==nullptr,"Existing error singleton destructor semantics retained");
        Check(!projection.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123,"Absent current manager is not created");
    }
    void Queued()
    {
        spSerializerManager manager;SaveContext(manager);spPCErrorManager errors;std::uint32_t raw=123;
        spProjectionNullIndexContextForAnalysis context{manager,errors,raw,Lifetime::StableBorrowed};
        spBoxProjection object(spProjectionConstructorDataForAnalysis{0});spProjectionSerializer serializer;
        const auto* error=errors.CreateErrorForAnalysis(spErrorSeverity::Error,1,nullptr,0);Check(error!=nullptr,"Queued error fixture");
        const auto used=errors.GetDataUsedForAnalysis();
        Check(!serializer.IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,object)&&raw==123
            &&errors.GetHeadForAnalysis()==error&&errors.GetDataUsedForAnalysis()==used,"Queued handler refuses without dispatch, clearing or raw effects");
    }
    struct PreindexedFixture
    {
        // Reverse destruction releases Projection before its borrowed Node.
        spSerializerManager manager;spPCErrorManager errors;std::uint32_t raw;
        std::shared_ptr<spMaterialSerializer> materialSerializer=std::make_shared<spMaterialSerializer>();
        std::shared_ptr<spFogSerializer> fogSerializer=std::make_shared<spFogSerializer>();
        std::shared_ptr<spRenderNodeSerializer> nodeSerializer=std::make_shared<spRenderNodeSerializer>();
        std::shared_ptr<spDXMaterial> material=std::make_shared<spDXMaterial>();
        std::shared_ptr<spFog> fog=std::make_shared<spFog>();spRenderNode node;
        std::unique_ptr<spProjection> object;std::unique_ptr<spProjectionSerializer> serializer;
        PreindexedFixture(unsigned kind,unsigned mask,std::uint32_t initial):raw(initial)
        {
            SaveContext(manager);
            Check(manager.RegisterForAnalysis(spDXMaterial::ClassID,materialSerializer,spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Register exact DXMaterial dispatch key");
            Check(manager.RegisterForAnalysis(spFog::ClassID,fogSerializer,spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Register Fog serializer");
            Check(manager.RegisterForAnalysis(spRenderNode::ClassID,nodeSerializer,spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Register RenderNode serializer");
            if(kind==0)serializer=std::make_unique<spProjectionSerializer>();
            else if(kind==1)serializer=std::make_unique<spBoxProjectionSerializer>();
            else serializer=std::make_unique<spPyramidProjectionSerializer>();
            if(kind==2)object=std::make_unique<spPyramidProjection>(spProjectionConstructorDataForAnalysis{0});
            else object=std::make_unique<spBoxProjection>(spProjectionConstructorDataForAnalysis{0});
            if(mask&1)object->SetMaterialForAnalysis(material);
            if(mask&2)object->SetFogForAnalysis(fog);
            if(mask&4)object->SetBoundRenderNodeForAnalysis(&node);
            spBaseObject* values[]={material.get(),fog.get(),&node};
            const spClassID classes[]={spDXMaterial::ClassID,spFog::ClassID,spRenderNode::ClassID};
            const std::uint32_t ids[]={7,9,11};auto* fat=manager.GetFATForAnalysis();
            for(unsigned i=0;i<3;++i)if(mask&(1u<<i))
            {
                Check(fat->SetNextResourceIDForAnalysis(ids[i]),"Prepare supplied relationship ID");
                Check(fat->IndexObjectForAnalysis(classes[i],*values[i]),"Preindex existing typed relationship");
            }
        }
        spProjectionPreindexedIndexContextForAnalysis Context()
        {return {manager,errors,raw,*materialSerializer,*fogSerializer,*nodeSerializer,Lifetime::StableBorrowed};}
        bool Index(){auto context=Context();return serializer->IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(context,*object);}
    };
    template<std::size_t N>void AppendFloatWords(std::vector<std::uint32_t>& out,const std::array<float,N>& values)
    {
        std::array<std::uint32_t,N> words{};std::memcpy(words.data(),values.data(),sizeof(words));
        out.insert(out.end(),words.begin(),words.end());
    }
    std::vector<std::uint32_t> RelationState(const PreindexedFixture& f)
    {
        // Represented public state and shared ownership, not a native ABI dump
        // or an assertion that use_count equals native intrusive refcount4.
        std::vector<std::uint32_t> out{
            f.object->GetMaterialForAnalysis().get()==f.material.get(),
            f.object->GetFogForAnalysis().get()==f.fog.get(),f.object->GetBoundRenderNodeForAnalysis()==&f.node,
            static_cast<std::uint32_t>(f.material.use_count()),static_cast<std::uint32_t>(f.fog.use_count()),
            f.material->GetVertexAlphaByteForAnalysis(),f.material->GetRenderOverrideByteForAnalysis(),
            f.material->GetOpaqueRuntimeFieldForAnalysis(),static_cast<std::uint32_t>(f.material->GetPassCountForAnalysis()),
            f.material->GetMaterialColorControllerForAnalysis()!=nullptr,f.material->HasInitializedSpecularPowerForAnalysis(),
            static_cast<std::uint32_t>(f.fog->GetTypeForAnalysis()),f.fog->GetColorARGBForAnalysis(),
            f.node.GetFlagsForAnalysis(),static_cast<std::uint32_t>(f.node.GetChildCountForAnalysis()),
            static_cast<std::uint32_t>(f.node.GetRenderableCountForAnalysis()),f.node.GetParentForAnalysis()!=nullptr};
        const auto& states=f.material->GetRenderStatesForAnalysis();out.insert(out.end(),states.begin(),states.end());
        AppendFloatWords(out,f.material->GetAmbientColorForAnalysis());AppendFloatWords(out,f.material->GetDiffuseColorForAnalysis());
        AppendFloatWords(out,f.material->GetSpecularColorForAnalysis());AppendFloatWords(out,f.material->GetEmissiveColorForAnalysis());
        AppendFloatWords(out,std::array<float,4>{f.material->GetSpecularPowerForAnalysis(),f.fog->GetStartForAnalysis(),f.fog->GetEndForAnalysis(),f.fog->GetDensityForAnalysis()});
        AppendFloatWords(out,f.node.GetPositionForAnalysis());AppendFloatWords(out,f.node.GetScaleForAnalysis());
        AppendFloatWords(out,f.node.GetOrientationForAnalysis());AppendFloatWords(out,f.node.GetWorldPositionForAnalysis());
        AppendFloatWords(out,f.node.GetWorldScaleForAnalysis());AppendFloatWords(out,f.node.GetWorldOrientationForAnalysis());return out;
    }
    std::vector<std::uint32_t> EntryState(PreindexedFixture& f)
    {
        std::vector<std::uint32_t> out;auto* fat=f.manager.GetFATForAnalysis();
        spBaseObject* values[]={f.material.get(),f.fog.get(),&f.node};
        for(auto* value:values)
        {
            const auto* entry=fat->FindByObjectForAnalysis(*value);out.push_back(entry!=nullptr);if(!entry)continue;
            const std::uint32_t words[]={entry->id,entry->fileID,entry->classID,entry->offset,entry->size,
                entry->payloadWritten,entry->nameIsNullForAnalysis,entry->name.empty(),entry->object==value,
                fat->FindByIDForAnalysis(entry->id)==entry};out.insert(out.end(),std::begin(words),std::end(words));
        }
        return out;
    }
    void PreindexedStep(PreindexedFixture& f,bool emit)
    {
        const auto target=Target(*f.object);const auto fat=Fat(f.manager);const auto error=Error(f.errors);
        const auto relations=RelationState(f);const auto entries=EntryState(f);const auto before=f.raw;
        const bool result=f.Index();
        const std::array<std::uint32_t,10> row{result,before,f.raw,static_cast<std::uint32_t>(f.errors.GetDataUsedForAnalysis()),
            f.errors.GetHeadForAnalysis()!=nullptr,target==Target(*f.object),fat==Fat(f.manager),error==Error(f.errors),
            relations==RelationState(f),entries==EntryState(f)};
        Check(row==std::array<std::uint32_t,10>{1,before,0,0,0,1,1,1,1,1},"Whole preindexed Index preserves represented state and ownership");
        if(emit){for(std::size_t i=0;i<row.size();++i)std::cout<<(i?" ":"")<<row[i];std::cout<<'\n';}
    }
    void ReplayPreindexed()
    {
        unsigned kind=0,mask=0,repeats=0;std::uint32_t raw=0;
        while(std::cin>>kind>>mask>>raw>>repeats)
        {
            Check(kind<=2&&mask>=1&&mask<=7&&repeats==2,"Bounded preindexed native input sequence");
            PreindexedFixture f(kind,mask,raw);for(unsigned i=0;i<repeats;++i)PreindexedStep(f,true);
        }
        Check(std::cin.eof(),"Complete preindexed replay input");
    }
    void RefusalUnchanged(PreindexedFixture& f,const char* label)
    {
        const auto target=Target(*f.object);const auto fat=Fat(f.manager);const auto errors=Error(f.errors);
        const auto relations=RelationState(f);const auto entries=EntryState(f);const auto raw=f.raw;
        Check(!f.Index()&&f.raw==raw&&target==Target(*f.object)&&fat==Fat(f.manager)&&errors==Error(f.errors)
            &&relations==RelationState(f)&&entries==EntryState(f),label);
    }
    struct ForeignNode final:spRenderNode
    {
        mutable unsigned calls=0;
        const spRTTIRecord& vfunc_18() const noexcept override{++calls;return spRenderNode::StaticRTTI();}
    };
    struct ForeignMaterialSerializer final:spMaterialSerializer
    {
        mutable unsigned calls=0;
        const spRTTIRecord& vfunc_18() const noexcept override{++calls;return spMaterialSerializer::StaticRTTI();}
    };
    struct ForeignProjectionSerializer final:spProjectionSerializer
    {
        mutable unsigned calls=0;
        spClassID GetTargetClassIDForAnalysis() const noexcept override{++calls;return spProjectionSerializer::TargetClassID;}
    };
    void PreindexedContracts()
    {
        for(unsigned kind=0;kind<3;++kind)for(unsigned mask=1;mask<8;++mask)
        {PreindexedFixture f(kind,mask,0xffffffff);PreindexedStep(f,false);PreindexedStep(f,false);}
        {PreindexedFixture f(0,0,123);RefusalUnchanged(f,"New profile excludes empty mask");}
        {PreindexedFixture f(0,7,123);auto context=f.Context();context.lifetime=Lifetime::UnqualifiedMutable;
            Check(!f.serializer->IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123,"Default borrowed lifetime refuses");}
        {PreindexedFixture f(0,7,123);f.manager.SetDispatchContextForAnalysis(spSerializerManager::PlatformCommon,spSerializerManager::OperationLoad);
            RefusalUnchanged(f,"Load profile refuses before effects");}
        {PreindexedFixture f(0,7,123);Check(f.manager.SetSerializationPolicyForAnalysis(0),"Other policy fixture");RefusalUnchanged(f,"Other policy refuses");}
        {PreindexedFixture f(0,7,123);auto* fat=f.manager.GetFATForAnalysis();fat->ClearResourceEntriesForAnalysis();Check(fat->SetNextResourceIDForAnalysis(1),"Reset missing FAT input");RefusalUnchanged(f,"Unindexed relationships refuse");}
        {PreindexedFixture f(0,7,123);f.manager.GetFATForAnalysis()->FindByIDForAnalysis(9)->payloadWritten=true;RefusalUnchanged(f,"Written entry outside supplied profile refuses");}
        {PreindexedFixture f(0,7,123);f.manager.GetFATForAnalysis()->FindByIDForAnalysis(7)->classID=spMaterial::ClassID;RefusalUnchanged(f,"Wrong actual concrete entry class refuses");}
        {PreindexedFixture f(0,7,123);Check(f.manager.GetFATForAnalysis()->SetNextResourceIDForAnalysis(20),"Other nextID fixture");RefusalUnchanged(f,"Other FAT nextID refuses");}
        {PreindexedFixture f(0,7,123);Check(f.manager.RegisterForAnalysis(1,f.fogSerializer,spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Extra registration fixture");RefusalUnchanged(f,"Additional registration refuses");}
        {PreindexedFixture f(0,7,123);spMaterialSerializer unregistered;
            spProjectionPreindexedIndexContextForAnalysis other{f.manager,f.errors,f.raw,unregistered,*f.fogSerializer,*f.nodeSerializer,Lifetime::StableBorrowed};
            Check(!f.serializer->IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(other,*f.object)&&f.raw==123,"Exact supplied serializer binding required");}
        {PreindexedFixture f(0,7,123);ForeignNode node;f.object->SetBoundRenderNodeForAnalysis(&node);RefusalUnchanged(f,"Derived Node refuses before RTTI callback");
            Check(node.calls==0,"No unqualified Node virtual dispatch");f.object->SetBoundRenderNodeForAnalysis(nullptr);}
        {PreindexedFixture f(0,7,123);ForeignMaterialSerializer foreign;
            spProjectionPreindexedIndexContextForAnalysis context{f.manager,f.errors,f.raw,foreign,*f.fogSerializer,*f.nodeSerializer,Lifetime::StableBorrowed};
            Check(!f.serializer->IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123&&foreign.calls==0,"Derived serializer binding refuses before callbacks");}
        {PreindexedFixture f(0,7,123);ForeignProjectionSerializer foreign;auto context=f.Context();
            Check(!foreign.IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123&&foreign.calls==0,"Foreign own endpoint receiver refuses before target callback");}
        {PreindexedFixture f(2,7,123);spBoxProjectionSerializer box;auto context=f.Context();
            Check(!box.IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123,"Wrong concrete target refuses");}
        {PreindexedFixture f(0,7,123);spProjectionNullIndexContextForAnalysis context{f.manager,f.errors,f.raw,Lifetime::StableBorrowed};
            Check(!f.serializer->IndexNullRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123,"Legacy null endpoint still refuses nonnull inputs");
            Check(!f.serializer->IndexRelationshipsWithContextForAnalysis(f.manager,*f.object)&&!f.serializer->IndexRelationshipsForAnalysis(*f.object)&&f.raw==123,"General entry points retain refusal");}
        {PreindexedFixture f(0,7,123);Check(f.errors.CreateErrorForAnalysis(spErrorSeverity::Error,1,nullptr,0)!=nullptr,"Preindexed queued error fixture");RefusalUnchanged(f,"Queued errors refuse without clearing");}
        {PreindexedFixture f(0,7,123);{spPCErrorManager other;RefusalUnchanged(f,"Noncurrent supplied error manager refuses");}
            RefusalUnchanged(f,"Missing global error manager is not acquired");}
    }
    struct FogOnlyFixture
    {
        spSerializerManager manager;spPCErrorManager errors;std::uint32_t raw;
        std::shared_ptr<spFogSerializer> fogSerializer=std::make_shared<spFogSerializer>();
        std::shared_ptr<spFog> fog=std::make_shared<spFog>();
        spRenderNode node; // outlives target's borrowed test link
        std::unique_ptr<spProjection> object;std::unique_ptr<spProjectionSerializer> serializer;
        FogOnlyFixture(unsigned kind,std::uint32_t initial):raw(initial)
        {
            SaveContext(manager);
            Check(manager.RegisterForAnalysis(spFog::ClassID,fogSerializer,
                spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Fog-only registration input");
            if(kind==0)serializer=std::make_unique<spProjectionSerializer>();
            else if(kind==1)serializer=std::make_unique<spBoxProjectionSerializer>();
            else serializer=std::make_unique<spPyramidProjectionSerializer>();
            if(kind==2)object=std::make_unique<spPyramidProjection>(spProjectionConstructorDataForAnalysis{0});
            else object=std::make_unique<spBoxProjection>(spProjectionConstructorDataForAnalysis{0});
            object->SetFogForAnalysis(fog);
        }
        spProjectionFogOnlyIndexContextForAnalysis Context()
        {return {manager,errors,raw,*fogSerializer,Lifetime::StableBorrowed};}
        bool Index(){auto context=Context();return serializer->IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*object);}
    };
    std::vector<std::uint32_t> FogOnlyState(const FogOnlyFixture& f)
    {
        std::vector<std::uint32_t> out{static_cast<std::uint32_t>(f.fog->GetTypeForAnalysis()),
            f.fog->GetColorARGBForAnalysis(),f.fog->GetName()!=nullptr,
            static_cast<std::uint32_t>(f.fog.use_count()),f.object->GetFogForAnalysis().get()==f.fog.get(),
            f.object->GetMaterialForAnalysis()!=nullptr,f.object->GetBoundRenderNodeForAnalysis()!=nullptr};
        AppendFloatWords(out,std::array<float,3>{f.fog->GetStartForAnalysis(),f.fog->GetEndForAnalysis(),f.fog->GetDensityForAnalysis()});
        return out;
    }
    std::vector<std::uint32_t> FogOnlyFat(FogOnlyFixture& f)
    {
        const auto counts=Fat(f.manager);
        std::vector<std::uint32_t> out{static_cast<std::uint32_t>(counts[0]),static_cast<std::uint32_t>(counts[1]),static_cast<std::uint32_t>(counts[2])};
        auto* fat=f.manager.GetFATForAnalysis();const auto* entry=fat->FindByObjectForAnalysis(*f.fog);
        out.push_back(entry!=nullptr);
        if(entry)
        {
            const std::uint32_t words[]={entry->id,entry->classID,entry->fileID,entry->nameIsNullForAnalysis,
                entry->name.empty(),entry->offset,entry->size,entry->payloadWritten,entry->object==f.fog.get(),
                fat->FindByIDForAnalysis(entry->id)==entry};
            out.insert(out.end(),std::begin(words),std::end(words));
        }
        return out;
    }
    void FogOnlyStep(FogOnlyFixture& f,unsigned repeat,bool emit)
    {
        const auto target=Target(*f.object);const auto fog=FogOnlyState(f);const auto errors=Error(f.errors);
        const auto before=Fat(f.manager);const auto raw=f.raw;
        Check(before==std::array<std::size_t,3>{repeat,0,repeat?2u:1u},"Native empty/one-entry input transition");
        const bool result=f.Index();auto* fat=f.manager.GetFATForAnalysis();
        const auto after=Fat(f.manager);const auto* entry=fat->FindByObjectForAnalysis(*f.fog);
        Check(after[1]==0,"Fog-only Index adds no external file metadata");
        Check(entry!=nullptr,"Fog-only Index publishes a live metadata entry");
        const std::array<std::uint32_t,20> row{result,raw,f.raw,static_cast<std::uint32_t>(before[0]),
            static_cast<std::uint32_t>(before[2]),static_cast<std::uint32_t>(after[0]),static_cast<std::uint32_t>(after[2]),
            entry->id,entry->classID,entry->fileID,entry->nameIsNullForAnalysis&&entry->name.empty(),
            entry->offset,entry->size,entry->payloadWritten,entry->object==f.fog.get(),fat->FindByIDForAnalysis(1)==entry,
            target==Target(*f.object),fog==FogOnlyState(f),errors==Error(f.errors),
            f.manager.GetRegistrationCountForAnalysis()==1&&f.manager.FindForAnalysis(spFog::ClassID)==f.fogSerializer.get()};
        const std::array<std::uint32_t,20> want{1,raw,0,repeat,repeat?2u:1u,1,2,1,spFog::ClassID,0,1,0,0,0,1,1,1,1,1,1};
        Check(row==want,"Whole Fog-only first insertion/repeat matches native represented transition");
        if(emit){for(std::size_t i=0;i<row.size();++i)std::cout<<(i?" ":"")<<row[i];std::cout<<'\n';}
    }
    void FogOnlyReplay()
    {
        unsigned kind=0,repeats=0;std::uint32_t raw=0;
        while(std::cin>>kind>>raw>>repeats)
        {
            Check(kind<3&&repeats==2,"Bounded three-family Fog-only replay input");
            FogOnlyFixture f(kind,raw);for(unsigned i=0;i<repeats;++i)FogOnlyStep(f,i,true);
        }
        Check(std::cin.eof(),"Complete Fog-only replay input");
    }
    void FogOnlyRefusal(FogOnlyFixture& f,const char* label)
    {
        const auto target=Target(*f.object);const auto fog=FogOnlyState(f);const auto fat=FogOnlyFat(f);
        const auto errors=Error(f.errors);const auto raw=f.raw;
        Check(!f.Index()&&f.raw==raw&&target==Target(*f.object)&&fog==FogOnlyState(f)&&fat==FogOnlyFat(f)&&errors==Error(f.errors),label);
    }
    struct ForeignFogSerializer final:spFogSerializer
    {
        mutable unsigned calls=0;
        const spRTTIRecord& vfunc_18() const noexcept override{++calls;return spFogSerializer::StaticRTTI();}
    };
    void FogOnlyContracts()
    {
        for(unsigned kind=0;kind<3;++kind)
        {FogOnlyFixture f(kind,kind==0?0:kind==1?0x1234:0xffffffff);FogOnlyStep(f,0,false);FogOnlyStep(f,1,false);}
        {FogOnlyFixture f(0,123);auto context=f.Context();context.lifetime=Lifetime::UnqualifiedMutable;
            Check(!f.serializer->IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)
                &&f.raw==123&&f.manager.GetFATForAnalysis()->GetResourceCountForAnalysis()==0,"Unqualified lifetime refuses before insertion");}
        {FogOnlyFixture f(0,123);f.object->SetFogForAnalysis(nullptr);FogOnlyRefusal(f,"Required Fog cannot be null");}
        {FogOnlyFixture f(0,123);f.object->SetFogForAnalysis(std::make_shared<spBaseObject>());FogOnlyRefusal(f,"Wrong physical Fog refuses");}
        {FogOnlyFixture f(0,123);f.object->SetMaterialForAnalysis(std::make_shared<spDXMaterial>());FogOnlyRefusal(f,"Material indexing stays outside Fog-only profile");}
        {FogOnlyFixture f(0,123);f.object->SetBoundRenderNodeForAnalysis(&f.node);FogOnlyRefusal(f,"RenderNode indexing stays outside Fog-only profile");}
        {FogOnlyFixture f(0,123);f.fog->SetName("outside-captured-null-name");FogOnlyRefusal(f,"Nonnull physical name refuses");}
        {FogOnlyFixture f(0,123);Check(f.manager.GetFATForAnalysis()->SetNextResourceIDForAnalysis(7),"Wrong empty nextID input");FogOnlyRefusal(f,"Noninitial empty FAT refuses");}
        {FogOnlyFixture f(0,123);auto* fat=f.manager.GetFATForAnalysis();Check(fat->IndexObjectForAnalysis(spRenderNode::ClassID,f.node),"Other resource entry input");
            FogOnlyRefusal(f,"Foreign one-entry FAT refuses");}
        {FogOnlyFixture f(0,123);FogOnlyStep(f,0,false);f.raw=123;f.manager.GetFATForAnalysis()->FindByIDForAnalysis(1)->payloadWritten=true;
            FogOnlyRefusal(f,"Written entry stays outside profile");}
        {FogOnlyFixture f(0,123);FogOnlyStep(f,0,false);f.raw=123;f.manager.GetFATForAnalysis()->FindByIDForAnalysis(1)->classID=spBaseObject::ClassID;
            FogOnlyRefusal(f,"Incorrect entry class refuses");}
        {FogOnlyFixture f(0,123);Check(f.manager.RegisterForAnalysis(1,f.fogSerializer,spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Extra binding input");
            FogOnlyRefusal(f,"Exactly one registration required");}
        {FogOnlyFixture f(0,123);spFogSerializer other;spProjectionFogOnlyIndexContextForAnalysis context{f.manager,f.errors,f.raw,other,Lifetime::StableBorrowed};
            Check(!f.serializer->IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123
                &&f.manager.GetFATForAnalysis()->GetResourceCountForAnalysis()==0,"Unregistered exact binding refuses");}
        {FogOnlyFixture f(0,123);ForeignFogSerializer other;spProjectionFogOnlyIndexContextForAnalysis context{f.manager,f.errors,f.raw,other,Lifetime::StableBorrowed};
            Check(!f.serializer->IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123&&other.calls==0,"Foreign Fog serializer refuses before callbacks");}
        {FogOnlyFixture f(0,123);ForeignProjectionSerializer other;auto context=f.Context();
            Check(!other.IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123&&other.calls==0,"No foreign GetTarget callback");}
        {FogOnlyFixture f(2,123);spBoxProjectionSerializer box;auto context=f.Context();
            Check(!box.IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123,"Wrong exact target family refuses");}
        {FogOnlyFixture f(0,123);f.manager.SetDispatchContextForAnalysis(spSerializerManager::PlatformCommon,spSerializerManager::OperationLoad);
            FogOnlyRefusal(f,"Load dispatch refuses");}
        {FogOnlyFixture f(0,123);Check(f.manager.SetSerializationPolicyForAnalysis(0),"Wrong policy input");FogOnlyRefusal(f,"Other policy refuses");}
        {FogOnlyFixture f(0,123);Check(f.errors.CreateErrorForAnalysis(spErrorSeverity::Error,1,nullptr,0)!=nullptr,"Queued Fog-only error input");
            FogOnlyRefusal(f,"Queued errors refuse before allocation/clearing");}
        {FogOnlyFixture f(0,123);spPCErrorManager other;FogOnlyRefusal(f,"Noncurrent supplied ErrorManager refuses");}
        {FogOnlyFixture f(0,123);spSerializerManager other;FogOnlyRefusal(f,"Noncurrent supplied SerializerManager refuses");}
    }
    struct EmptyMaterialFixture
    {
        spSerializerManager manager;spPCErrorManager errors;std::uint32_t raw;
        std::shared_ptr<spMaterialSerializer> materialSerializer=std::make_shared<spMaterialSerializer>();
        std::shared_ptr<spDXMaterial> material=std::make_shared<spDXMaterial>();
        spRenderNode node; // outlives target's borrowed test link
        std::unique_ptr<spProjection> object;std::unique_ptr<spProjectionSerializer> serializer;
        EmptyMaterialFixture(unsigned kind,std::uint32_t initial):raw(initial)
        {
            SaveContext(manager);
            Check(manager.RegisterForAnalysis(spDXMaterial::ClassID,materialSerializer,
                spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Empty-Material registration input");
            if(kind==0)serializer=std::make_unique<spProjectionSerializer>();
            else if(kind==1)serializer=std::make_unique<spBoxProjectionSerializer>();
            else serializer=std::make_unique<spPyramidProjectionSerializer>();
            if(kind==2)object=std::make_unique<spPyramidProjection>(spProjectionConstructorDataForAnalysis{0});
            else object=std::make_unique<spBoxProjection>(spProjectionConstructorDataForAnalysis{0});
            object->SetMaterialForAnalysis(material);
        }
        spProjectionEmptyMaterialIndexContextForAnalysis Context()
        {return {manager,errors,raw,*materialSerializer,Lifetime::StableBorrowed};}
        bool Index(){auto context=Context();return serializer->IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*object);}
    };
    std::vector<std::uint32_t> EmptyMaterialState(const EmptyMaterialFixture& f)
    {
        // Do not read or invent the native unwritten power word. These are
        // represented host fields and ownership, not native ABI/ref4 equality.
        std::vector<std::uint32_t> out{f.material->GetName()!=nullptr,
            static_cast<std::uint32_t>(f.material.use_count()),f.object->GetMaterialForAnalysis().get()==f.material.get(),
            f.object->GetFogForAnalysis()!=nullptr,f.object->GetBoundRenderNodeForAnalysis()!=nullptr,
            static_cast<std::uint32_t>(f.material->GetPassCountForAnalysis()),f.material->GetMaterialColorControllerForAnalysis()!=nullptr,
            f.material->GetVertexAlphaByteForAnalysis(),f.material->GetRenderOverrideByteForAnalysis(),f.material->GetOpaqueRuntimeFieldForAnalysis()};
        const auto& states=f.material->GetRenderStatesForAnalysis();out.insert(out.end(),states.begin(),states.end());
        AppendFloatWords(out,f.material->GetAmbientColorForAnalysis());AppendFloatWords(out,f.material->GetDiffuseColorForAnalysis());
        AppendFloatWords(out,f.material->GetSpecularColorForAnalysis());AppendFloatWords(out,f.material->GetEmissiveColorForAnalysis());return out;
    }
    std::vector<std::uint32_t> EmptyMaterialFat(EmptyMaterialFixture& f)
    {
        const auto counts=Fat(f.manager);
        std::vector<std::uint32_t> out{static_cast<std::uint32_t>(counts[0]),static_cast<std::uint32_t>(counts[1]),static_cast<std::uint32_t>(counts[2])};
        auto* fat=f.manager.GetFATForAnalysis();const auto* entry=fat->FindByObjectForAnalysis(*f.material);
        out.push_back(entry!=nullptr);
        if(entry)
        {
            const std::uint32_t words[]={entry->id,entry->classID,entry->fileID,entry->nameIsNullForAnalysis,
                entry->name.empty(),entry->offset,entry->size,entry->payloadWritten,entry->object==f.material.get(),
                fat->FindByIDForAnalysis(entry->id)==entry};
            out.insert(out.end(),std::begin(words),std::end(words));
        }
        return out;
    }
    void EmptyMaterialStep(EmptyMaterialFixture& f,unsigned repeat,bool emit)
    {
        const auto target=Target(*f.object);const auto material=EmptyMaterialState(f);const auto errors=Error(f.errors);
        const auto before=Fat(f.manager);const auto raw=f.raw;
        Check(before==std::array<std::size_t,3>{repeat,0,repeat?2u:1u},"Native empty/one-entry input transition");
        const bool result=f.Index();auto* fat=f.manager.GetFATForAnalysis();
        const auto after=Fat(f.manager);const auto* entry=fat->FindByObjectForAnalysis(*f.material);
        Check(after[1]==0,"Empty-Material Index adds no external file metadata");
        Check(entry!=nullptr,"Empty-Material Index publishes a live metadata entry");
        const std::array<std::uint32_t,20> row{result,raw,f.raw,static_cast<std::uint32_t>(before[0]),
            static_cast<std::uint32_t>(before[2]),static_cast<std::uint32_t>(after[0]),static_cast<std::uint32_t>(after[2]),
            entry->id,entry->classID,entry->fileID,entry->nameIsNullForAnalysis&&entry->name.empty(),
            entry->offset,entry->size,entry->payloadWritten,entry->object==f.material.get(),fat->FindByIDForAnalysis(1)==entry,
            target==Target(*f.object),material==EmptyMaterialState(f),errors==Error(f.errors),
            f.manager.GetRegistrationCountForAnalysis()==1&&f.manager.FindForAnalysis(spDXMaterial::ClassID)==f.materialSerializer.get()};
        const std::array<std::uint32_t,20> want{1,raw,0,repeat,repeat?2u:1u,1,2,1,spDXMaterial::ClassID,0,1,0,0,0,1,1,1,1,1,1};
        Check(row==want,"Whole Empty-Material first insertion/repeat matches native represented transition");
        if(emit){for(std::size_t i=0;i<row.size();++i)std::cout<<(i?" ":"")<<row[i];std::cout<<'\n';}
    }
    void EmptyMaterialReplay()
    {
        unsigned kind=0,repeats=0;std::uint32_t raw=0;
        while(std::cin>>kind>>raw>>repeats)
        {
            Check(kind<3&&repeats==2,"Bounded three-family Empty-Material replay input");
            EmptyMaterialFixture f(kind,raw);for(unsigned i=0;i<repeats;++i)EmptyMaterialStep(f,i,true);
        }
        Check(std::cin.eof(),"Complete Empty-Material replay input");
    }
    void EmptyMaterialRefusal(EmptyMaterialFixture& f,const char* label)
    {
        const auto target=Target(*f.object);const auto material=EmptyMaterialState(f);const auto fat=EmptyMaterialFat(f);
        const auto errors=Error(f.errors);const auto raw=f.raw;
        Check(!f.Index()&&f.raw==raw&&target==Target(*f.object)&&material==EmptyMaterialState(f)&&fat==EmptyMaterialFat(f)&&errors==Error(f.errors),label);
    }
    struct ForeignEmptyMaterialSerializer final:spMaterialSerializer
    {
        mutable unsigned calls=0;
        const spRTTIRecord& vfunc_18() const noexcept override{++calls;return spMaterialSerializer::StaticRTTI();}
    };
    void EmptyMaterialContracts()
    {
        for(unsigned kind=0;kind<3;++kind)
        {EmptyMaterialFixture f(kind,kind==0?0:kind==1?0x1234:0xffffffff);EmptyMaterialStep(f,0,false);EmptyMaterialStep(f,1,false);}
        {EmptyMaterialFixture f(0,123);auto context=f.Context();context.lifetime=Lifetime::UnqualifiedMutable;
            Check(!f.serializer->IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)
                &&f.raw==123&&f.manager.GetFATForAnalysis()->GetResourceCountForAnalysis()==0,"Unqualified lifetime refuses before insertion");}
        {EmptyMaterialFixture f(0,123);f.object->SetMaterialForAnalysis(nullptr);EmptyMaterialRefusal(f,"Required Material cannot be null");}
        {EmptyMaterialFixture f(0,123);f.object->SetMaterialForAnalysis(std::make_shared<spBaseObject>());EmptyMaterialRefusal(f,"Wrong physical Material refuses");}
        {EmptyMaterialFixture f(0,123);f.object->SetFogForAnalysis(std::make_shared<spFog>());EmptyMaterialRefusal(f,"Fog insertion stays outside empty Material profile");}
        {EmptyMaterialFixture f(0,123);f.object->SetBoundRenderNodeForAnalysis(&f.node);EmptyMaterialRefusal(f,"RenderNode indexing stays outside Empty-Material profile");}
        {EmptyMaterialFixture f(0,123);f.material->SetName("outside-captured-null-name");EmptyMaterialRefusal(f,"Nonnull physical name refuses");}
        {EmptyMaterialFixture f(0,123);Check(f.material->SetPassForAnalysis(0,std::make_shared<spBaseObject>()),"Populated pass input");
            EmptyMaterialRefusal(f,"Nonzero pass count refuses before invoking a pass");}
        {EmptyMaterialFixture f(0,123);f.material->SetMaterialColorControllerForAnalysis(std::make_shared<spBaseObject>());
            EmptyMaterialRefusal(f,"Nonnull color controller refuses before callbacks");}
        {EmptyMaterialFixture f(0,123);Check(f.manager.GetFATForAnalysis()->SetNextResourceIDForAnalysis(7),"Wrong empty nextID input");EmptyMaterialRefusal(f,"Noninitial empty FAT refuses");}
        {EmptyMaterialFixture f(0,123);auto* fat=f.manager.GetFATForAnalysis();Check(fat->IndexObjectForAnalysis(spRenderNode::ClassID,f.node),"Other resource entry input");
            EmptyMaterialRefusal(f,"Foreign one-entry FAT refuses");}
        {EmptyMaterialFixture f(0,123);EmptyMaterialStep(f,0,false);f.raw=123;f.manager.GetFATForAnalysis()->FindByIDForAnalysis(1)->payloadWritten=true;
            EmptyMaterialRefusal(f,"Written entry stays outside profile");}
        {EmptyMaterialFixture f(0,123);EmptyMaterialStep(f,0,false);f.raw=123;f.manager.GetFATForAnalysis()->FindByIDForAnalysis(1)->classID=spBaseObject::ClassID;
            EmptyMaterialRefusal(f,"Incorrect entry class refuses");}
        {EmptyMaterialFixture f(0,123);Check(f.manager.RegisterForAnalysis(1,f.materialSerializer,spSerializerManager::PlatformAll,spSerializerManager::OperationBoth),"Extra binding input");
            EmptyMaterialRefusal(f,"Exactly one registration required");}
        {EmptyMaterialFixture f(0,123);spMaterialSerializer other;spProjectionEmptyMaterialIndexContextForAnalysis context{f.manager,f.errors,f.raw,other,Lifetime::StableBorrowed};
            Check(!f.serializer->IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123
                &&f.manager.GetFATForAnalysis()->GetResourceCountForAnalysis()==0,"Unregistered exact binding refuses");}
        {EmptyMaterialFixture f(0,123);ForeignEmptyMaterialSerializer other;spProjectionEmptyMaterialIndexContextForAnalysis context{f.manager,f.errors,f.raw,other,Lifetime::StableBorrowed};
            Check(!f.serializer->IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123&&other.calls==0,"Foreign Material serializer refuses before callbacks");}
        {EmptyMaterialFixture f(0,123);ForeignProjectionSerializer other;auto context=f.Context();
            Check(!other.IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123&&other.calls==0,"No foreign GetTarget callback");}
        {EmptyMaterialFixture f(2,123);spBoxProjectionSerializer box;auto context=f.Context();
            Check(!box.IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(context,*f.object)&&f.raw==123,"Wrong exact target family refuses");}
        {EmptyMaterialFixture f(0,123);f.manager.SetDispatchContextForAnalysis(spSerializerManager::PlatformCommon,spSerializerManager::OperationLoad);
            EmptyMaterialRefusal(f,"Load dispatch refuses");}
        {EmptyMaterialFixture f(0,123);Check(f.manager.SetSerializationPolicyForAnalysis(0),"Wrong policy input");EmptyMaterialRefusal(f,"Other policy refuses");}
        {EmptyMaterialFixture f(0,123);Check(f.errors.CreateErrorForAnalysis(spErrorSeverity::Error,1,nullptr,0)!=nullptr,"Queued Empty-Material error input");
            EmptyMaterialRefusal(f,"Queued errors refuse before allocation/clearing");}
        {EmptyMaterialFixture f(0,123);spPCErrorManager other;EmptyMaterialRefusal(f,"Noncurrent supplied ErrorManager refuses");}
        {EmptyMaterialFixture f(0,123);spSerializerManager other;EmptyMaterialRefusal(f,"Noncurrent supplied SerializerManager refuses");}
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();
        else if(argc==2&&std::string(argv[1])=="--preindexed-replay")ReplayPreindexed();
        else if(argc==2&&std::string(argv[1])=="--fog-only-replay")FogOnlyReplay();
        else if(argc==2&&std::string(argv[1])=="--empty-material-replay")EmptyMaterialReplay();
        else {Contracts();Queued();PreindexedContracts();FogOnlyContracts();EmptyMaterialContracts();std::cout<<"Projection Index contracts passed checks="<<checks<<'\n';}
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"ProjectionIndex "<<error.what()<<'\n';return 1;}
}

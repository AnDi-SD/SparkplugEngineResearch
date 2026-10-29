#include "spProjectionSerializer.h"
#include "spProjection.h"
#include "spBoxProjection.h"
#include "spRenderNode.h"
#include "spFog.h"
#include "spMaterialSerializer.h"
#include "spFogSerializer.h"
#include "spRenderNodeSerializer.h"
#include "Code/SparkplugDX/spDXMaterial.h"
#include "spResourceFATSerializer.h"
#include "spSerializerManager.h"
#include "Analysis/Host/spProjectionIndexHost.h"
#include "Analysis/PC/spSectionCursor.h"
#include <limits>
#include <utility>
#include <typeinfo>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> Create(){return std::make_unique<spProjectionSerializer>();}
        const spRTTIRecord record{spProjectionSerializer::ClassID,spRenderableSerializer::ClassID,
            "spProjectionSerializer",&spRenderableSerializer::StaticRTTI(),&Create,nullptr};
        const bool registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
        // Host-only dispatch substitution, not a recovered native class or
        // RTTI/receiver mapping. It is never registered, retained or cloned.
        // Shared IndexResource inserts first and skips this own callback on
        // duplicate. Borrowed context/target outlive this synchronous object.
        class EmptyMaterialOwnIndexDispatchForAnalysis final : public spSerializer
        {
        public:
            EmptyMaterialOwnIndexDispatchForAnalysis(
                spProjectionEmptyMaterialIndexContextForAnalysis& context,spDXMaterial& material) noexcept
                :context_(context),material_(material){}
            bool IndexRelationshipsWithContextForAnalysis(
                spSerializerManager& manager,spBaseObject& object) const override
            {
                if(&manager!=&context_.manager||&object!=&material_)return false;
                // Exact stable zero-pass/null-controller admission makes the
                // existing qualified Material tail pure (including its null
                // reference). Its unchanged generic semantics remain separate
                // from this explicit post-insertion nested warm-NULL gate.
                if(!context_.materialSerializer.spMaterialSerializer::IndexRelationshipsWithContextForAnalysis(manager,object))return false;
                return context_.errors.HandleWarmEmptyPcForAnalysis(context_.rawWord75AB9C,context_.lifetime)
                    ==spPCErrorManager::WarmEmptyHandleStatusForAnalysis::Completed;
            }
        private:
            spProjectionEmptyMaterialIndexContextForAnalysis& context_;
            spDXMaterial& material_;
        };
        bool FailWrite(std::string* error,const char* message)
        {if(error&&error->empty())*error=message;return false;}
        bool AdmitWrite(const spProjection& object,spSerializerManager* manager,std::string* error)
        {
            auto* fat=manager?manager->GetFATForAnalysis():nullptr;
            const auto* material=object.GetMaterialForAnalysis().get();
            const auto* fog=object.GetFogForAnalysis().get();
            if((material&&!dynamic_cast<const spDXMaterial*>(material))
                ||(fog&&!dynamic_cast<const spFog*>(fog)))
                return FailWrite(error,"Projection writer requires qualified DXMaterial/Fog types");
            const auto written=[&](const spBaseObject* value)
            {
                if(!value)return true;
                const auto* entry=fat?fat->FindByObjectForAnalysis(*value):nullptr;
                return entry&&entry->payloadWritten;
            };
            return (written(material)&&written(fog)&&written(object.GetBoundRenderNodeForAnalysis()))
                ||FailWrite(error,"Projection writer requires externally retained, already written relationship FAT inputs");
        }
        template<class Payload>bool WriteFixedPair(spStream& stream,std::uint32_t field,Payload&& payload)
        {
            // The native pair reserves Fixed8, writes two words separately,
            // then seeks back and rewrites its one-byte header. The general
            // portable DataBlock builder refuses fixed reservations; spell
            // this exact8-byte profile through its shared header encoder.
            using Blocks=spDataBlockSerializer;std::uint32_t start=0,end=0;
            if(!stream.GetCurrentPosition(start)||start>std::uint32_t(std::numeric_limits<std::int32_t>::max())-9
                ||!Blocks::WriteHeaderWithCodeForAnalysis(stream,field,8,Blocks::SizeCode::Fixed8)||!payload()
                ||!stream.GetCurrentPosition(end)||end-start!=9)return false;
            return stream.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(start))
                &&Blocks::WriteHeaderWithCodeForAnalysis(stream,field,8,Blocks::SizeCode::Fixed8)
                &&stream.Seek(spStream::SeekSource::essStart,static_cast<std::int32_t>(end));
        }
    }
    spProjectionSerializer::~spProjectionSerializer()=default;
    const spRTTIRecord& spProjectionSerializer::StaticRTTI() noexcept{(void)registered;return record;}
    const spRTTIRecord& spProjectionSerializer::vfunc_18() const noexcept{return record;}
    std::unique_ptr<spBaseObject> spProjectionSerializer::vfunc_10(spCloneManager& manager) const
    {
        auto clone=std::make_unique<spProjectionSerializer>();manager.RegisterCloneForAnalysis(*this,*clone);
        return spSerializer::vfunc_14(*clone,manager)?std::move(clone):nullptr;
    }
    spClassID spProjectionSerializer::GetTargetClassIDForAnalysis() const noexcept{return TargetClassID;}
    bool spProjectionSerializer::ReadPayloadForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();auto* target=dynamic_cast<spProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)
        {context.failed=true;if(error)*error="Projection reader requires its own compatible section adapter";return false;}
        return ReadProjectionFieldsForAnalysis(context,source,size,*target,true,error);
    }
    bool spProjectionSerializer::ReadProjectionFieldsForAnalysis(spSerializerReadContextForAnalysis& context,
        spStream& source,std::uint32_t size,spProjection& object,bool requireEnd,std::string* error) const
    {
        std::uint32_t start=0,position=0;
        if(!source.GetCurrentPosition(start)
            ||!ReadRenderableFieldsForAnalysis(context,source,size,object,false,error)
            ||!source.GetCurrentPosition(position)||position<start||position-start>=size)
        {context.failed=true;if(error&&error->empty())*error="Missing Projection section";return false;}
        evidence::pc::serialization::SectionCursor cursor(context,source,size-(position-start),requireEnd,error);
        while(const auto* header=cursor.Next())
        {
            if(header->IsTerminator())return true;
            if(header->fieldID<2)
            {
                if(header->payloadSize!=8)return cursor.Fail("Invalid Projection word pair");
                const auto first=2u*header->fieldID;std::uint32_t word=0;
                // Native commits the first word before reading the second.
                if(!source.Read(word))return cursor.Fail("Cannot read first Projection word");
                object.state_.words5C[first]=word;
                if(!source.Read(word))return cursor.Fail("Cannot read second Projection word");
                object.state_.words5C[first+1]=word;
            }
            else if(header->fieldID<=4)
            {
                std::uint8_t byte=0;if(!cursor.Read(byte))return cursor.Fail("Invalid Projection byte");
                if(header->fieldID==2)object.state_.word6C=byte;
                else object.state_.flags58[header->fieldID-2]=byte;
            }
            else if(header->fieldID==5)
            {
                auto* raw=ReadFieldReferenceForAnalysis(context,spRenderNode::ClassID,source,*header,error);
                if(context.failed)return false;
                auto* node=dynamic_cast<spRenderNode*>(raw);
                // Native resolver ignores expectedClassID. Reject a wrong
                // graph explicitly before publishing an unsafe typed pointer.
                if(raw&&!node)return cursor.Fail("Projection bound relationship is not a typed RenderNode");
                object.SetBoundRenderNodeForAnalysis(node);
            }
            else if(!cursor.Skip())return cursor.Fail("Cannot skip Projection field");
        }
        return false;
    }
    bool spProjectionSerializer::IndexRelationshipsForAnalysis(spBaseObject&) const{return false;}
    bool spProjectionSerializer::IndexRelationshipsWithContextForAnalysis(spSerializerManager&,spBaseObject&) const{return false;}
    bool spProjectionSerializer::IndexProjectionNullFieldsForAnalysis(
        spProjectionNullIndexContextForAnalysis& context,spProjection& object) const
    {
        const auto* fat=context.manager.GetFATForAnalysis();
        // Analytical admission precedes every effect. Native nonnull reference
        // recursion, cold globals and other manager/FAT profiles are unqualified.
        if(spSerializerManager::GetInstance()!=&context.manager
            ||context.manager.GetPlatformMaskForAnalysis()!=spSerializerManager::PlatformCommon
            ||context.manager.GetOperationMaskForAnalysis()!=spSerializerManager::OperationSave
            ||context.manager.GetSerializationPolicyForAnalysis()!=2
            ||!fat||fat->GetResourceCountForAnalysis()!=0||fat->GetFileCountForAnalysis()!=0
            ||fat->GetNextResourceIDForAnalysis()!=1
            ||object.GetMaterialForAnalysis()||object.GetFogForAnalysis()
            ||object.GetBoundRenderNodeForAnalysis())return false;
        const auto reference=[&](spBaseObject* value)
        {
            // Actual467300 calls the real common error gate BEFORE its null
            // check. Native empty Handle returns false; the reference succeeds.
            return context.errors.HandleWarmEmptyPcForAnalysis(context.rawWord75AB9C,context.lifetime)
                ==spPCErrorManager::WarmEmptyHandleStatusForAnalysis::Completed
                &&IndexReferenceForAnalysis(context.manager,value);
        };
        // Parent Renderable fields precede Projection's borrowed bound84.
        return reference(object.GetMaterialForAnalysis().get())
            &&reference(object.GetFogForAnalysis().get())
            &&reference(object.GetBoundRenderNodeForAnalysis());
    }
    bool spProjectionSerializer::IndexNullRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionNullIndexContextForAnalysis& context,spBaseObject& object) const
    {
        auto* target=dynamic_cast<spProjection*>(&object);
        return target&&GetTargetClassIDForAnalysis()==TargetClassID
            &&IndexProjectionNullFieldsForAnalysis(context,*target);
    }
    bool spProjectionSerializer::IndexProjectionPreindexedFieldsForAnalysis(
        spProjectionPreindexedIndexContextForAnalysis& context,spProjection& object) const
    {
        auto& manager=context.manager;auto* fat=manager.GetFATForAnalysis();
        // Exact portable types are host guards before any unqualified virtual
        // dispatch. Current Common/Save lookup equivalence is admitted; this
        // does not introspect or claim native registration masks FF/3.
        if(typeid(context.materialSerializer)!=typeid(spMaterialSerializer)
            ||typeid(context.fogSerializer)!=typeid(spFogSerializer)
            ||typeid(context.nodeSerializer)!=typeid(spRenderNodeSerializer)
            ||spSerializerManager::GetInstance()!=&manager
            ||manager.GetPlatformMaskForAnalysis()!=spSerializerManager::PlatformCommon
            ||manager.GetOperationMaskForAnalysis()!=spSerializerManager::OperationSave
            ||manager.GetSerializationPolicyForAnalysis()!=2
            ||manager.GetRegistrationCountForAnalysis()!=3||!fat||fat->GetFileCountForAnalysis()!=0)
            return false;
        auto* material=object.GetMaterialForAnalysis().get();
        auto* fog=object.GetFogForAnalysis().get();auto* node=object.GetBoundRenderNodeForAnalysis();
        // DXMaterial/Fog are final. RenderNode permits arbitrary derivatives
        // elsewhere, but their callbacks are outside this explicit profile.
        if((material&&!dynamic_cast<spDXMaterial*>(material))
            ||(fog&&!dynamic_cast<spFog*>(fog))
            ||(node&&typeid(*node)!=typeid(spRenderNode))||(!material&&!fog&&!node))return false;
        const spClassID classes[]={spDXMaterial::ClassID,spFog::ClassID,spRenderNode::ClassID};
        spSerializer* bindings[]={&context.materialSerializer,&context.fogSerializer,&context.nodeSerializer};
        spBaseObject* values[]={material,fog,node};const std::uint32_t ids[]={7,9,11};
        std::size_t count=0;std::uint32_t nextID=1;
        for(std::size_t i=0;i<3;++i)
        {
            if(manager.FindForAnalysis(classes[i])!=bindings[i])return false;
            if(!values[i])continue;
            const auto* entry=fat->FindByObjectForAnalysis(*values[i]);
            if(!entry||entry!=fat->FindByIDForAnalysis(ids[i])||entry->id!=ids[i]
                ||entry->classID!=classes[i]||entry->object!=values[i]||entry->fileID!=0
                ||!entry->nameIsNullForAnalysis||!entry->name.empty()
                ||entry->offset!=0||entry->size!=0||entry->payloadWritten)return false;
            ++count;nextID=ids[i]+1;
        }
        if(fat->GetResourceCountForAnalysis()!=count||fat->GetNextResourceIDForAnalysis()!=nextID)return false;
        const auto reference=[&](spBaseObject* value)
        {
            // Native4672C0 executes actual466FA0, observes duplicate AL0 and
            // succeeds without own recursion. Shared portable IndexResource
            // has an existing-entry early return with the same bounded effect.
            return context.errors.HandleWarmEmptyPcForAnalysis(context.rawWord75AB9C,context.lifetime)
                ==spPCErrorManager::WarmEmptyHandleStatusForAnalysis::Completed
                &&IndexReferenceForAnalysis(manager,value);
        };
        return reference(material)&&reference(fog)&&reference(node);
    }
    bool spProjectionSerializer::IndexPreindexedRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionPreindexedIndexContextForAnalysis& context,spBaseObject& object) const
    {
        // The parent serializer's native fixture uses a physical Box target.
        if(typeid(*this)!=typeid(spProjectionSerializer)||typeid(object)!=typeid(spBoxProjection))return false;
        auto* target=dynamic_cast<spBoxProjection*>(&object);
        return target&&IndexProjectionPreindexedFieldsForAnalysis(context,*target);
    }
    bool spProjectionSerializer::IndexProjectionFogOnlyFieldsForAnalysis(
        spProjectionFogOnlyIndexContextForAnalysis& context,spProjection& object) const
    {
        auto& manager=context.manager;auto* fat=manager.GetFATForAnalysis();
        if(typeid(context.fogSerializer)!=typeid(spFogSerializer)
            ||spSerializerManager::GetInstance()!=&manager
            ||manager.GetPlatformMaskForAnalysis()!=spSerializerManager::PlatformCommon
            ||manager.GetOperationMaskForAnalysis()!=spSerializerManager::OperationSave
            ||manager.GetSerializationPolicyForAnalysis()!=2
            ||manager.GetRegistrationCountForAnalysis()!=1||!fat||fat->GetFileCountForAnalysis()!=0)
            return false;
        auto* fog=dynamic_cast<spFog*>(object.GetFogForAnalysis().get());
        // Fog is final. Its native own Index is an unconditional leaf; scalar
        // payload is preserved as data, no renderer or payload semantics added.
        if(object.GetMaterialForAnalysis()||object.GetBoundRenderNodeForAnalysis()
            ||!fog||fog->GetName()!=nullptr
            ||manager.FindForAnalysis(spFog::ClassID)!=&context.fogSerializer)return false;
        const auto count=fat->GetResourceCountForAnalysis();
        const auto* entry=fat->FindByObjectForAnalysis(*fog);
        if(count==0)
        {
            if(fat->GetNextResourceIDForAnalysis()!=1||entry||fat->FindByIDForAnalysis(1))return false;
        }
        else if(count==1)
        {
            if(fat->GetNextResourceIDForAnalysis()!=2||!entry||entry!=fat->FindByIDForAnalysis(1)
                ||entry->id!=1||entry->classID!=spFog::ClassID||entry->object!=fog
                ||entry->fileID!=0||!entry->nameIsNullForAnalysis||!entry->name.empty()
                ||entry->offset!=0||entry->size!=0||entry->payloadWritten)return false;
        }
        else return false;
        const auto reference=[&](spBaseObject* value)
        {
            return context.errors.HandleWarmEmptyPcForAnalysis(context.rawWord75AB9C,context.lifetime)
                ==spPCErrorManager::WarmEmptyHandleStatusForAnalysis::Completed
                &&IndexReferenceForAnalysis(manager,value);
        };
        // Shared IndexResource inserts before invoking the actual typed Fog
        // own adapter. Repeat uses its existing-entry early return. Native
        // FAT allocation sizes/counts are not portable container requirements.
        return reference(nullptr)&&reference(fog)&&reference(nullptr);
    }
    bool spProjectionSerializer::IndexFogOnlyRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionFogOnlyIndexContextForAnalysis& context,spBaseObject& object) const
    {
        // Host exact types supply admission; no GetTarget virtual is called.
        if(typeid(*this)!=typeid(spProjectionSerializer)||typeid(object)!=typeid(spBoxProjection))return false;
        auto* target=dynamic_cast<spBoxProjection*>(&object);
        return target&&IndexProjectionFogOnlyFieldsForAnalysis(context,*target);
    }
    bool spProjectionSerializer::IndexProjectionEmptyMaterialFieldsForAnalysis(
        spProjectionEmptyMaterialIndexContextForAnalysis& context,spProjection& object) const
    {
        auto& manager=context.manager;auto* fat=manager.GetFATForAnalysis();
        if(typeid(context.materialSerializer)!=typeid(spMaterialSerializer)
            ||spSerializerManager::GetInstance()!=&manager
            ||manager.GetPlatformMaskForAnalysis()!=spSerializerManager::PlatformCommon
            ||manager.GetOperationMaskForAnalysis()!=spSerializerManager::OperationSave
            ||manager.GetSerializationPolicyForAnalysis()!=2
            ||manager.GetRegistrationCountForAnalysis()!=1||!fat||fat->GetFileCountForAnalysis()!=0)return false;
        auto* material=dynamic_cast<spDXMaterial*>(object.GetMaterialForAnalysis().get());
        // Final physical type and a stable pure empty tail exclude callbacks,
        // populated passes/controllers and the unknown native power word B8.
        if(!material||object.GetFogForAnalysis()||object.GetBoundRenderNodeForAnalysis()
            ||material->GetName()!=nullptr||material->GetPassCountForAnalysis()!=0
            ||material->GetMaterialColorControllerForAnalysis()!=nullptr
            ||manager.FindForAnalysis(spDXMaterial::ClassID)!=&context.materialSerializer)return false;
        const auto count=fat->GetResourceCountForAnalysis();const auto* entry=fat->FindByObjectForAnalysis(*material);
        if(count==0)
        {
            if(fat->GetNextResourceIDForAnalysis()!=1||entry||fat->FindByIDForAnalysis(1))return false;
        }
        else if(count==1)
        {
            if(fat->GetNextResourceIDForAnalysis()!=2||!entry||entry!=fat->FindByIDForAnalysis(1)
                ||entry->id!=1||entry->classID!=spDXMaterial::ClassID||entry->object!=material
                ||entry->fileID!=0||!entry->nameIsNullForAnalysis||!entry->name.empty()
                ||entry->offset!=0||entry->size!=0||entry->payloadWritten)return false;
        }
        else return false;
        const auto gate=[&]
        {
            return context.errors.HandleWarmEmptyPcForAnalysis(context.rawWord75AB9C,context.lifetime)
                ==spPCErrorManager::WarmEmptyHandleStatusForAnalysis::Completed;
        };
        if(!gate())return false;
        // Binding lookup above is pure analytical admission. Native selects
        // that actual binding after its gate; this local source adapter is an
        // explicit receiver substitution, not native callback/RTTI parity.
        EmptyMaterialOwnIndexDispatchForAnalysis dispatch(context,*material);
        if(!dispatch.IndexResourceForAnalysis(manager,*material))return false;
        // Shared insertion has already run the adapter own tail/nested gate
        // exactly when new. Duplicate returns before that own dispatch.
        return gate()&&IndexReferenceForAnalysis(manager,nullptr)
            &&gate()&&IndexReferenceForAnalysis(manager,nullptr);
    }
    bool spProjectionSerializer::IndexEmptyMaterialOnlyRelationshipsWithWarmEmptyPcForAnalysis(
        spProjectionEmptyMaterialIndexContextForAnalysis& context,spBaseObject& object) const
    {
        if(typeid(*this)!=typeid(spProjectionSerializer)||typeid(object)!=typeid(spBoxProjection))return false;
        auto* target=dynamic_cast<spBoxProjection*>(&object);
        return target&&IndexProjectionEmptyMaterialFieldsForAnalysis(context,*target);
    }
    bool spProjectionSerializer::WriteProjectionFieldsForAnalysis(spSerializerManager* manager,
        spStream& stream,const spProjection& object,std::string* error) const
    {
        if(!AdmitWrite(object,manager,error)||!WriteRenderableFieldsForAnalysis(manager,stream,object,error))return false;
        spDataBlockSerializer blocks;if(!blocks.BeginObjectForAnalysis(stream,&object))return FailWrite(error,"Cannot begin Projection write section");
        const auto& state=object.GetProjectionStateForAnalysis();
        for(std::uint32_t field=0;field<2;++field)
        {
            const auto first=2*field;
            if(state.words5C[first]!=0x3F800000||state.words5C[first+1]!=0x3F800000)
                if(!WriteFixedPair(stream,field,[&](){return stream.Write(state.words5C[first])&&stream.Write(state.words5C[first+1]);}))
                    return FailWrite(error,"Cannot write Projection raw word pair");
        }
        const auto word=state.word6C;
        if(word!=2)
        {
            const auto byte=static_cast<std::uint8_t>(word);
            if(!blocks.WriteFieldForAnalysis(stream,2,&byte,1))return FailWrite(error,"Cannot write Projection low byte");
        }
        for(std::uint32_t field=3;field<=4;++field)
        {
            const auto byte=state.flags58[field-2];
            if(byte&&!blocks.WriteFieldForAnalysis(stream,field,&byte,1))return FailWrite(error,"Cannot write Projection raw flag");
        }
        if(const auto* node=object.GetBoundRenderNodeForAnalysis())
        {
            if(!blocks.WriteBeginForAnalysis(5,spDataBlockSerializer::SizeCode::UInt32)
                ||!WriteReferenceForAnalysis(*manager,stream,node,error)||!blocks.WriteEndForAnalysis(5))
                return FailWrite(error,"Cannot write Projection existing Node reference");
        }
        return blocks.FinalizeObjectForAnalysis()||FailWrite(error,"Cannot terminate Projection write section");
    }
    bool spProjectionSerializer::WritePayloadForAnalysis(spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)return FailWrite(error,"Derived Projection writer requires its own section adapter");
        return WriteProjectionFieldsForAnalysis(nullptr,stream,*target,error);
    }
    bool spProjectionSerializer::WritePayloadWithContextForAnalysis(spSerializerManager& manager,spStream& stream,const spBaseObject& object,std::string* error) const
    {
        if(error)error->clear();const auto* target=dynamic_cast<const spProjection*>(&object);
        if(!target||GetTargetClassIDForAnalysis()!=TargetClassID)return FailWrite(error,"Derived Projection writer requires its own section adapter");
        return WriteProjectionFieldsForAnalysis(&manager,stream,*target,error);
    }
}

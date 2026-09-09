// Tests the HOST observation boundary. No new game serializer or RTTI class.
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceFATSerializer.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/Sparkplug/spNodeSerializer.h"
#include "Code/Sparkplug/spNode.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sparkplug::reconstruction;
namespace {
using Bytes=std::vector<std::uint8_t>;
using Context=spSerializerReadContextForAnalysis;
int checks=0;
void Check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
template<class T>void Add(Bytes& bytes,T value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));}
void Open(spMemoryStream& stream,const Bytes& bytes){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"allocate input");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());}
void Register(spSerializerManager& manager){(void)spNode::StaticRTTI();Check(manager.RegisterForAnalysis(spNode::ClassID,std::make_shared<spNodeSerializer>(),255,3),"actual Node serializer");}
Bytes NodeFile(bool malformed=false){
    // Independent compact test envelope: the unknown 8-byte Node field looks
    // exactly like ID2,size0, but the actual reader skips it as field9.
    Bytes object;Add(object,spNode::ClassID);Add(object,0x4f4f4253u);
    object.push_back(0xa9);object.push_back(8);Add(object,2u);Add(object,0u);
    object.push_back(0xa5);object.push_back(17);Add(object,2u);Add(object,9u);
    Add(object,spNode::ClassID);Add(object,0x4f4f4253u);object.push_back(malformed?0x60:0);
    object.push_back(0);Check(object.size()==38,"fixed fixture extent");
    Bytes file;for(auto word:{0x53504646u,0x26u,0u,110u,2u,72u,38u})Add(file,word);
    Add(file,2u);for(auto id:{1u,2u}){Add(file,id);Add(file,std::uint16_t(0));Add(file,spNode::ClassID);Add(file,id==1?0u:28u);Add(file,id==1?38u:9u);}
    Add(file,0u);file.insert(file.end(),object.begin(),object.end());return file;
}
void Whole(){
    for(unsigned mode=0;mode<4;++mode){
        spSerializerManager manager;spResourceManager resources;Register(manager);
        spMemoryStream input,other;const auto file=NodeFile(mode==3);Open(input,file);Open(other,file);
        Context context(manager,resources);context.captureFileObjectIDsForAnalysis=true;
        if(mode!=1)context.fileReadTraceSourceForAnalysis=mode==2?&other:&input;
        std::string error;auto* root=dynamic_cast<spNode*>(manager.LoadResourcesForAnalysis(input,context,&error));
        const auto& trace=context.fileReadTraceForAnalysis;
        if(mode==3){Check(!root&&context.failed&&!trace.complete,"failed read never publishes complete trace");Check(trace.valid&&trace.referenceReads.size()==1&&!trace.referenceReads[0].success,"failure retains unsuccessful exact observation");continue;}
        Check(root&&root->GetChildCountForAnalysis()==1&&!context.failed,"trace mode does not change actual Node graph");
        Check(context.currentFileReadObjectIdForAnalysis==0,"consumer scope restores to zero");
        if(mode==1){Check(!trace.valid&&!trace.complete&&trace.referenceReads.empty()&&trace.payloadReads.empty(),"disabled trace has no observations");continue;}
        if(mode==2){Check(!trace.valid&&trace.complete&&!trace.diagnostic.empty()&&trace.referenceReads.empty(),"wrong source only invalidates trace; whole read still succeeds");continue;}
        Check(trace.valid&&trace.complete&&trace.dataPhysicalOrigin==72,"explicit whole-file provenance");
        Check(trace.referenceReads.size()==1,"guard rewind and unknown 8-byte payload create no extra reference");
        const auto& reference=trace.referenceReads.front();
        Check(reference.consumerId==1&&reference.id==2&&reference.inlineSize==9&&reference.idPhysicalOffset==92&&reference.sizePhysicalOffset==96,"actual resolver reports absolute wire positions and consumer");
        Check(reference.success&&reference.resolution==Context::ReferenceResolutionForAnalysis::Created,"new inline reference result");
        Check(trace.payloadReads.size()==3,"outer, inline, and later outer already-materialized entry are distinct");
        for(const auto& entry:context.fileObjectsForAnalysis){
            const auto complete=std::count_if(trace.payloadReads.begin(),trace.payloadReads.end(),[&](const auto& row){return row.complete&&row.kind!=Context::PayloadReadKindForAnalysis::SkippedExisting&&row.kind!=Context::PayloadReadKindForAnalysis::SkippedCache&&row.objectId==entry.id&&row.physicalOffset==72+entry.offset&&row.size==entry.size;});
            Check(complete==1,"completed payload spans match each FAT identity exactly once");
        }
        const auto& skipped=trace.payloadReads.back();Check(skipped.kind==Context::PayloadReadKindForAnalysis::SkippedExisting&&!skipped.complete&&skipped.physicalOffset==100,"unvisited FAT range is not coverage");
        Check(std::memcmp(input.GetBuffer(),file.data(),file.size())==0,"trace does not modify input bytes");
    }
}
void Direct(){
    for(unsigned mode=0;mode<3;++mode){
        spSerializerManager manager;spResourceManager resources;Register(manager);
        Bytes directory;Add(directory,1u);Add(directory,2u);Add(directory,std::uint16_t(0));Add(directory,spNode::ClassID);Add(directory,0u);Add(directory,9u);
        spMemoryStream index;Open(index,directory);Check(manager.GetFATForAnalysis()->LoadIndexForAnalysis(index),"prebound directory");
        auto node=std::make_shared<spNode>();manager.GetFATForAnalysis()->FindByIDForAnalysis(2)->object=node.get();
        Bytes bytes;Add(bytes,2u);Add(bytes,9u);Add(bytes,spNode::ClassID);Add(bytes,0x4f4f4253u);bytes.push_back(0);Add(bytes,0u);
        spMemoryStream input,ids;Open(input,bytes);Open(ids,Bytes{2,0,0,0});
        Context context(manager,resources);context.externalOwners.push_back(node);context.fileReadTraceSourceForAnalysis=&input;
        context.ResetFileReadTraceForAnalysis(input);context.SetFileReadTraceDataOriginForAnalysis(input);
        if(mode==1)Check(input.Seek(spStream::SeekSource::essStart,4),"split-stream payload begins at size");
        if(mode==2)context.fileReadTraceForAnalysis.referenceReads.resize(Context::MaximumFileReadTraceRowsForAnalysis);
        Check(spSerializer::ReadReferenceForAnalysis(context,0,mode==1?ids:input,input)==node.get()&&!context.failed,"observation provenance or capacity failure never changes resolver");
        const auto& trace=context.fileReadTraceForAnalysis;
        if(mode==1){Check(!trace.valid&&!trace.complete&&!trace.diagnostic.empty(),"different ID stream requires explicit provenance and is rejected only as a trace");continue;}
        if(mode==2){Check(!trace.valid&&trace.referenceReads.size()==Context::MaximumFileReadTraceRowsForAnalysis&&!trace.diagnostic.empty(),"bounded trace stops growing");continue;}
        Check(!spSerializer::ReadReferenceForAnalysis(context,0,input,input)&&!context.failed,"actual null reference");
        Check(trace.valid&&!trace.complete&&trace.referenceReads.size()==2,"direct resolver observations are not a completed file load");
        Check(trace.referenceReads[0].resolution==Context::ReferenceResolutionForAnalysis::Existing&&trace.referenceReads[0].success&&trace.referenceReads[0].idPhysicalOffset==0,"existing pointer observation");
        Check(trace.payloadReads.size()==1&&trace.payloadReads[0].kind==Context::PayloadReadKindForAnalysis::SkippedExisting&&trace.payloadReads[0].physicalOffset==8&&trace.payloadReads[0].complete,"successful skip is explicitly distinguished from payload read");
        Check(trace.referenceReads[1].resolution==Context::ReferenceResolutionForAnalysis::Null&&trace.referenceReads[1].sizePhysicalOffset==0xFFFFFFFFu&&trace.referenceReads[1].idPhysicalOffset==17,"null has no size word");
    }
}
}
int main(){try{Whole();Direct();std::cout<<"PASS "<<checks<<'/'<<checks<<": optional file reference-read observations\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

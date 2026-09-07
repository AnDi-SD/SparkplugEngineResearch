#include "Code/SparkplugPC/spPCVertexDeclaration.h"
#include "Code/SparkplugPC/spPCRenderer.h"
#include "Code/SparkplugDX/spDXMesh.h"
#include "Code/Sparkplug/spVertexBuffer.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace sparkplug::reconstruction;
namespace
{
    int checks=0;
    void Check(bool condition,const char* label) {++checks;if(!condition)throw std::runtime_error(label);}
    std::vector<std::uint32_t> Cases()
    {
        std::vector<std::uint32_t> values{0,0xffffffff,0x840,0x940,0x20,0x93e,0x1fffff,6,0xe,0x1e,0x3e};
        for(unsigned i=0;i<32;++i)values.push_back(1u<<i);
        std::uint32_t state=0x534d4f;
        for(unsigned i=0;i<64;++i) {state=state*1664525u+1013904223u;values.push_back(state&0x1fffff);}
        std::vector<std::uint32_t> result;
        for(auto v:values)if(std::find(result.begin(),result.end(),v)==result.end())result.push_back(v);
        return result;
    }
    std::string Hex(const std::vector<spPCVertexElementForAnalysis>& elements)
    {
        std::ostringstream text;text<<std::hex<<std::setfill('0');
        const auto* bytes=reinterpret_cast<const unsigned char*>(elements.data());
        for(std::size_t i=0;i<elements.size()*8;++i)text<<std::setw(2)<<unsigned(bytes[i]);
        return text.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        const bool capture=argc==2&&std::strcmp(argv[1],"--capture")==0;
        if(capture)std::cout<<'[';
        bool comma=false;
        for(auto flags:Cases())
        {
            spPCVertexDeclaration declaration;
            Check(declaration.InitializeForAnalysis(flags),"declaration initialization");
            const auto& elements=declaration.GetElementsForAnalysis();
            Check(!elements.empty()&&elements.back().stream==0xff&&elements.back().type==0x11,"exact native terminator");
            Check(elements.size()*8<=declaration.GetNativeAllocationBytesForAnalysis(),"used elements bounded by original allocation");
            if(capture)
            {
                if(comma)std::cout<<',';comma=true;
                std::cout<<'['<<flags<<','<<declaration.GetFVFCodeForAnalysis()<<','
                    <<declaration.GetNativeAllocationBytesForAnalysis()<<",\""<<Hex(elements)<<"\"]";
            }
        }
        if(capture){std::cout<<"]\n";return 0;}
        Check(spDXVertexDeclaration::StaticRTTI().factory==nullptr,"base has no RTTI factory");
        Check(spDXVertexDeclaration::StaticRTTI().baseClassID==spCrossPlatform::ClassID,"original base chain");
        spPCVertexDeclaration packed;packed.SetName("PC format");
        Check(packed.InitializeForAnalysis(0x93e),"packed initialization");
        Check(packed.GetNativeAllocationBytesForAnalysis()==72&&packed.GetElementsForAnalysis().size()==6,"native72 allocation only48 used");
        auto copy=packed.Clone();auto* blank=dynamic_cast<spPCVertexDeclaration*>(copy.get());
        Check(blank&&!blank->IsInitializedForAnalysis()&&blank->GetFVFCodeForAnalysis()==0,"concrete clone leaves native payload blank");
        Check(blank&&std::strcmp(blank->GetName(),"PC format")==0,"clone copies inherited name");
        spPCVertexDeclaration gap;Check(gap.InitializeForAnalysis(0x400|0x800),"gap mask initialization");
        Check(gap.GetElementsForAnalysis()[2].offset==28,"bit400 advances16 despite type2");
        std::shared_ptr<spPCVertexDeclaration> surviving;
        {
            spPCRenderer renderer;
            auto first=renderer.GetVertexDeclarationForAnalysis(0x940);
            Check(first&&first==renderer.GetVertexDeclarationForAnalysis(0x940),"renderer reuses declaration object by component mask");
            Check(first!=renderer.GetVertexDeclarationForAnalysis(0x840)&&renderer.GetVertexDeclarationCountForAnalysis()==2,"distinct masks own distinct entries");
            spIndexBuffer indices;spVertexBuffer vertices;
            Check(indices.InitializeForAnalysis(2,spIndexBuffer::eIndexBufferType::Type3),"logo topology");
            Check(vertices.InitializeForAnalysis(0x940,4),"logo vertex format");
            spDXMeshCombiner combiner;
            Check(combiner.InitializeForAnalysis(0x940,8,288,16),"raw planning component mask is not converted FVF");
            spDXMesh one,two;
            Check(one.InitializeFromBuffersForAnalysis(indices,vertices,false,&combiner,&renderer),"first mesh uses common declaration cache");
            Check(two.InitializeFromBuffersForAnalysis(indices,vertices,false,&combiner,&renderer),"second mesh range and common declaration");
            Check(one.GetVertexDeclarationForAnalysis()==first&&two.GetVertexDeclarationForAnalysis()==first,"mesh stores actual shared declaration object");
            Check(two.GetVertexBeginForAnalysis()==4&&two.GetIndexBeginForAnalysis()==4&&combiner.IsFullForAnalysis(),"two meshes retain disjoint ranges");
            Check(one.GetFVFCodeForAnalysis()==0x152&&combiner.GetFVFCodeForAnalysis()==0x940,"planning/raw mask and converted mesh FVF remain distinct");
            surviving=first;
            auto copied=renderer.Clone();auto* blankRenderer=dynamic_cast<spPCRenderer*>(copied.get());
            Check(blankRenderer&&blankRenderer->GetVertexDeclarationCountForAnalysis()==0,"renderer clone has blank declaration cache");
        }
        Check(surviving&&surviving->GetFVFCodeForAnalysis()==0x152,"explicit shared host ownership survives analytical renderer");
        std::cout<<"PASS "<<checks<<'/'<<checks<<": portable PC vertex declaration\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

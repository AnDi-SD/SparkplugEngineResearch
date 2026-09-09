// Golden states/wire bytes: actual PC simple-bv captures, 2026-09-10 cycle.
#include "Code/Sparkplug/spSphereBV.h"
#include "Code/Sparkplug/spBoxBV.h"
#include "Code/Sparkplug/spSphereBVSerializer.h"
#include "Code/Sparkplug/spBoxBVSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;
    int checks=0;
    void Check(bool value,const std::string& message)
    {++checks;if(!value)throw std::runtime_error(message);}
    Bytes Parse(const std::string& text)
    {
        Check(text.size()<=8192&&text.size()%2==0,"capture input is bounded hex");
        const auto digit=[](char c)->unsigned
        {
            if(c>='0'&&c<='9')return unsigned(c-'0');
            if(c>='a'&&c<='f')return unsigned(c-'a'+10);
            if(c>='A'&&c<='F')return unsigned(c-'A'+10);
            throw std::runtime_error("invalid hex digit");
        };
        Bytes result;for(std::size_t i=0;i<text.size();i+=2)
            result.push_back(static_cast<std::uint8_t>((digit(text[i])<<4)|digit(text[i+1])));
        return result;
    }
    std::string Hex(const void* data,std::size_t size)
    {
        static const char digits[]="0123456789abcdef";
        const auto* p=static_cast<const std::uint8_t*>(data);std::string result;
        for(std::size_t i=0;i<size;++i){result+=digits[p[i]>>4];result+=digits[p[i]&15];}
        return result;
    }
    template<class T>std::string Bits(const T& value){return Hex(&value,sizeof(value));}
    void Open(spMemoryStream& stream,const Bytes& bytes={})
    {
        Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"test stream allocation");
        if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
        Check(stream.Seek(spStream::SeekSource::essStart,0),"test stream rewind");
    }
    std::string State(const spSphereBV& object)
    {
        auto sphere=Bits(object.GetBoundingCenterForAnalysis())+Bits(object.GetBoundingRadiusForAnalysis());
        return "{\"sphereBits\":\""+sphere+"\",\"positionBits\":\""+Bits(object.GetPositionForAnalysis())
            +"\",\"valueBits\":\""+Bits(object.GetRadiusForAnalysis())+"\"}";
    }
    std::string State(const spBoxBV& object)
    {
        auto sphere=Bits(object.GetBoundingCenterForAnalysis())+Bits(object.GetBoundingRadiusForAnalysis());
        return "{\"sphereBits\":\""+sphere+"\",\"positionBits\":\""+Bits(object.GetPositionForAnalysis())
            +"\",\"valueBits\":\""+Bits(object.GetSizeForAnalysis())+"\"}";
    }
    template<class Object,class Serializer>struct Fixture
    {
        Object object;
        Serializer serializer;
        spSerializerManager manager;
        spResourceManager resources;
        spSerializerReadContextForAnalysis context{manager,resources};
        spMemoryStream stream;
        std::string error;
        bool Read(const std::string& hex)
        {
            const auto bytes=Parse(hex);Open(stream,bytes);
            return serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),object,&error);
        }
        std::string Write()
        {
            Open(stream);const bool result=serializer.WritePayloadForAnalysis(stream,object,&error);
            Check(result,error);std::uint32_t size=0;Check(stream.GetSize(&size),"written extent");
            return Hex(stream.GetBuffer(),size);
        }
    };
    using Sphere=Fixture<spSphereBV,spSphereBVSerializer>;
    using Box=Fixture<spBoxBV,spBoxBVSerializer>;
    const std::string Position="a00c0000803f000000c000004040";
    const std::string SphereValue="a1040000e8c0";
    const std::string BoxValue="a10c00000040000080c000008040";
    template<class F>void Common(const std::string& value,const std::string& expectedWrite)
    {
        F normal;const auto defaults=State(normal.object);
        Check(normal.Read(Position+value+"00"),normal.error);
        Check(normal.object.GetPositionForAnalysis()==spBoundingVolume::Vector3{1,-2,3},"original signed position");
        Check(normal.object.GetBoundingCenterForAnalysis()==normal.object.GetPositionForAnalysis(),"reader writes the original base mirror");
        Check(normal.Write()==expectedWrite,"original negative-valued writer bytes");

        F repeat;
        const std::string firstValue=std::is_same_v<F,Sphere>?"6100000000":"a10c000000000000000000000000";
        Check(repeat.Read(firstValue+"a00c000000000000000000000000a903414243"+Position+value+"00"),repeat.error);
        Check(State(repeat.object)==State(normal.object),"unknown field skip and repeated scalar final assignment");

        auto clonedBase=normal.object.Clone();
        const auto* clone=dynamic_cast<const std::remove_reference_t<decltype(normal.object)>*>(clonedBase.get());
        Check(clone&&State(*clone)==defaults,"actual clone leaves geometric constructor defaults");

        auto position=spBoundingVolume::Vector3{7,8,9};
        auto rotation=spBoundingVolume::Matrix3{0,1,0,-1,0,0,0,0,1};
        const auto beforeRotation=rotation;const auto scale=spBoundingVolume::Vector3{2,3,4};
        const auto state=State(normal.object);
        normal.object.UpdateCollisionTransformForAnalysis(position,rotation,scale);
        Check(position==spBoundingVolume::Vector3{9,9,12},"original shared4723B0 rotated local offset");
        Check(rotation==beforeRotation&&scale==spBoundingVolume::Vector3{2,3,4},"simple BV transform keeps rotation and scale");
        Check(State(normal.object)==state,"simple BV transform changes supplied PR rather than authored shape");
    }
    void Fields()
    {
        Sphere sphere;Box box;
        Check(sphere.object.IsExactly(0x390946D2)&&sphere.object.IsKindOf(0x21CC76AF),"actual sphere RTTI identity/base");
        Check(box.object.IsExactly(0x7B4C0876)&&box.object.IsKindOf(0x21CC76AF),"actual box RTTI identity/base");
        Check(sphere.Read("00")&&sphere.object.GetRadiusForAnalysis()==0&&sphere.object.GetBoundingRadiusForAnalysis()==0,"PC sphere zero defaults");
        Check(box.Read("00")&&box.object.GetSizeForAnalysis()==spBoundingVolume::Vector3{1,1,1}
            &&box.object.GetBoundingRadiusForAnalysis()==0,"PC box ctor does not derive radius");
        Check(sphere.Write()=="610000000000","default sphere writer always writes radius");
        Check(box.Write()=="a10c0000803f0000803f0000803f00","default box writer always writes full size");
        Common<Sphere>(SphereValue,Position+"610000e8c000");
        Common<Box>(BoxValue,Position+BoxValue+"00");

        Box rounding;Check(rounding.Read("a10ccdcccc3dcdcccc3dcdcc8c3f00"),rounding.error);
        Check(Bits(rounding.object.GetBoundingRadiusForAnalysis())=="79f50d3f","original Box radius requires wide intermediates");
        const auto derived=spBoxBVSerializer::DecodeSizeForAnalysis({2,-4,4});
        Check(derived.halfExtents==spBoxBVSerializer::Vector3{1,-2,2}&&derived.boundingSphereRadius==3,"signed full sizes preserve signed half extents");

        Sphere vase;Check(vase.Read("61011ab84200"),vase.error);
        Check(Bits(vase.object.GetRadiusForAnalysis())=="011ab842"&&vase.Write()=="61011ab84200","actual vase radius wire bytes");
        Box flower;Check(flower.Read("a10c2c6f4b43b8300844ca1b5b4300"),flower.error);
        Check(Bits(flower.object.GetBoundingRadiusForAnalysis())=="ad5a9b43","original blooming_flower radius");
        Box qc;Check(qc.Read("a10c3abb8e42acb00b43a193964200"),qc.error);
        Check(Bits(qc.object.GetBoundingRadiusForAnalysis())=="10ffad42","original QuietusCarnivorous radius");
    }
    void RawAndFailure()
    {
        const std::string rawPosition="a00c000000802143c57f000080ff";
        Sphere sphere;Check(sphere.Read(rawPosition+"614523c17f00"),sphere.error);
        Check(Bits(sphere.object.GetPositionForAnalysis())=="000000802143c57f000080ff","sphere raw position keeps negative zero and NaN/Inf bits");
        Check(Bits(sphere.object.GetRadiusForAnalysis())=="4523c17f"
            &&Bits(sphere.object.GetBoundingRadiusForAnalysis())=="4523c17f","sphere reader copies raw NaN to source and base radius");
        Box box;Check(box.Read(rawPosition+"a10c000000804523c17f0000807f00"),box.error);
        Check(Bits(box.object.GetSizeForAnalysis())=="000000804523c17f0000807f","box raw size preserves negative zero and NaN/Inf bits");
        Check(Bits(box.object.GetBoundingRadiusForAnalysis())=="4523c17f","original derived NaN radius in the captured raw case");

        Sphere failedSphere;
        Check(!failedSphere.Read(Position+"a104")&&failedSphere.context.failed,"missing sphere radius is rejected");
        Check(failedSphere.object.GetPositionForAnalysis()==spBoundingVolume::Vector3{1,-2,3}
            &&failedSphere.object.GetRadiusForAnalysis()==0,"failed radius read keeps prior position and previous radius");
        Box failedBox;
        Check(!failedBox.Read(Position+"a10c")&&failedBox.context.failed,"missing box size is rejected");
        Check(failedBox.object.GetPositionForAnalysis()==spBoundingVolume::Vector3{1,-2,3}
            &&failedBox.object.GetSizeForAnalysis()==spBoundingVolume::Vector3{1,1,1}
            &&failedBox.object.GetBoundingRadiusForAnalysis()==0,"failed size read keeps prior position and previous size/bounds");

        Sphere shortScalar;Open(shortScalar.stream,Parse("4523c1"));
        Check(!spSphereBVSerializer::ReadScalarFieldForAnalysis(1,shortScalar.stream,shortScalar.object)
            &&shortScalar.object.GetRadiusForAnalysis()==0,"short scalar helper does not commit a partial radius");
        Box unknownScalar;Open(unknownScalar.stream,Parse("00000040000080c000008040"));
        Check(!spBoxBVSerializer::ReadScalarFieldForAnalysis(9,unknownScalar.stream,unknownScalar.object),"unknown scalar helper does not pretend to read a field");
        std::uint32_t offset=99;Check(unknownScalar.stream.GetCurrentPosition(offset)&&offset==0,"unknown scalar helper consumes no bytes");
        Check(spBoxBVSerializer::ReadScalarFieldForAnalysis(1,unknownScalar.stream,unknownScalar.object)
            &&unknownScalar.object.GetBoundingRadiusForAnalysis()==3,"field-only API invokes the common actual shape update");

        Sphere envelope;Check(!envelope.Read("a00301020300"),"host rejects a known field with the wrong declared extent");
        Sphere trailing;Check(!trailing.Read("0000"),"host exact-section guard rejects trailing data");
        Sphere mismatch;Open(mismatch.stream,Parse("00"));spBoxBV wrong;
        Check(!mismatch.serializer.ReadPayloadForAnalysis(mismatch.context,mismatch.stream,1,wrong,&mismatch.error),"actual target type mismatch rejected");
    }
    template<class Object,class Serializer>
    std::string Capture(const std::string& hex,const std::string& mode)
    {
        Fixture<Object,Serializer> f;
        const auto defaults=State(f.object);const bool read=f.Read(hex);
        std::uint32_t position=0;Check(f.stream.GetCurrentPosition(position),"capture cursor");
        const auto decoded=State(f.object);
        std::ostringstream out;
        out<<"{\"defaults\":"<<defaults<<",\"readResult\":"<<read<<",\"readPosition\":"<<position<<",\"decoded\":"<<decoded;
        if(read&&mode!="raw-bits")out<<",\"written\":\""<<f.Write()<<'"';
        if(read&&mode=="clone")
        {
            const auto cloned=f.object.Clone();const auto* object=dynamic_cast<const Object*>(cloned.get());
            Check(object!=nullptr,"typed clone capture");out<<",\"clone\":"<<State(*object);
        }
        if(read&&mode=="transform")
        {
            auto p=spBoundingVolume::Vector3{7,8,9};auto r=spBoundingVolume::Matrix3{0,1,0,-1,0,0,0,0,1};
            const auto s=spBoundingVolume::Vector3{2,3,4};f.object.UpdateCollisionTransformForAnalysis(p,r,s);
            out<<",\"transform\":{\"position\":["<<p[0]<<','<<p[1]<<','<<p[2]<<"],\"rotation\":[";
            for(std::size_t i=0;i<r.size();++i){if(i)out<<',';out<<r[i];}
            out<<"],\"scale\":[2,3,4],\"objectAfter\":"<<State(f.object)<<'}';
        }
        out<<'}';return out.str();
    }
}
int main(int argc,char** argv)
{
    try
    {
        if((argc==4||argc==5)&&std::string(argv[1])=="--capture")
        {
            const std::string mode=argc==5?argv[4]:"values";
            if(std::string(argv[2])=="sphere")std::cout<<Capture<spSphereBV,spSphereBVSerializer>(argv[3],mode)<<'\n';
            else if(std::string(argv[2])=="box")std::cout<<Capture<spBoxBV,spBoxBVSerializer>(argv[3],mode)<<'\n';
            else throw std::runtime_error("capture kind must be sphere or box");
            return 0;
        }
        Check(argc==1,"usage: --capture sphere|box HEX [mode]");
        Fields();RawAndFailure();
        std::cout<<"PASS "<<checks<<" checks: original PC simple BV readers\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

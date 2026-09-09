// Golden states: fresh actual PC OBB reader captures, 2026-09-10 cycle.
#include "Code/Sparkplug/spOBBBV.h"
#include "Code/Sparkplug/spOBBBVSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spSectionCursor.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;
    int checks=0;
    void Check(bool value,const std::string& message)
    {++checks;if(!value)throw std::runtime_error(message);}
    Bytes Parse(const std::string& text)
    {
        Check(text.size()<=8192&&text.size()%2==0,"bounded capture hex");
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
    std::uint32_t Bits(float value)
    {std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));return bits;}
    std::string WordArray(const float* values,std::size_t count)
    {
        std::ostringstream out;out<<'[';
        for(std::size_t i=0;i<count;++i)
        {if(i)out<<',';out<<'"'<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(8)<<Bits(values[i])<<'"';}
        out<<']';return out.str();
    }
    template<std::size_t N>std::string WordArray(const std::array<float,N>& values)
    {return WordArray(values.data(),N);}
    struct Fixture
    {
        spOBBBV object;
        spOBBBVSerializer serializer;
        spSerializerManager manager;
        spResourceManager resources;
        spSerializerReadContextForAnalysis context{manager,resources};
        spMemoryStream stream;
        std::array<float,4> observedQuaternion{0,0,0,1};
        std::string error;
        bool Read(const std::string& hex,bool scalar)
        {
            const auto bytes=Parse(hex);
            Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"capture stream allocation");
            if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());
            Check(stream.Seek(spStream::SeekSource::essStart,0),"capture stream rewind");
            if(!scalar)return serializer.ReadPayloadForAnalysis(context,stream,
                static_cast<std::uint32_t>(bytes.size()),object,&error);
            // Inspection harness: the shared block reader selects bounded
            // fields, and the actual serializer performs every scalar effect.
            sparkplug::evidence::pc::serialization::SectionCursor cursor(context,stream,
                static_cast<std::uint32_t>(bytes.size()),true,&error);
            while(const auto* header=cursor.Next())
            {
                if(header->IsTerminator())return true;
                if(!serializer.IsKnownReadFieldForAnalysis(header->fieldID))
                {if(!cursor.Skip())return false;continue;}
                std::array<std::uint8_t,16> payload{};
                if(header->payloadSize>payload.size()||!stream.ReadData(payload.data(),header->payloadSize))return false;
                if(!serializer.ReadScalarFieldForAnalysis(object,header->fieldID,payload.data(),
                    header->payloadSize,&observedQuaternion,&error))return false;
            }
            return false;
        }
        std::string State() const
        {
            const auto& center=object.GetBoundingCenterForAnalysis();
            const std::array<float,4> sphere{center[0],center[1],center[2],object.GetBoundingRadiusForAnalysis()};
            return "{\"positionBits\":"+WordArray(object.GetPositionForAnalysis())
                +",\"sizeBits\":"+WordArray(object.GetSizeForAnalysis())
                +",\"orientationBits\":"+WordArray(object.GetOrientationForAnalysis())
                +",\"sphereBits\":"+WordArray(sphere)+"}";
        }
    };
    void Regression()
    {
        Fixture defaults;Check(defaults.Read("00",true),defaults.error);
        Check(defaults.object.GetSizeForAnalysis()==spOBBBV::Vector3{1,1,1}
            &&defaults.object.GetBoundingRadiusForAnalysis()==0,"actual ctor leaves zero bounding radius");
        const std::array<std::pair<const char*,std::uint32_t>,5> cases{{
            {"a10c00000040000080400000804000",0x40400000},
            {"a10ccdcccc3dcdcccc3dcdcc8c3f00",0x3F0DF579},
            {"a10c000000c000008040000080c000",0x40400000},
            {"a10c01000000020000000300000000",0x00000002},
            {"a10ccaf24971caf2c9711776177200",0x71BCE7CA},
        }};
        for(const auto& sample:cases)
        {
            Fixture scalar,whole;Check(scalar.Read(sample.first,true),scalar.error);
            Check(whole.Read(sample.first,false),whole.error);
            Check(Bits(scalar.object.GetBoundingRadiusForAnalysis())==sample.second,"original radius bits");
            Check(scalar.State()==whole.State(),"actual scalar and whole reader states agree");
        }
        Fixture quaternion;
        Check(quaternion.Read("a21000000000000000000000003f0000003f00",true),quaternion.error);
        Check(quaternion.observedQuaternion==std::array<float,4>{0,0,.5f,.5f},"observed authored quaternion remains unnormalized");
        Check(quaternion.object.GetOrientationForAnalysis()==spOBBBV::Matrix3{.5f,.5f,0,-.5f,.5f,0,0,0,1},"original nonunit quaternion matrix");
        Fixture rawPosition;
        Check(rawPosition.Read("a00c000000804523c17f0000807f00",true),rawPosition.error);
        Check(WordArray(rawPosition.object.GetPositionForAnalysis())=="[\"80000000\",\"7FC12345\",\"7F800000\"]","inspection preserves raw position bits");
        Check(rawPosition.object.GetPositionForAnalysis().data()!=rawPosition.object.GetBoundingCenterForAnalysis().data()
            &&WordArray(rawPosition.object.GetBoundingCenterForAnalysis())==WordArray(rawPosition.object.GetPositionForAnalysis()),"position writes the separate base mirror");
        Fixture rawQuaternion;
        Check(rawQuaternion.Read("a210000000804523c17f0000807f000080ff00",true),rawQuaternion.error);
        Check(WordArray(rawQuaternion.observedQuaternion)=="[\"80000000\",\"7FC12345\",\"7F800000\",\"FF800000\"]","raw quaternion is observed before matrix conversion");
        Fixture rejectPosition,rejectQuaternion;
        Check(!rejectPosition.Read("a00c000000804523c17f0000807f00",false)&&rejectPosition.context.failed,"whole reader retains explicit finite position guard");
        Check(!rejectQuaternion.Read("a210000000804523c17f0000807f000080ff00",false)&&rejectQuaternion.context.failed,"whole reader retains explicit finite quaternion guard");
        spOBBBV object;const std::array<float,3> value{1,2,3};std::string error;
        Check(!spOBBBVSerializer::ReadScalarFieldForAnalysis(object,0,value.data(),11,nullptr,&error),"scalar exact vector extent guard");
        Check(!spOBBBVSerializer::ReadScalarFieldForAnalysis(object,2,value.data(),12,nullptr,&error),"scalar exact quaternion extent guard");
        Check(!spOBBBVSerializer::ReadScalarFieldForAnalysis(object,3,value.data(),12,nullptr,&error),"unknown field is not an OBB scalar");
        Check(!spOBBBVSerializer::ReadScalarFieldForAnalysis(object,0,nullptr,12,nullptr,&error),"scalar null payload guard");
        Check(object.GetPositionForAnalysis()==spOBBBV::Vector3{},"rejected fields leave object untouched");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==3&&std::string(argv[1])=="--capture")
        {
            Fixture fixture;const auto defaults=fixture.State();Check(fixture.Read(argv[2],true),fixture.error);
            std::cout<<"{\"status\":\"passed\",\"defaults\":"<<defaults<<",\"decoded\":"<<fixture.State()
                <<",\"observedQuaternionBits\":"<<WordArray(fixture.observedQuaternion)<<"}\n";return 0;
        }
        Check(argc==1,"usage: spOBBScalarTests [--capture HEX_SECTION]");
        Regression();std::cout<<"PASS "<<checks<<" OBB scalar checks\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}
}

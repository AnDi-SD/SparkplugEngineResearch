#include "Code/Sparkplug/spTextureData.h"
#include "Code/Sparkplug/spTextureDataSerializer.h"
#include "Code/Sparkplug/spDXTextureDataSerializer.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkplugDX/spDXTexture.h"
#include "Code/SparkplugDX/spDXTextureSerializer.h"
#include "Code/SparkBase/spMemoryStream.h"
#include "Analysis/PC/spTextureMipFilter.h"
#include "Analysis/PC/spTextureBlockCodec.h"
#include "Analysis/PC/spTextureBlockOptimizer.h"
#include "Analysis/PC/spTextureBlockEncoder.h"
#include "Analysis/PC/spTextureCompressedMipFilter.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sparkplug::reconstruction;
namespace
{
    using Bytes=std::vector<std::uint8_t>;int checks=0;
    void Check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
    template<class T>void Add(Bytes& out,const T& value){const auto* p=reinterpret_cast<const std::uint8_t*>(&value);out.insert(out.end(),p,p+sizeof(value));}
    void Field(Bytes& out,std::uint8_t id,const Bytes& bytes){Check(bytes.size()<256&&!bytes.empty(),"tiny field");out.push_back(0xa0+id);out.push_back(static_cast<std::uint8_t>(bytes.size()));out.insert(out.end(),bytes.begin(),bytes.end());}
    template<class T>void Field(Bytes& out,std::uint8_t id,const T& value){Bytes bytes;Add(bytes,value);Field(out,id,bytes);}
    void Open(spMemoryStream& stream,const Bytes& bytes={}){Check(stream.ResizeAndSetSize(static_cast<std::uint32_t>(bytes.size())),"capacity");if(!bytes.empty())std::memcpy(stream.GetBuffer(),bytes.data(),bytes.size());Check(stream.Seek(spStream::SeekSource::essStart,0),"rewind");}
    Bytes Data(spMemoryStream& stream){std::uint32_t size=0;Check(stream.GetSize(&size),"stream size");const auto* p=static_cast<const std::uint8_t*>(stream.GetBuffer());return size?Bytes(p,p+size):Bytes{};}
    std::string Hex(const Bytes& bytes){constexpr char digits[]="0123456789abcdef";std::string result;for(auto b:bytes){result+=digits[b>>4];result+=digits[b&15];}return result;}
    Bytes Input(std::uint32_t format,std::uint32_t pixelSize,bool wrapperEnd=true,std::uint32_t width=2)
    {
        Bytes raw;Add(raw,width);Add(raw,1u);Add(raw,format);Add(raw,pixelSize);
        for(std::uint32_t i=1;i<=2*pixelSize;++i)raw.push_back(static_cast<std::uint8_t>(i));
        Bytes nested;Field(nested,5,raw);nested.push_back(0);Bytes data;Field(data,2,std::uint8_t(0));if(wrapperEnd)data.push_back(0);
        Field(data,6,1u);Field(data,0,nested);data.push_back(0);return data;
    }
    std::string Cross(const std::string& mode)
    {
        Check(mode=="rgba"||mode=="gray"||mode=="rgb16","explicit opaque-byte pixel fixture, not a channel-order claim");
        const auto format=mode=="rgba"?0u:mode=="gray"?2u:3u;const auto pixelSize=spTextureBuffer::PixelSizeForFormatForAnalysis(format);
        const auto data=Input(format,pixelSize);spMemoryStream input;Open(input,data);spSerializerManager manager;spResourceManager resources;
        spSerializerReadContextForAnalysis context(manager,resources);spTextureData texture;spTextureDataSerializer serializer;std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(data.size()),texture,&error),error.c_str());
        std::uint32_t position=0;Check(input.GetCurrentPosition(position)&&position==data.size(),"both source and local section terminators consumed");
        const auto& buffer=texture.GetTextureBufferForAnalysis();Bytes pixels;for(auto b:buffer.GetBufferForAnalysis())pixels.push_back(std::uint8_t(b));
        Check(buffer.GetWidthForAnalysis()==2&&buffer.GetHeightForAnalysis()==1&&buffer.GetDepthForAnalysis()==1&&buffer.GetPixelFormatForAnalysis()==format&&buffer.GetPixelSizeForAnalysis()==pixelSize,"actual CPU texture state");
        Check(texture.IsInitializedForAnalysis()&&texture.GetField1CForAnalysis()==1&&texture.GetTextureFlagsForAnalysis()==0,"actual cross reader Init buffer arguments");
        Check(serializer.IndexRelationshipsWithContextForAnalysis(manager,texture),"no resource graph for CPU texture with absent source");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,texture,&error),error.c_str());
        const auto written=Data(output);spMemoryStream repeat;Open(repeat,written);spTextureData decoded;
        Check(serializer.ReadPayloadForAnalysis(context,repeat,static_cast<std::uint32_t>(written.size()),decoded,&error),error.c_str());
        Check(decoded.GetTextureBufferForAnalysis().GetBufferForAnalysis()==buffer.GetBufferForAnalysis(),"writer roundtrip preserves opaque pixel bytes");
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(data)<<"\",[2,1,1,"<<format<<','<<pixelSize<<"],\""<<Hex(pixels)<<"\",\""<<Hex(written)<<"\"]";return row.str();
    }
    void Bounds()
    {
        for(int mode=0;mode<6;++mode)
        {
            auto data=Input(mode==2?9u:0u,mode==1?1u:4u,mode!=0,mode==3?65536u:2u);
            if(mode==4)data.pop_back();if(mode==5)data.insert(data.begin(),{0xa3,1,0});
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spTextureData texture;spMemoryStream stream;Open(stream,data);std::string error;
            Check(!spTextureDataSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(data.size()),texture,&error)&&context.failed&&!error.empty(),"strict malformed/unrestored texture rejected");
        }
        spTextureData empty;spMemoryStream stream;Open(stream);std::string error;
        Check(!spTextureDataSerializer{}.WritePayloadForAnalysis(stream,empty,&error)&&Data(stream).empty(),"empty CPU texture is not invented output");
        auto bytes=Input(0,4);Open(stream,bytes);spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        Check(!spDXTextureDataSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(bytes.size()),empty,&error),"unrestored DX native codec does not silently inherit CPU-only codec");
    }
    Bytes RuntimeInput(std::uint32_t format,std::uint32_t size)
    {
        Bytes data;Add(data,size);Add(data,size);Add(data,format);Add(data,std::uint8_t(0));
        Add(data,spDXTexture::FullMipCountForAnalysis(size,size));auto dimension=size;
        while(true)
        {
            spDXTexture::MipForAnalysis layout;Check(spDXTexture::DescribeMipForAnalysis(dimension,dimension,format,layout),"tiny runtime mip layout");
            for(std::uint32_t i=1;i<=layout.rowBytes*layout.rows;++i)data.push_back(static_cast<std::uint8_t>(i));
            if(dimension==1)break;dimension>>=1;
        }
        return data;
    }
    std::string Runtime(const std::string& mode)
    {
        const auto separator=mode.find('-');const auto format=static_cast<std::uint32_t>(std::stoul(mode));
        const auto size=separator==std::string::npos?1u:static_cast<std::uint32_t>(std::stoul(mode.substr(separator+1)));
        Check(format<8&&(size==1||size==2||size==4),"explicit runtime format/full chain fixture");
        const auto data=RuntimeInput(format,size);spMemoryStream input;Open(input,data);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t packed) noexcept{return packed+4;};
        spDXTexture texture;spDXTextureSerializer serializer;std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(data.size()),texture,&error),error.c_str());
        Check(texture.HasInitializedRuntimeFormatForAnalysis()&&texture.GetRuntimeFormatForAnalysis()==format,"actual runtime format state");
        Check(texture.GetWidthForAnalysis()==size&&texture.GetHeightForAnalysis()==size,"runtime attach does not normalize dimensions");
        Check(texture.GetMipsForAnalysis().size()==spDXTexture::FullMipCountForAnalysis(size,size),"complete mip count");
        Check(texture.GetField1CForAnalysis()==0&&texture.GetTextureFlagsForAnalysis()==0&&!texture.GetField31ForAnalysis(),"runtime attachment preserves base init mode fields");
        Check(!texture.HasLiveGraphicsBackendForAnalysis(),"CPU shadow is not a claimed live GPU backend");
        Check(serializer.IndexRelationshipsWithContextForAnalysis(manager,texture),"runtime palette-free texture has no reference graph");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadWithContextForAnalysis(manager,output,texture,&error),error.c_str());
        const auto written=Data(output);Check(written==data,"runtime texture writer excludes physical pitch padding");
        spDXTexture decoded;Open(input,written);Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(written.size()),decoded,&error),error.c_str());
        Check(decoded.GetNativeByteCountForAnalysis()==texture.GetNativeByteCountForAnalysis(),"native byte count uses physical pitch only for uncompressed surfaces");
        std::ostringstream row;row<<'[';if(size==1)row<<format;else row<<'"'<<mode<<'"';
        row<<",\""<<Hex(data)<<"\",["<<size<<','<<size<<','<<format<<','<<texture.GetNativeByteCountForAnalysis()<<"],\""<<Hex(written)<<"\"]";return row.str();
    }
    Bytes NativeInput(std::uint32_t flags,std::uint32_t size,bool embedded=false)
    {
        Bytes native;auto dimension=size;bool first=true;
        while(true)
        {
            spDXTexture::MipForAnalysis mip;Check(spDXTexture::DescribeMipForAnalysis(dimension,dimension,flags?flags-1:3u,mip),"native full mip layout");
            Bytes raw;
            if(first){Add(raw,std::uint8_t(1));Add(raw,size);Add(raw,size);Add(raw,flags);Add(raw,std::uint8_t(1));}
            Add(raw,dimension);Add(raw,mip.rowBytes);Add(raw,mip.rows);
            for(std::uint32_t i=1;i<=mip.rowBytes*mip.rows;++i)raw.push_back(static_cast<std::uint8_t>(i));
            Field(native,first?0:1,raw);first=false;if(dimension==1)break;dimension>>=1;
        }
        native.push_back(0);Bytes data;Field(data,2,std::uint8_t(0));data.push_back(0);Field(data,6,6u);Field(data,1,native);data.push_back(0);
        if(embedded){Bytes wrapper;Field(wrapper,3,data);wrapper.push_back(0);data=std::move(wrapper);}return data;
    }
    std::string Native(const std::string& mode)
    {
        const auto separator=mode.find('-');const auto kind=mode.substr(0,separator);
        const auto size=separator==std::string::npos?1u:static_cast<std::uint32_t>(std::stoul(mode.substr(separator+1)));
        const auto flags=kind=="raw"?0u:kind=="dxt1"?1u:kind=="dxt3"?2u:3u;
        Check((kind=="raw"||kind=="dxt1"||kind=="dxt3"||kind=="dxt5")&&(size==1||size==2||size==4),"explicit native mip case");
        const auto data=NativeInput(flags,size,mode.find("embedded")!=std::string::npos);spMemoryStream input;Open(input,data);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t row) noexcept{return row+4;};
        spDXTexture texture;spDXTextureDataSerializer serializer;std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(data.size()),texture,&error),error.c_str());
        Check(context.depth==0&&!context.failed,"embedded source depth restored");
        Check(texture.GetWidthForAnalysis()==size&&texture.GetHeightForAnalysis()==size,"native data preserves exact dimensions");
        Check(texture.GetField18ForAnalysis()==texture.GetMipsForAnalysis().size()&&texture.GetField1CForAnalysis()==1
            &&texture.GetTextureFlagsForAnalysis()==flags&&texture.IsInitializedForAnalysis(),"native data copies proven base state");
        Check(!texture.HasInitializedRuntimeFormatForAnalysis()&&texture.GetNativeByteCountForAnalysis()==0,"native data does not initialize runtime44/48");
        Check(texture.GetSurfaceFormatForAnalysis()==(flags?flags-1:3u),"separate known surface descriptor is not native runtime44");
        spMemoryStream output;Open(output);
        Check(!spDXTextureSerializer{}.WritePayloadForAnalysis(output,texture,&error)&&Data(output).empty(),"runtime writer refuses unset44 after native data import");
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(data)<<"\",[";bool first=true;
        for(const auto& mip:texture.GetMipsForAnalysis())
        {
            if(!first)row<<',';first=false;Bytes pixels;for(auto b:mip.packedBytes)pixels.push_back(std::uint8_t(b));row<<'"'<<Hex(pixels)<<'"';
        }
        row<<"],["<<texture.GetField18ForAnalysis()<<",1,"<<flags<<",1,"<<size<<','<<size<<"]]";return row.str();
    }
    void NativeBounds()
    {
        for(int mode=0;mode<5;++mode)
        {
            auto data=NativeInput(0,1);if(mode==0)data.pop_back();if(mode==1)data[6]=1; // platform6 payload is offsets6..9; disables native branch
            if(mode==2)data.insert(data.begin(),{0xa4,1,0});
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            if(mode==3)context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t) noexcept{return 0u;};
            if(mode==4){data=NativeInput(0,1,true);context.depth=64;}
            spMemoryStream input;Open(input,data);spDXTexture texture;std::string error;
            Check(!spDXTextureDataSerializer{}.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(data.size()),texture,&error)&&context.failed,"malformed/unrestored native texture rejected");
        }
    }
    void EncodeBlocks()
    {
        unsigned flags=0,count=0;while(std::cin>>flags)
        {
            Check(++count<=128,"bounded encoder batch");sparkplug::evidence::pc::texture_blocks::Block block{};
            for(auto& color:block)for(float& value:color)
            {std::uint32_t bits=0;Check(bool(std::cin>>bits),"complete encoder block");std::memcpy(&value,&bits,4);}
            std::vector<std::byte> packed;Check(sparkplug::evidence::pc::texture_blocks::EncodeNoDither(flags,block,packed),"restored block encoder input");
            Bytes bytes;for(auto b:packed)bytes.push_back(static_cast<std::uint8_t>(b));std::cout<<'"'<<Hex(bytes)<<"\"\n";
        }
        Check(std::cin.eof()&&count,"complete encoder input");
    }
    void OptimizeBlocks(bool capture=false)
    {
        unsigned steps=0,count=0;
        while(std::cin>>steps)
        {
            Check(++count<=128,"bounded optimizer batch");sparkplug::evidence::pc::texture_blocks::Block block{};
            for(auto& color:block)for(unsigned c=0;c<3;++c)
            {std::uint32_t bits=0;Check(bool(std::cin>>bits),"complete optimizer block");std::memcpy(&color[c],&bits,4);}
            sparkplug::evidence::pc::texture_blocks::RGB a,b;
            std::vector<sparkplug::evidence::pc::texture_blocks::OptimizerStep> trace;
            Check(sparkplug::evidence::pc::texture_blocks::OptimizeRGB(block,steps,a,b,capture?&trace:nullptr),"bounded weighted block");
            if(capture)std::cout<<"{\"result\":";
            std::cout<<'[';bool first=true;for(const auto& endpoint:{a,b})for(float value:endpoint)
            {if(!first)std::cout<<',';first=false;std::uint32_t bits;std::memcpy(&bits,&value,4);std::cout<<bits;}
            std::cout<<']';if(capture)
            {
                std::cout<<",\"trace\":[";bool rowFirst=true;
                for(const auto& row:trace){if(!rowFirst)std::cout<<',';rowFirst=false;std::cout<<'[';bool valueFirst=true;
                    for(float value:row){if(!valueFirst)std::cout<<',';valueFirst=false;std::uint32_t bits;std::memcpy(&bits,&value,4);std::cout<<bits;}std::cout<<']';}
                std::cout<<"]}";
            }
            std::cout<<'\n';
        }
        Check(std::cin.eof()&&count,"complete optimizer input");
    }
    void DecodeBlocks()
    {
        unsigned flags=0,count=0;std::string hex;
        while(std::cin>>flags>>hex)
        {
            Check(++count<=1024&&hex.size()<=32&&hex.size()%2==0,"bounded block decoder input");
            const auto nibble=[](char c)->unsigned
            {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("lowercase block hex");};
            std::vector<std::byte> bytes;for(std::size_t i=0;i<hex.size();i+=2)bytes.push_back(static_cast<std::byte>(nibble(hex[i])*16+nibble(hex[i+1])));
            sparkplug::evidence::pc::texture_blocks::Block block;
            Check(sparkplug::evidence::pc::texture_blocks::Decode(flags,bytes.data(),bytes.size(),block),"valid native block shape");
            std::cout<<'[';bool first=true;for(const auto& color:block)for(float value:color)
            {if(!first)std::cout<<',';first=false;std::uint32_t bits;std::memcpy(&bits,&value,4);std::cout<<bits;}
            std::cout<<"]\n";
        }
        Check(std::cin.eof()&&count,"complete nonempty block batch");
    }
    void BlockDecodeGuards()
    {
        namespace codec=sparkplug::evidence::pc::texture_blocks;
        std::array<std::byte,16> bytes;bytes.fill(std::byte{255});bytes[8]=bytes[9]=std::byte{0};
        codec::Block block;Check(codec::Decode(2,bytes.data(),bytes.size(),block),"DXT3 explicit alpha with DXT1 color branch");
        Check(std::all_of(block.begin(),block.end(),[](const auto& c){return c==codec::Color{0,0,0,1};}),"native DXT3 index3 is black when first endpoint is lower");
        const auto previous=block;
        Check(!codec::Decode(0,bytes.data(),bytes.size(),block)&&block==previous,"invalid codec preserves output");
        Check(!codec::Decode(1,bytes.data(),bytes.size(),block)&&block==previous,"invalid packed extent preserves output");
    }
    void BlockOptimizerGuards()
    {
        namespace codec=sparkplug::evidence::pc::texture_blocks;
        const float updated=sparkplug::evidence::pc::float80_rtz::UpdateEndpoint(0,codec::Constant(0xBCCE6730),.75F);
        std::uint32_t bits;std::memcpy(&bits,&updated,4);
        Check(bits==0x3D099A1F,"native extended reciprocal residual survives until truncate float store");
        codec::Block block{};codec::RGB a{1,2,3},b{4,5,6};
        Check(!codec::OptimizeRGB(block,2,a,b)&&a==codec::RGB{1,2,3}&&b==codec::RGB{4,5,6},"unknown palette size preserves outputs");
        block[0][0]=codec::Constant(0x7FC00000);
        Check(!codec::OptimizeRGB(block,4,a,b)&&a==codec::RGB{1,2,3},"nonfinite optimizer input rejected");
    }
    void CompressedMipGuards()
    {
        namespace filter=sparkplug::evidence::pc::texture_mips;
        spDXTexture::MipForAnalysis input,output;Check(spDXTexture::DescribeMipForAnalysis(4,4,0,input),"compressed source layout");
        for(unsigned byte:{0u,248u,31u,0u,0u,0u,0u,0u})input.packedBytes.push_back(std::byte(byte));
        const std::vector<std::byte> red{std::byte{0},std::byte{248},std::byte{0},std::byte{248},std::byte{0},std::byte{0},std::byte{0},std::byte{0}};
        Check(filter::GenerateNextCompressed(input,1,output)&&output.width==2&&output.height==2&&output.packedBytes==red,"original canonical red DXT1 generated block");
        auto next=output;Check(filter::GenerateNextCompressed(output,1,next)&&next.width==1&&next.packedBytes==red,"partial block wrap fills final1x1");
        input.packedBytes.pop_back();Check(!filter::GenerateNextCompressed(input,1,next)&&next.packedBytes==red,"truncated compressed input preserves destination");
        sparkplug::evidence::pc::texture_blocks::Block block{};auto bytes=red;
        Check(!sparkplug::evidence::pc::texture_blocks::EncodeNoDither(4,block,bytes)&&bytes==red,"unknown block format does not mutate output");
    }
    std::string MissingMips()
    {
        std::string hex;Check(bool(std::cin>>hex)&&hex.size()<=4096&&hex.size()%2==0,"bounded native payload hex");
        const auto nibble=[](char c)->unsigned
        {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("lowercase hex input");};
        Bytes data;for(std::size_t i=0;i<hex.size();i+=2)data.push_back(static_cast<std::uint8_t>(nibble(hex[i])*16+nibble(hex[i+1])));
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t row) noexcept{return row+4;};
        spMemoryStream stream;Open(stream,data);spDXTexture texture;std::string error;
        Check(spDXTextureDataSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(data.size()),texture,&error),error.c_str());
        std::uint32_t cursor=0;Check(stream.GetCurrentPosition(cursor)&&cursor==data.size()&&!context.failed&&!context.depth,"complete missing mip payload consumed");
        Check(!texture.HasInitializedRuntimeFormatForAnalysis()&&!texture.GetNativeByteCountForAnalysis(),"missing mip path leaves runtime44/48 untouched");
        std::ostringstream out;out<<"{\"state\":["<<texture.GetField18ForAnalysis()<<','<<unsigned(texture.GetField1CForAnalysis())<<','
            <<texture.GetTextureFlagsForAnalysis()<<','<<texture.IsInitializedForAnalysis()<<','<<texture.GetWidthForAnalysis()<<','<<texture.GetHeightForAnalysis()<<"],\"levels\":[";
        bool first=true;for(const auto& mip:texture.GetMipsForAnalysis())
        {
            Check(mip.physicalPitch==mip.rowBytes+4,"generated surfaces use declared pitch callback");
            if(!first)out<<',';first=false;Bytes pixels;for(auto b:mip.packedBytes)pixels.push_back(std::uint8_t(b));
            out<<"{\"width\":"<<mip.width<<",\"height\":"<<mip.height<<",\"packedHex\":\""<<Hex(pixels)<<"\"}";
        }
        out<<"]}";return out.str();
    }
    void MissingMipGuards()
    {
        namespace filter=sparkplug::evidence::pc::texture_mips;
        spDXTexture::MipForAnalysis input,output;
        Check(spDXTexture::DescribeMipForAnalysis(2,2,3,input),"rounding input shape");
        for(unsigned blue:{134u,138u,125u,129u})for(unsigned c:{blue,0u,0u,255u})input.packedBytes.push_back(static_cast<std::byte>(c));
        Check(filter::GenerateNext(input,output)&&output.packedBytes==std::vector<std::byte>{std::byte{131},std::byte{0},std::byte{0},std::byte{255}},"native float normalization and truncate stores differ from integer mean at 131.5");
        const auto previous=output.packedBytes;input.packedBytes.pop_back();
        Check(!filter::GenerateNext(input,output)&&output.packedBytes==previous,"invalid mip extent rejected without output mutation");
    }
    std::string NativeWrite(const std::string& mode)
    {
        Check(mode=="raw"||mode=="raw-2"||mode=="dxt1-4"||mode=="raw-both"||mode=="raw-auto"||mode=="raw-4-partial","explicit native writer case");
        const auto flags=mode=="dxt1-4"?1u:0u;const auto size=mode=="raw-2"?2u:(mode=="dxt1-4"||mode=="raw-4-partial")?4u:1u;
        const auto policy=mode=="raw-both"?2u:mode=="raw-auto"?0u:1u;const bool partial=mode=="raw-4-partial";
        spTextureData texture;std::vector<spTextureData::NativeMipForAnalysis> mips;auto dimension=size;
        while(true)
        {
            spDXTexture::MipForAnalysis layout;Check(spDXTexture::DescribeMipForAnalysis(dimension,dimension,flags?0:3,layout),"native writer input layout");
            spTextureData::NativeMipForAnalysis mip;mip.width=dimension;mip.rowStride=layout.rowBytes;mip.rows=layout.rows;
            for(std::uint32_t i=1;i<=mip.rowStride*mip.rows;++i)mip.bytes.push_back(static_cast<std::byte>(i));
            mips.push_back(std::move(mip));if(dimension==1||partial)break;dimension>>=1;
        }
        Check(texture.SetNativeMipDataForAnalysis(size,size,flags,1,mips),"prepare explicit native CPU mip input");
        if(policy!=1)
        {
            auto& buffer=texture.GetTextureBufferForAnalysis();Check(buffer.InitializeForAnalysis(1,1,1,0),"separate CPU cross buffer");
            Check(buffer.SetDataForAnalysis({std::byte{1},std::byte{2},std::byte{3},std::byte{4}}),"CPU cross pixel input");
        }
        spSerializerManager manager;Check(manager.SetSerializationPolicyForAnalysis(policy),"explicit native serialization policy");
        Check(!manager.SetSerializationPolicyForAnalysis(3)&&manager.GetSerializationPolicyForAnalysis()==policy,"unsupported host policy does not change configured value");
        spMemoryStream stream;Open(stream);spDXTextureDataSerializer serializer;std::string error;
        Check(serializer.WritePayloadWithContextForAnalysis(manager,stream,texture,&error),error.c_str());const auto written=Data(stream);
        spMemoryStream input;Open(input,written);spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);spDXTexture decoded;
        const bool read=serializer.ReadPayloadForAnalysis(context,input,static_cast<std::uint32_t>(written.size()),decoded,&error);
        if(partial)Check(read&&!context.failed&&decoded.GetMipsForAnalysis().size()==3
            &&decoded.GetMipsForAnalysis()[0].packedBytes==mips[0].bytes,"partial raw writer chain generates missing levels and preserves supplied pixels");
        else
        {
            Check(read,error.c_str());Check(decoded.GetMipsForAnalysis().size()==mips.size(),"native writer output loads through same reconstructed runtime reader");
            for(std::size_t i=0;i<mips.size();++i)Check(decoded.GetMipsForAnalysis()[i].packedBytes==mips[i].bytes,"shared writer/reader exact native mip bytes");
        }
        auto clone=texture.Clone();auto* cloned=dynamic_cast<spTextureData*>(clone.get());
        Check(cloned&&cloned->GetNativeMipsForAnalysis().empty()&&!cloned->GetField68ForAnalysis(),"native CPUData clone remains blank mip container");
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(written)<<"\"]";return row.str();
    }
    void NativeWriterBounds()
    {
        spTextureData texture;spTextureData::NativeMipForAnalysis invalid;invalid.width=1;invalid.rowStride=4;invalid.rows=1;
        Check(!texture.SetNativeMipDataForAnalysis(1,1,0,1,{invalid}),"truncated native input rejected");
        invalid.bytes={std::byte{1},std::byte{2},std::byte{3},std::byte{4}};
        Check(!texture.SetNativeMipDataForAnalysis(1,1,0,1,{invalid,invalid}),"excess mip input rejected");
        Check(!texture.SetNativeMipDataForAnalysis(3,1,0,1,{invalid}),"host native input dimensions guarded");
        spMemoryStream stream;Open(stream);spSerializerManager manager;std::string error;spDXTextureDataSerializer serializer;
        Check(!serializer.WritePayloadWithContextForAnalysis(manager,stream,texture,&error)&&Data(stream).empty(),"empty native input yields no output");
        Check(texture.SetNativeMipDataForAnalysis(1,1,0,1,{invalid}),"valid raw native input");
        Check(!serializer.WritePayloadWithContextForAnalysis(manager,stream,texture,&error)&&Data(stream).empty(),"policy2 requires real cross buffer before any output");
        Check(manager.SetSerializationPolicyForAnalysis(1),"native-only policy");
        texture.SetField68ForAnalysis(false);
        Check(!serializer.WritePayloadWithContextForAnalysis(manager,stream,texture,&error)&&Data(stream).empty(),"unconverted native bytes not silently accepted");
        spDXTexture runtime;
        Check(!serializer.WritePayloadWithContextForAnalysis(manager,stream,runtime,&error)&&Data(stream).empty(),"CPUData writer never reinterprets runtime DX layout");
    }
    Bytes PaletteInput(std::uint32_t format)
    {
        auto data=RuntimeInput(format,1);data[12]=1;
        Bytes entries;for(unsigned i=0;i<1024;++i)entries.push_back(static_cast<std::uint8_t>(i));
        data.insert(data.begin()+13,entries.begin(),entries.end());
        if(format==5)data.back()=0x9a;return data;
    }
    std::string Palette(const std::string& mode)
    {
        Check(mode=="raw"||mode=="indexed"||mode=="indexed-upload-fail","explicit native palette case");
        const auto format=mode=="raw"?3u:5u;const auto data=PaletteInput(format);
        spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
        context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t packed) noexcept{return packed+4;};
        spDXTexture texture;spDXTextureSerializer serializer;spMemoryStream stream;Open(stream,data);std::string error;
        Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(data.size()),texture,&error),error.c_str());
        const auto* palette=texture.GetPaletteForAnalysis();
        Check(palette&&palette->IsExactly(spPalette::ClassID)&&palette->IsKindOf(spBaseObject::ClassID),"original palette class/direct base");
        Check(palette->HasInitializedEntriesForAnalysis()&&palette->GetIndexForAnalysis()==0xffffffffu,"CPU palette does not fabricate GPU registration");
        Bytes entries;for(auto b:palette->GetEntriesForAnalysis())entries.push_back(static_cast<std::uint8_t>(b));
        Check(std::equal(entries.begin(),entries.end(),data.begin()+13),"exact palette bytes");
        spPalette copied(*palette);
        Check(copied.HasInitializedEntriesForAnalysis()&&copied.GetEntriesForAnalysis()==palette->GetEntriesForAnalysis()
            &&copied.GetIndexForAnalysis()==0xffffffffu,"copy constructor copies entries with fresh index");
        auto clonedOwner=palette->Clone();const auto* cloned=dynamic_cast<const spPalette*>(clonedOwner.get());
        Check(cloned&&!cloned->HasInitializedEntriesForAnalysis()&&cloned->GetIndexForAnalysis()==0xffffffffu,"virtual palette clone is blank unlike copy constructor");
        spMemoryStream output;Open(output);Check(serializer.WritePayloadForAnalysis(output,texture,&error),error.c_str());
        const auto written=Data(output);Check(written==data,"exact palette flat codec roundtrip");
        std::ostringstream row;row<<"[\""<<mode<<"\",\""<<Hex(data)<<"\",[1,1,"<<format<<','<<texture.GetNativeByteCountForAnalysis()<<"],\""<<Hex(written)<<"\"]";
        // Safer host policy is intentionally different from native bad setter/dtor.
        texture.AdoptPaletteForAnalysis(std::make_unique<spPalette>());Open(output);
        Check(!serializer.WritePayloadForAnalysis(output,texture,&error)&&Data(output).empty(),"uninitialized palette blocked before writing any bytes");
        auto without=RuntimeInput(format,1);Open(stream,without);
        Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(without.size()),texture,&error)&&!texture.GetPaletteForAnalysis(),"palette-free reread safely clears old owned palette");
        return row.str();
    }
    void PaletteBounds()
    {
        spPalette empty;std::array<std::byte,4> shortEntries{};
        Check(!empty.SetEntriesForAnalysis(shortEntries.data(),shortEntries.size())&&!empty.HasInitializedEntriesForAnalysis(),"palette requires exact1024 bytes");
        for(unsigned mode=0;mode<3;++mode)
        {
            auto good=PaletteInput(5);spMemoryStream stream;Open(stream,good);
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            spDXTexture texture;spDXTextureSerializer serializer;std::string error;
            Check(serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(good.size()),texture,&error),error.c_str());
            const auto* old=texture.GetPaletteForAnalysis();
            auto broken=good;broken.resize(mode==0?13:mode==1?1036:good.size()-1);Open(stream,broken);
            Check(!serializer.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(broken.size()),texture,&error)&&context.failed,"truncated palette/mip rejected");
            Check(texture.GetPaletteForAnalysis()==old&&old->HasInitializedEntriesForAnalysis(),"failed reread leaves previous valid palette owned");
        }
    }
    void RuntimeBoundsAndHeaders()
    {
        for(int mode=0;mode<7;++mode)
        {
            auto data=RuntimeInput(3,1);
            if(mode==0)data.pop_back();if(mode==1)data[8]=8;if(mode==2)data[12]=1;
            if(mode==3)data[13]=0;if(mode==4)data[0]=0;if(mode==5)data[0]=3;
            spSerializerManager manager;spResourceManager resources;spSerializerReadContextForAnalysis context(manager,resources);
            if(mode==6)context.pcTexturePitchForAnalysis=[](void*,std::uint32_t,std::uint32_t) noexcept{return 0u;};
            spMemoryStream stream;Open(stream,data);spDXTexture texture;std::string error;
            Check(!spDXTextureSerializer{}.ReadPayloadForAnalysis(context,stream,static_cast<std::uint32_t>(data.size()),texture,&error)&&context.failed&&!error.empty(),"unrestored/malformed runtime texture rejected");
            Check(!texture.IsInitializedForAnalysis(),"host reads bounded payload before replacing texture state");
        }
        spDXTexture empty;spMemoryStream stream;Open(stream);std::string error;
        Check(!spDXTextureSerializer{}.WritePayloadForAnalysis(stream,empty,&error)&&Data(stream).empty(),"uninitialized runtime format is not invented output");
        Bytes header;Add(header,0x12345678u);Add(header,0xabcdef01u);Open(stream,header);
        auto dataObject=spTextureDataSerializer{}.ReadObjectHeaderAndCreateForAnalysis(stream);
        Check(dataObject&&dataObject->IsExactly(spDXTexture::ClassID)&&dataObject->IsKindOf(spTexture::ClassID),"actual data header creates correct runtime DXTexture despite arbitrary header");
        Open(stream,header);auto dxDataObject=spDXTextureDataSerializer{}.ReadObjectHeaderAndCreateForAnalysis(stream);
        Check(dxDataObject&&dxDataObject->IsExactly(spDXTexture::ClassID),"DXData uses same actual header factory");
        header.clear();Add(header,spDXTexture::ClassID);Add(header,0xabcdef01u);Open(stream,header);
        auto runtimeObject=spDXTextureSerializer{}.ReadObjectHeaderAndCreateForAnalysis(stream);
        Check(runtimeObject&&runtimeObject->IsExactly(spDXTexture::ClassID),"runtime serializer uses common generic header dispatch");
        Check(!dynamic_cast<spDXTexture*>(dataObject.get())->HasInitializedRuntimeFormatForAnalysis(),"correct factory does not invent native uninitialized44");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--encode-blocks"){EncodeBlocks();return 0;}
        if(argc==2&&std::string(argv[1])=="--optimize-blocks"){OptimizeBlocks();return 0;}
        if(argc==2&&std::string(argv[1])=="--optimize-blocks-trace"){OptimizeBlocks(true);return 0;}
        if(argc==2&&std::string(argv[1])=="--decode-blocks"){DecodeBlocks();return 0;}
        if(argc==2&&std::string(argv[1])=="--missing-native"){std::cout<<MissingMips()<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--cross"){std::cout<<Cross(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--runtime"){std::cout<<Runtime(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--native"){std::cout<<Native(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--palette"){std::cout<<Palette(argv[2])<<'\n';return 0;}
        if(argc==3&&std::string(argv[1])=="--native-write"){std::cout<<NativeWrite(argv[2])<<'\n';return 0;}
        for(const auto* mode:{"rgba","gray","rgb16"})(void)Cross(mode);Bounds();
        for(const auto* mode:{"0","1","2","3","4","5","6","7","3-2","3-4","0-4","2-2","5-2","7-4"})(void)Runtime(mode);
        RuntimeBoundsAndHeaders();
        for(const auto* mode:{"raw","dxt1","dxt3","dxt5","raw-2","raw-4","dxt1-4","dxt3-2","dxt5-4","raw-2-embedded","dxt1-4-embedded"})(void)Native(mode);
        NativeBounds();
        MissingMipGuards();
        BlockDecodeGuards();
        BlockOptimizerGuards();
        CompressedMipGuards();
        for(const auto* mode:{"raw","indexed","indexed-upload-fail"})(void)Palette(mode);
        PaletteBounds();
        for(const auto* mode:{"raw","raw-2","dxt1-4","raw-both","raw-auto","raw-4-partial"})(void)NativeWrite(mode);
        NativeWriterBounds();
        std::cout<<"PASS "<<checks<<'/'<<checks<<": bounded PC CPU texture source/local/nested codec\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

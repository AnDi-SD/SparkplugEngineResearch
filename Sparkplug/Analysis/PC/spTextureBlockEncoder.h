#pragma once

// PC64BB40 color block encoder and64C8BC explicit alpha, no dithering.
#include "spTextureBlockOptimizer.h"
#include "spTextureAlphaOptimizer.h"

namespace sparkplug::evidence::pc::texture_blocks
{
    inline unsigned Quantize(float value,unsigned maximum)
    {return static_cast<unsigned>(TruncateFloat(double(std::clamp(value,0.0F,1.0F))*maximum+.5));}
    inline std::uint16_t Encode565(const RGB& color)
    {return static_cast<std::uint16_t>((Quantize(color[0],31)<<11)|(Quantize(color[1],63)<<5)|Quantize(color[2],31));}
    inline bool EncodeColor(const Block& block,bool transparency,std::array<std::byte,8>& output)
    {
        unsigned transparent=0;for(const auto& color:block)transparent+=color[3]<.5F;
        if(transparency&&transparent==16)
        {output={std::byte{0},std::byte{0},std::byte{255},std::byte{255},std::byte{255},std::byte{255},std::byte{255},std::byte{255}};return true;}
        const unsigned steps=transparency&&transparent?3:4;const auto weights=Weights();Block quantized{};
        for(unsigned i=0;i<16;++i)for(unsigned c=0;c<3;++c)
        {
            const auto q=Quantize(block[i][c],c==1?63:31);const float reciprocal=Constant(c==1?0x3C820821:0x3D042108);
            if(c==2)quantized[i][c]=float80_rtz::ToFloat(float80_rtz::MultiplyFloat(
                float80_rtz::MultiplyFloat(float80_rtz::FromFloat(float(q)),reciprocal),weights[c]));
            else quantized[i][c]=TruncateFloat(double(TruncateFloat(double(q)*reciprocal))*weights[c]);
        }
        RGB a,b;if(!OptimizeRGB(quantized,steps,a,b))return false;
        const RGB inverse{Constant(0x4057762E),1,Constant(0x411EC1DD)};
        for(unsigned c=0;c<3;++c){a[c]=static_cast<float>(double(a[c])*inverse[c]);b[c]=static_cast<float>(double(b[c])*inverse[c]);}
        auto first=Encode565(a),second=Encode565(b);
        std::uint32_t indices=0;
        if(steps!=4||first!=second)
        {
            if((steps==3)!=(first<=second))std::swap(first,second);
            const auto decodedA=Decode565(first),decodedB=Decode565(second);
            RGB low{},high{};for(unsigned c=0;c<3;++c)
            {low[c]=static_cast<float>(double(decodedA[c])*weights[c]);high[c]=static_cast<float>(double(decodedB[c])*weights[c]);}
            const double difference[3]={float(double(high[0])-low[0]),double(high[1])-low[1],float(double(high[2])-low[2])};
            const double square=(difference[2]*difference[2]+difference[1]*difference[1])+difference[0]*difference[0];
            const double scale=first==second?0:double(steps-1)/square;
            RGB direction{};for(unsigned c=0;c<3;++c)direction[c]=static_cast<float>(difference[c]*scale);
            const std::array<unsigned,4> lookup=steps==3?std::array<unsigned,4>{0,2,1,0}:std::array<unsigned,4>{0,2,3,1};
            for(unsigned i=0;i<16;++i)
            {
                unsigned index=3;
                if(steps!=3||block[i][3]>=.5F)
                {
                    RGB weighted{};for(unsigned c=0;c<3;++c)weighted[c]=TruncateFloat(double(block[i][c])*weights[c]);
                    const double projection=((double(weighted[2])-low[2])*direction[2]+(double(weighted[1])-low[1])*direction[1])
                        +(double(weighted[0])-low[0])*direction[0];
                    if(projection<=0)index=0;else if(projection>=steps-1)index=1;
                    else index=lookup[static_cast<unsigned>(TruncateFloat(projection+.5))];
                }
                indices|=index<<(i*2);
            }
        }
        output={std::byte(first&255),std::byte(first>>8),std::byte(second&255),std::byte(second>>8),
                std::byte(indices&255),std::byte((indices>>8)&255),std::byte((indices>>16)&255),std::byte(indices>>24)};
        return true;
    }
    inline std::array<std::byte,8> EncodeAlpha(const Block& block)
    {
        std::array<float,16> quantized{};float minimum=block[0][3],maximum=minimum;
        for(unsigned i=0;i<16;++i)
        {
            const double normalized=double(Quantize(block[i][3],255))*Constant(0x3B808081);
            quantized[i]=TruncateFloat(normalized);
            if(normalized<minimum)minimum=quantized[i];else if(normalized>maximum)maximum=quantized[i];
        }
        if(minimum==1)return {std::byte{255},std::byte{255},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{0}};
        const unsigned steps=minimum!=0&&maximum!=1?8:6;const auto endpoints=OptimizeAlpha(quantized,steps);
        unsigned first=Quantize(endpoints[0],255),second=Quantize(endpoints[1],255);
        std::array<std::byte,8> output{};
        if(steps==8&&first==second){output[0]=output[1]=std::byte(first);return output;}
        if(steps==8)std::swap(first,second);
        output[0]=std::byte(first);output[1]=std::byte(second);
        const float a=static_cast<float>(double(first)*Constant(0x3B808081)),b=static_cast<float>(double(second)*Constant(0x3B808081));
        const float scale=a==b?0:static_cast<float>(double(steps-1)/(double(b)-a));
        const std::array<unsigned,8> lookup=steps==6?std::array<unsigned,8>{0,2,3,4,5,1,0,0}:std::array<unsigned,8>{0,2,3,4,5,6,7,1};
        std::uint64_t indices=0;
        for(unsigned i=0;i<16;++i)
        {
            const float value=block[i][3];const double projection=(double(value)-a)*scale;unsigned index;
            if(projection<=0)index=steps==6&&value<=double(a)*.5?6:0;
            else if(projection>=steps-1)index=steps==6&&value>=(double(b)+1)*.5?7:1;
            else index=lookup[static_cast<unsigned>(TruncateFloat(projection+.5))];
            indices|=std::uint64_t(index)<<(i*3);
        }
        for(unsigned i=0;i<6;++i)output[i+2]=std::byte((indices>>(8*i))&255);return output;
    }
    inline bool EncodeNoDither(unsigned flags,const Block& block,std::vector<std::byte>& output)
    {
        if(flags<1||flags>3)return false;
        for(const auto& color:block)for(float value:color)if(!std::isfinite(value)||value<0||value>1)return false;
        std::array<std::byte,8> colors{};if(!EncodeColor(block,flags==1,colors))return false;
        std::vector<std::byte> bytes(flags==1?8:16);
        if(flags==2)for(unsigned i=0;i<16;++i)bytes[i/2]|=std::byte(Quantize(block[i][3],15)<<(4*(i%2)));
        if(flags==3){const auto alpha=EncodeAlpha(block);std::copy(alpha.begin(),alpha.end(),bytes.begin());}
        std::copy(colors.begin(),colors.end(),bytes.begin()+(flags==1?0:8));output=std::move(bytes);return true;
    }
}

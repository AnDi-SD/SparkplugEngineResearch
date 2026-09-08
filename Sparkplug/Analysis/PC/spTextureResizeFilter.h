#pragma once
// Original619219 wrap coefficient table and61C44F source-order accumulation.
// Common upload uses both axes when either normalized dimension changes.
#include "spTextureMipFilter.h"

namespace sparkplug::evidence::pc::texture_mips
{
    using AxisRows=std::vector<std::vector<std::pair<std::uint32_t,float>>>;
    inline bool BuildWrappedAxis(std::uint32_t source,std::uint32_t destination,AxisRows& output)
    {
        if(!source||!destination||source>65535||destination>65535||destination>2u*source)return false;
        const float ratio=static_cast<float>(double(destination)/source);
        const float halfInverse=static_cast<float>(.5*source/destination);
        AxisRows rows(source);
        for(std::uint32_t i=0;i<source;++i)
        {
            std::uint32_t index=0;float sum=0;
            for(unsigned half=0;half<2;++half)
            {
                const float offset=static_cast<float>(double(i)+half-.5);
                const float lower=static_cast<float>(double(offset)*ratio);
                const float upper=static_cast<float>(double(offset)*ratio+ratio);
                for(auto j=static_cast<std::int32_t>(std::floor(lower));double(j)<upper;++j)
                {
                    const auto key=static_cast<std::uint32_t>(j<0?j+destination:j>=static_cast<std::int32_t>(destination)?j-destination:j);
                    if(key!=index)
                    {if(sum>1e-5F)rows[i].emplace_back(index,sum);sum=0;index=key;}
                    const double a=std::max(double(j),double(lower)),b=std::min(double(j)+1,double(upper));
                    double blend=(a+b)*halfInverse-offset;if(half)blend=1-blend;
                    sum=static_cast<float>(double(sum)+(b-a)*blend);
                }
            }
            if(sum>1e-5F)rows[i].emplace_back(index,sum);
        }
        output=std::move(rows);return true;
    }
    inline std::array<float,4> DecodeRawPixel(const std::byte* pixels,std::uint32_t format)
    {
        if(format<=1)
        {
            std::array<float,4> color{};
            for(unsigned c=0;c<4;++c)color[c]=static_cast<float>(double(static_cast<unsigned>(pixels[c]))*(1.0F/255.0F));
            if(format==1)color[3]=1;return color;
        }
        const unsigned word=static_cast<unsigned>(pixels[0])|(static_cast<unsigned>(pixels[1])<<8);
        if(format==3)return {static_cast<float>(double(word&31)*(1.0F/31.0F)),
            static_cast<float>(double((word>>5)&63)*(1.0F/63.0F)),static_cast<float>(double(word>>11)*(1.0F/31.0F)),1};
        std::array<float,4> color{};
        for(unsigned c=0;c<4;++c)color[c]=static_cast<float>(double((word>>(4*c))&15)*(1.0F/15.0F));
        return color;
    }
    inline unsigned EncodeRawChannel(float value,unsigned maximum,double bias)
    {
        const auto scaled=PositiveFloatTowardZero(double(std::clamp(value,0.0F,1.0F))*maximum);
        return static_cast<unsigned>(PositiveFloatTowardZero(double(scaled)+bias));
    }
    inline bool ResampleRaw(const Mip& source,std::uint32_t format,std::uint32_t width,std::uint32_t height,
        bool dither,Mip& output)
    {
        if(format>4)return false;const unsigned pixelSize=format<=1?4:format==2?1:2;
        if(!source.width||!source.height||source.rowBytes!=source.width*pixelSize||source.rows!=source.height
            ||source.packedBytes.size()!=std::uint64_t(source.width)*source.height*pixelSize
            ||std::uint64_t(source.width)*source.height*4>16u*1024u*1024u
            ||std::uint64_t(width)*height*4>16u*1024u*1024u)return false;
        Mip destination;
        if(!reconstruction::spDXTexture::DescribeMipForAnalysis(width,height,format+3,destination))return false;
        if(format==2)
        {
            // Common raw input supplies no palette. Native codecs construct
            //256 identical white RGBA entries on both sides;62274A keeps the
            //first equal-distance entry. Every filtered pixel therefore maps
            //to index0. This is not a general supplied-palette quantizer.
            destination.packedBytes.assign(std::size_t(destination.rowBytes)*destination.rows,std::byte{0});
            output=std::move(destination);return true;
        }
        AxisRows columns,rows;
        if(!BuildWrappedAxis(source.width,width,columns)||!BuildWrappedAxis(source.height,height,rows))return false;
        std::vector<float> sums(std::size_t(width)*height*4,0);
        for(std::uint32_t y=0;y<source.height;++y)for(std::uint32_t x=0;x<source.width;++x)
        {
            const auto color=DecodeRawPixel(source.packedBytes.data()+(std::size_t(y)*source.width+x)*pixelSize,format);
            for(const auto& row:rows[y])for(const auto& column:columns[x])
            {
                const double weight=double(row.second)*column.second;
                const auto dest=(std::size_t(row.first)*width+column.first)*4;
                for(unsigned c=0;c<4;++c)sums[dest+c]=static_cast<float>(double(sums[dest+c])
                    +color[c]*weight);
            }
        }
        destination.packedBytes.resize(std::size_t(destination.rowBytes)*destination.rows);
        //60FDB4 replaces DEFAULT with0x80004: triangle plus ordered dither.
        //61F153 scans alternate rows backwards; table position follows scan
        //order, not physical x. Mip generation instead passes filter4.
        constexpr unsigned thresholds[4][4]{{31,15,27,11},{7,23,3,19},{25,9,29,13},{1,17,5,21}};
        for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x)
        {
            const auto scan=(y&1)?width-1-x:x;
            const double bias=dither?thresholds[y&3][scan&3]/32.0:.5;
            unsigned encoded[4]{};
            for(unsigned c=0;c<4;++c)
            {
                const auto i=(std::size_t(y)*width+x)*4+c;
                const unsigned maximum=format<=1?255:format==3?(c==1?63:31):15;
                encoded[c]=EncodeRawChannel(sums[i],maximum,bias);
            }
            const auto at=(std::size_t(y)*width+x)*pixelSize;
            if(format<=1)
            {for(unsigned c=0;c<4;++c)destination.packedBytes[at+c]=std::byte(format==1&&c==3?0:encoded[c]);}
            else
            {
                const unsigned word=format==3?encoded[0]|(encoded[1]<<5)|(encoded[2]<<11)
                    :encoded[0]|(encoded[1]<<4)|(encoded[2]<<8)|(encoded[3]<<12);
                destination.packedBytes[at]=std::byte(word&255);destination.packedBytes[at+1]=std::byte(word>>8);
            }
        }
        output=std::move(destination);return true;
    }
    inline bool ResizeRGBA(const Mip& source,std::uint32_t width,std::uint32_t height,Mip& output)
    {return ResampleRaw(source,0,width,height,true,output);}
}

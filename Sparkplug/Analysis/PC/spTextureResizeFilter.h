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
    inline bool ResizeRGBA(const Mip& source,std::uint32_t width,std::uint32_t height,Mip& output)
    {
        if(!source.width||!source.height||source.rowBytes!=source.width*4||source.rows!=source.height
            ||source.packedBytes.size()!=std::uint64_t(source.width)*source.height*4
            ||source.packedBytes.size()>16u*1024u*1024u)return false;
        Mip destination;
        if(!reconstruction::spDXTexture::DescribeMipForAnalysis(width,height,3,destination))return false;
        AxisRows columns,rows;
        if(!BuildWrappedAxis(source.width,width,columns)||!BuildWrappedAxis(source.height,height,rows))return false;
        std::vector<float> sums(static_cast<std::size_t>(destination.rowBytes)*destination.rows,0);
        std::array<float,256> decoded{};
        for(unsigned i=0;i<256;++i)decoded[i]=static_cast<float>(double(i)*(1.0F/255.0F));
        for(std::uint32_t y=0;y<source.height;++y)for(std::uint32_t x=0;x<source.width;++x)
            for(const auto& row:rows[y])for(const auto& column:columns[x])
            {
                const double weight=double(row.second)*column.second;
                const auto input=(std::size_t(y)*source.width+x)*4,dest=(std::size_t(row.first)*width+column.first)*4;
                for(unsigned c=0;c<4;++c)sums[dest+c]=static_cast<float>(double(sums[dest+c])
                    +decoded[static_cast<unsigned>(source.packedBytes[input+c])]*weight);
            }
        destination.packedBytes.resize(sums.size());
        //60FDB4 replaces DEFAULT with0x80004: triangle plus ordered dither.
        //61F153 scans alternate rows backwards; table position follows scan
        //order, not physical x. Mip generation instead passes filter4.
        constexpr unsigned thresholds[4][4]{{31,15,27,11},{7,23,3,19},{25,9,29,13},{1,17,5,21}};
        for(std::uint32_t y=0;y<height;++y)for(std::uint32_t x=0;x<width;++x)
        {
            const auto scan=(y&1)?width-1-x:x;
            const double bias=thresholds[y&3][scan&3]/32.0;
            for(unsigned c=0;c<4;++c)
            {
                const auto i=(std::size_t(y)*width+x)*4+c;
                const auto scaled=PositiveFloatTowardZero(double(std::clamp(sums[i],0.0F,1.0F))*255.0);
                destination.packedBytes[i]=static_cast<std::byte>(static_cast<unsigned>(PositiveFloatTowardZero(double(scaled)+bias)));
            }
        }
        output=std::move(destination);return true;
    }
}

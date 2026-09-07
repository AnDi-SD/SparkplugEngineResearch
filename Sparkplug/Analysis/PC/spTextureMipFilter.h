#pragma once

// PC4AB030 -> 61039A -> 60FDB4 -> 61C44F: native raw missing-mip branch.
// This bounded CPU shadow preserves the observed filter4 wrap coefficients,
// source-order float stores and the RGBA codec's final rounding mode.
#include "Code/SparkplugDX/spDXTexture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace sparkplug::evidence::pc::texture_mips
{
    using Mip=reconstruction::spDXTexture::MipForAnalysis;
    struct Contributions final
    {
        std::array<std::pair<std::uint32_t,float>,2> values{};
        unsigned count=1;
    };
    inline Contributions Axis(std::uint32_t source,std::uint32_t size)
    {
        if(size==1)return {{{{0u,1.0F},{0u,0.0F}}},1};
        if(size==2)return {{{{0u,.5F},{0u,0.0F}}},1};
        const auto target=size/2,index=source/2;
        if(source&1)return {{{{index,.4375F},{(index+1)%target,.0625F}}},2};
        return {{{{(index+target-1)%target,.0625F},{index,.4375F}}},2};
    }
    inline float PositiveFloatTowardZero(double value)
    {
        const auto rounded=static_cast<float>(value);
        return rounded>value?std::nextafter(rounded,0.0F):rounded;
    }
    inline std::byte Encode(float value)
    {
        // PC61F1E9 sets x87 RC=truncate for both float stores and FISTP.
        const auto scaled=PositiveFloatTowardZero(double(std::clamp(value,0.0F,1.0F))*255.0);
        const auto biased=PositiveFloatTowardZero(double(scaled)+.5);
        return static_cast<std::byte>(static_cast<unsigned>(biased));
    }
    inline bool GenerateNext(const Mip& source,Mip& destination)
    {
        const auto width=source.width,height=source.height;
        if(!width||!height||(width&(width-1))||(height&(height-1))||width>65535||height>65535
            ||(width==1&&height==1)||source.rowBytes!=width*4||source.rows!=height
            ||source.packedBytes.size()!=std::uint64_t(width)*height*4
            ||source.packedBytes.size()>16u*1024u*1024u)return false;
        Mip generated;
        if(!reconstruction::spDXTexture::DescribeMipForAnalysis(std::max(1u,width/2),std::max(1u,height/2),3,generated))return false;
        std::vector<float> sums(static_cast<std::size_t>(generated.rowBytes)*generated.rows,0);
        // Original625743 uses binary32 constant 3B808081 then stores each
        // decoded channel. The lookup avoids repeated conversion per target.
        std::array<float,256> decoded{};
        constexpr float reciprocal=1.0F/255.0F;
        for(unsigned i=0;i<256;++i)decoded[i]=static_cast<float>(double(i)*reciprocal);
        for(std::uint32_t y=0;y<height;++y)
        {
            const auto rows=Axis(y,height);
            for(std::uint32_t x=0;x<width;++x)
            {
                const auto columns=Axis(x,width);const auto input=(std::size_t(y)*width+x)*4;
                for(unsigned r=0;r<rows.count;++r)for(unsigned c=0;c<columns.count;++c)
                {
                    const auto& row=rows.values[r];const auto& column=columns.values[c];
                    const double weight=double(row.second)*column.second;
                    const auto output=(std::size_t(row.first)*generated.width+column.first)*4;
                    for(unsigned channel=0;channel<4;++channel)
                        sums[output+channel]=static_cast<float>(double(sums[output+channel])
                            +decoded[static_cast<unsigned>(source.packedBytes[input+channel])]*weight);
                }
            }
        }
        generated.packedBytes.resize(sums.size());
        for(std::size_t i=0;i<sums.size();++i)generated.packedBytes[i]=Encode(sums[i]);
        destination=std::move(generated);return true;
    }
}

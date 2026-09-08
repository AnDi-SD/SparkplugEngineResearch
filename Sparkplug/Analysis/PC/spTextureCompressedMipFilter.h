#pragma once

// PC61C44F with627873/627452 compressed row codecs. Filter float values
// directly, then encode each four-by-four block with the observed dither=0.
#include "spTextureMipFilter.h"
#include "spTextureBlockEncoder.h"

namespace sparkplug::evidence::pc::texture_mips
{
    inline bool GenerateNextCompressed(const Mip& source,unsigned flags,Mip& destination)
    {
        using namespace texture_blocks;const auto width=source.width,height=source.height;
        if(flags<1||flags>3||!width||!height||(width&(width-1))||(height&(height-1))
            ||width>65535||height>65535||(width==1&&height==1)
            ||std::uint64_t(width)*height*4>16u*1024u*1024u)return false;
        Mip layout,generated;
        if(!reconstruction::spDXTexture::DescribeMipForAnalysis(width,height,flags-1,layout)
            ||source.rowBytes!=layout.rowBytes||source.rows!=layout.rows
            ||source.packedBytes.size()!=std::uint64_t(layout.rowBytes)*layout.rows
            ||!reconstruction::spDXTexture::DescribeMipForAnalysis(std::max(1u,width/2),std::max(1u,height/2),flags-1,generated))return false;
        // Same16-MiB decoded pixel extent as raw input: at most64 MiB of
        // source floats and16 MiB of destination floats, plus packed storage.
        std::vector<Color> pixels(std::size_t(width)*height),sums(std::size_t(generated.width)*generated.height);
        const unsigned blockBytes=flags==1?8:16;
        for(unsigned y=0;y<height;y+=4)for(unsigned x=0;x<width;x+=4)
        {
            Block block;const auto offset=std::size_t(y/4)*source.rowBytes+(x/4)*blockBytes;
            if(!Decode(flags,source.packedBytes.data()+offset,blockBytes,block))return false;
            for(unsigned by=0;by<4&&y+by<height;++by)for(unsigned bx=0;bx<4&&x+bx<width;++bx)
                pixels[std::size_t(y+by)*width+x+bx]=block[by*4+bx];
        }
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
        {
            const auto rows=Axis(y,height),columns=Axis(x,width);const auto& input=pixels[std::size_t(y)*width+x];
            for(unsigned r=0;r<rows.count;++r)for(unsigned c=0;c<columns.count;++c)
            {
                const auto& row=rows.values[r];const auto& column=columns.values[c];
                auto& output=sums[std::size_t(row.first)*generated.width+column.first];const double weight=double(row.second)*column.second;
                for(unsigned channel=0;channel<4;++channel)output[channel]=static_cast<float>(double(output[channel])+input[channel]*weight);
            }
        }
        generated.packedBytes.resize(std::size_t(generated.rowBytes)*generated.rows);
        for(unsigned y=0;y<generated.height;y+=4)for(unsigned x=0;x<generated.width;x+=4)
        {
            Block block;for(unsigned by=0;by<4;++by)for(unsigned bx=0;bx<4;++bx)
                block[by*4+bx]=sums[std::size_t((y+by)%generated.height)*generated.width+(x+bx)%generated.width];
            std::vector<std::byte> packed;if(!EncodeNoDither(flags,block,packed))return false;
            const auto offset=std::size_t(y/4)*generated.rowBytes+(x/4)*blockBytes;
            std::copy(packed.begin(),packed.end(),generated.packedBytes.begin()+offset);
        }
        destination=std::move(generated);return true;
    }
}

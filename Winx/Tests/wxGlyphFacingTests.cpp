#include "Code/wxGlyph.h"
#include "Analysis/PC/wxGlyphAbi.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using Glyph = winx::reconstruction::wxGlyph;
using Prefix = Glyph::CapturedPrefixForAnalysis;
using Extended = Glyph::ExtendedReturnForAnalysis;
using Result = Glyph::ResultForAnalysis;
constexpr auto Profile = Glyph::ProfileForAnalysis::PcCapturedPrimaryFacingFiniteNormalOrZeroF32NoOpSetter;
unsigned checks = 0;
void Check(bool value) { ++checks; if (!value) throw std::runtime_error("GlyphFacing contract"); }
void Put(Prefix& p, unsigned at, std::uint32_t word) { for (unsigned j=0;j<4;++j) p[at+j]=std::uint8_t(word>>(8*j)); }
std::uint32_t Word(const Prefix& p,unsigned at) { return std::uint32_t(p[at])|(std::uint32_t(p[at+1])<<8)|(std::uint32_t(p[at+2])<<16)|(std::uint32_t(p[at+3])<<24); }
template<class T> std::string Hex(const T& p) { constexpr char h[]="0123456789abcdef"; std::string s; for(auto v:p) { s+=h[v>>4]; s+=h[v&15]; } return s; }
unsigned Nibble(char c) { if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;throw std::runtime_error("hex"); }
template<class T> T Parse(const std::string& s) { T p{};if(s.size()!=p.size()*2)throw std::runtime_error("size");for(unsigned j=0;j<p.size();++j)p[j]=std::uint8_t((Nibble(s[2*j])<<4)|Nibble(s[2*j+1]));return p; }
void Contracts()
{
    struct Fixture { std::uint32_t word, argument; Extended expected; };
    const Fixture fixtures[] = {
        {0x00000000, 0x7FC00001, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
        {0x80000000, 0xFF800000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80}},
        {0x3F800000, 0x00000001, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xFF,0x3F}},
        {0xBF800000, 0x80000001, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xFF,0xBF}},
        {0x00800000, 0x00000000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x81,0x3F}},
        {0x80800000, 0xFFFFFFFF, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x81,0xBF}},
        {0x7F7FFFFF, 0x3F800000, {0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x7E,0x40}},
        {0xFF7FFFFF, 0xBF800000, {0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x7E,0xC0}},
        {0x3F800001, 0x7FC00001, {0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x80,0xFF,0x3F}},
        {0xBF800001, 0xFF800000, {0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x80,0xFF,0xBF}},
        {0x3F000000, 0x00000001, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xFE,0x3F}},
        {0x40000000, 0x80000001, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x40}},
        {0x40490FDB, 0x00000000, {0x00,0x00,0x00,0x00,0x00,0xDB,0x0F,0xC9,0x00,0x40}},
        {0xC0490FDB, 0xFFFFFFFF, {0x00,0x00,0x00,0x00,0x00,0xDB,0x0F,0xC9,0x00,0xC0}},
        {0x01000001, 0x3F800000, {0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x80,0x82,0x3F}},
        {0xFEFFFFFF, 0xBF800000, {0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x7D,0xC0}}
    };
    for(const auto& f:fixtures)
    {
        Prefix p{};Put(p,0,0x701CE4);Put(p,0x15C,f.word);
        Extended value{};Check(Glyph::ReadFacingForAnalysis(p,value,Profile)==Result::Completed&&value==f.expected);
        auto alias=p;Check(Glyph::ApplyFacingNoOpForAnalysis(alias,f.argument,alias,Profile)==Result::Completed&&alias==p);
        Glyph::ComposedFacingForAnalysis composed{};Check(Glyph::ComposeFacingForAnalysis(p,f.argument,composed,Profile)==Result::Completed);
        Check(composed.before==f.expected&&composed.after==f.expected&&composed.afterNoOp==p);
        composed.afterNoOp=p;Check(Glyph::ComposeFacingForAnalysis(composed.afterNoOp,f.argument,composed,Profile)==Result::Completed);
        Check(composed.afterNoOp==p&&composed.before==f.expected&&composed.after==f.expected);
    }
    Prefix p{};Put(p,0,0x701CE4);Put(p,0x15C,0x3F800000);Extended sentinel{};sentinel.fill(0xA6);auto result=sentinel;
    Check(Glyph::ReadFacingForAnalysis(p,result)==Result::UnqualifiedProfile&&result==sentinel);
    auto unchanged=p;Check(Glyph::ApplyFacingNoOpForAnalysis(p,0,unchanged)==Result::UnqualifiedProfile&&unchanged==p);
    auto wrong=p;Put(wrong,0,0);Check(Glyph::ReadFacingForAnalysis(wrong,result,Profile)==Result::CapturedPrimaryNotAdmitted&&result==sentinel);
    for(auto bad:{1u,0x80000001u,0x7F800000u,0x7FC00001u})
    {
        auto input=p;Put(input,0x15C,bad);result=sentinel;
        Check(Glyph::ReadFacingForAnalysis(input,result,Profile)==Result::CapturedFacingNotAdmitted&&result==sentinel);
        Prefix copied{};Check(Glyph::ApplyFacingNoOpForAnalysis(input,0xFFFFFFFF,copied,Profile)==Result::Completed&&copied==input);
        Glyph::ComposedFacingForAnalysis composed{};composed.before.fill(0xA6);composed.after.fill(0xB7);composed.afterNoOp.fill(0xC8);auto before=composed;
        Check(Glyph::ComposeFacingForAnalysis(input,0,composed,Profile)==Result::CapturedFacingNotAdmitted);
        Check(composed.before==before.before&&composed.after==before.after&&composed.afterNoOp==before.afterNoOp);
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")
        {
            std::string line;while(std::getline(std::cin,line))
            {
                std::istringstream in(line);std::string raw,arg,extra;if(!(in>>raw>>arg)||in>>extra)throw std::runtime_error("row");
                auto p=Parse<Prefix>(raw);auto a=Parse<std::array<std::uint8_t,4>>(arg);std::uint32_t word=0;for(unsigned j=0;j<4;++j)word|=std::uint32_t(a[j])<<(8*j);
                Glyph::ComposedFacingForAnalysis out{};if(Glyph::ComposeFacingForAnalysis(p,word,out,Profile)!=Result::Completed)throw std::runtime_error("admission");
                std::cout<<Hex(out.before)<<' '<<Hex(out.afterNoOp)<<' '<<Hex(out.after)<<'\n';
            }
            return 0;
        }
        if(argc!=1)throw std::runtime_error("option");Contracts();std::cout<<"GlyphFacing: "<<checks<<" contracts passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

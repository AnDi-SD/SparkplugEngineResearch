#include "Code/wxTextController.h"
#include "Analysis/PC/wxTextControllerAbi.h"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
    void Replay()
    {
        unsigned repeats=0;
        while(std::cin>>repeats)
        {
            Check(repeats>=1&&repeats<=2,"Bounded Reset repeat count");
            wxTextController::CapturedOwnWordsForAnalysis input;
            for(auto& word:input)Check(bool(std::cin>>word),"Complete captured own-state input");
            wxTextController object(input);
            for(unsigned i=0;i<repeats;++i)
            {
                object.ResetForAnalysis();
                const auto& words=object.GetOwnWordsForAnalysis();
                for(std::size_t j=0;j<words.size();++j)std::cout<<(j?" ":"")<<words[j];
                std::cout<<'\n';
            }
        }
        Check(std::cin.eof(),"Complete TextController replay stream");
    }
    void Contracts()
    {
        wxTextController::CapturedOwnWordsForAnalysis input;
        for(std::size_t i=0;i<input.size();++i)input[i]=0x70000000u+static_cast<std::uint32_t>(i);
        wxTextController object(input);
        Check(object.IsExactly(wxTextController::ClassID)&&object.IsKindOf(spBaseObject::ClassID),"Native physical Base ancestry");
        Check(wxTextController::StaticRTTI().factory==nullptr,"Captured adapter does not expose an invented native factory");
        Check(object.GetOwnWordsForAnalysis()==input,"Caller captured state is retained before Reset");
        object.ResetForAnalysis();const auto first=object.GetOwnWordsForAnalysis();
        for(std::size_t i=0;i<first.size();++i)
            Check(first[i]==(i<186?0:input[i]),"Own cleared ranges and untouched final18 words");
        object.ResetForAnalysis();Check(first==object.GetOwnWordsForAnalysis(),"Reset repeated on already reset state");
        spCloneManager clones;Check(!object.vfunc_10(clones),"Unqualified Clone has explicit refusal");
    }
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--replay")Replay();else{Contracts();std::cout<<"TextController captured Reset contracts passed\n";}
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

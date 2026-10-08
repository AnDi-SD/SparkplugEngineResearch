#include "spPS2Mouse.h"
#include "../../Analysis/PS2/spPS2MouseAbi.h"
#include <cstring>
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        namespace abi = sparkplug::evidence::ps2;
        std::unique_ptr<spBaseObject> CreateMouse() { return std::make_unique<spPS2Mouse>(); }
        const spRTTIRecord Record{spPS2Mouse::ClassID,spPS2InputDevice::ClassID,"spPS2Mouse",
            &spPS2InputDevice::StaticRTTI(),&CreateMouse,nullptr};
        const bool Registered=spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);
        std::int8_t SignedByte(std::uint8_t bits) noexcept
        { std::int8_t value;std::memcpy(&value,&bits,1);return value; }
        std::uint32_t FloatBits(float value) noexcept
        { std::uint32_t bits;std::memcpy(&bits,&value,4);return bits; }
        template<class T> T Require(std::optional<T> value)
        { if(!value)throw std::logic_error("Original PS2 mouse reads an unresolved payload or EE conversion");return *value; }
    }
    const spRTTIRecord& spPS2Mouse::StaticRTTI() noexcept {(void)Registered;return Record;}
    const spRTTIRecord& spPS2Mouse::vfunc_18() const noexcept {return Record;}
    std::unique_ptr<spBaseObject> spPS2Mouse::vfunc_10(spCloneManager& manager) const
    {
        // Borrowed host binding is analytical, not a copied native tail field.
        auto clone=std::make_unique<spPS2Mouse>(boundary_);
        manager.RegisterCloneForAnalysis(*this,*clone);
        if(!vfunc_14(*clone,manager))return {};
        return clone;
    }
    std::optional<std::uint32_t> spPS2Mouse::ReadTailWordForAnalysis(std::uint32_t offset) const noexcept
    {
        if(offset<0x48||offset>0xDC)return std::nullopt;
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)
        {const auto& cell=tail_[offset-0x48+i];if(!cell)return std::nullopt;value|=std::uint32_t(*cell)<<(8*i);}
        return value;
    }
    void spPS2Mouse::WriteTailWordForAnalysis(std::uint32_t offset,std::uint32_t value)
    {
        if(offset<0x48||offset>0xDC)throw std::out_of_range("PS2 mouse native tail word");
        for(unsigned i=0;i<4;++i)tail_[offset-0x48+i]=static_cast<std::uint8_t>(value>>(8*i));
    }
    void spPS2Mouse::ClearForAnalysis(std::uint32_t offset,std::uint32_t bytes) noexcept
    {for(std::uint32_t i=0;i<bytes;++i)tail_[offset-0x48+i]=0;}
    bool spPS2Mouse::WaitForAnalysis()
    {
        const auto semaphore=boundary_->ReadGpWord(abi::PS2MouseSemaphoreGpOffset);
        if(!semaphore)return false;
        boundary_->WaitSemaphore(*semaphore);return true;
    }
    bool spPS2Mouse::SignalForAnalysis()
    {
        const auto semaphore=boundary_->ReadGpWord(abi::PS2MouseSemaphoreGpOffset);
        if(!semaphore)return false;
        boundary_->SignalSemaphore(*semaphore);return true;
    }
    bool spPS2Mouse::RpcCompletionForAnalysis(std::string* error)
    {
        if(!boundary_){if(error)*error="PS2 mouse host boundary is not bound";return false;}
        const auto semaphore=boundary_->ReadGpWord(abi::PS2MouseSemaphoreGpOffset);
        if(!semaphore){if(error)*error="Original RPC callback semaphore cell is unresolved";return false;}
        boundary_->SignalSemaphoreFromInterrupt(*semaphore);return true;
    }
    std::optional<std::int32_t> spPS2Mouse::InitializeRpcForAnalysis()
    {
        const auto semaphore=boundary_->CreateSemaphore({std::nullopt,1,1});
        if(!semaphore)return std::nullopt;
        boundary_->WriteGpWord(abi::PS2MouseSemaphoreGpOffset,static_cast<std::uint32_t>(*semaphore));
        if(*semaphore==-1){boundary_->Report(0x45BA50,std::nullopt);return -1;}
        boundary_->InitializeRpc(0);
        for(;;)
        {
            const auto status=boundary_->BindRpc(abi::PS2MouseRpcClient,0x80000210,0);
            if(!status)return std::nullopt;
            if(*status<0){boundary_->Report(0x45BA40,std::nullopt);return -1;}
            const auto server=boundary_->ReadWord(abi::PS2MouseRpcClient+0x24);
            if(!server)return std::nullopt;
            if(*server)return 0;
            // No invented retry count: native repeats until this live cell is
            // nonzero. A backend's own resource limit must stop its own work.
        }
    }
    std::optional<bool> spPS2Mouse::InitializeForAnalysis(std::string* error)
    {
        auto unresolved=[&]()->std::optional<bool>{if(error)*error="Original PS2 mouse initialization dependency is unresolved";return std::nullopt;};
        if(!boundary_)return unresolved();
        const auto gate=boundary_->ReadGpByte(abi::PS2MouseInitializeGateGpOffset);
        if(!gate)return unresolved();
        if(!*gate)return true; // Native leaves flag/tail unchanged in this path.
        const auto initialized=InitializeRpcForAnalysis();
        if(!initialized)return unresolved();
        if(*initialized==-1)return false;
        SetField44ForAnalysis(1);WriteTailWordForAnalysis(0x48,2);
        ClearForAnalysis(0x50,0x30);ClearForAnalysis(0x80,0x30);ClearForAnalysis(0xB0,0x30);
        return true;
    }
    std::optional<std::int32_t> spPS2Mouse::CallForAnalysis(unsigned function,std::uint32_t receive)
    {
        return boundary_->CallRpc({abi::PS2MouseRpcClient,function,1,abi::PS2MouseRpcSend,
            0x20,receive,0x20,abi::PS2MouseRpcCompletion,0});
    }
    std::optional<bool> spPS2Mouse::ProcessEventForAnalysis()
    {
        ClearForAnalysis(0xB0,0x30);
        const auto incoming=boundary_->ReadByte(0x4B7881);
        if(!incoming)return std::nullopt;
        tail_[0x70-0x48]=*incoming;
        const auto previous=tail_[0xA0-0x48],current=tail_[0x70-0x48];
        if(!previous||!current)return std::nullopt;
        if(*current!=*previous)
            for(unsigned bit=0;bit<8;++bit)
            {
                const unsigned now=(*current>>bit)&1,old=(*previous>>bit)&1;
                WriteTailWordForAnalysis(0x50+4*bit,now);
                if(now!=old)WriteTailWordForAnalysis(0xB0+4*bit,1);
            }
        for(unsigned axis=0;axis<2;++axis)
        {
            const auto offset=boundary_->ReadByte(0x4B7882+axis);
            const auto position=ReadTailWordForAnalysis(0x74+4*axis);
            if(!offset||!position)return std::nullopt;
            const auto updated=boundary_->AddSignedByte(*position,SignedByte(*offset));
            if(!updated)return std::nullopt;
            WriteTailWordForAnalysis(0x74+4*axis,*updated);
        }
        const auto format=boundary_->ReadByte(0x4B7880);
        if(!format)return std::nullopt;
        if(SignedByte(*format)>=4)
        {
            const auto wheel=boundary_->ReadByte(0x4B7884);
            if(!wheel)return std::nullopt;
            // CVT.S.W of signedbyte and multiply120 are exact integral singles.
            WriteTailWordForAnalysis(0x7C,FloatBits(static_cast<float>(120*int(SignedByte(*wheel)))));
        }
        constexpr std::uint32_t lower[2]{0xC3A00000,0xC3600000},upper[2]{0x43A00000,0x43600000};
        for(unsigned axis=0;axis<2;++axis)
        {
            const auto initial=ReadTailWordForAnalysis(0x74+4*axis);
            if(!initial)return std::nullopt;
            const auto low=boundary_->CompareSingleLess(*initial,lower[axis]);
            if(!low)return std::nullopt;
            if(*low)WriteTailWordForAnalysis(0x74+4*axis,lower[axis]);
            const auto reread=ReadTailWordForAnalysis(0x74+4*axis);
            if(!reread)return std::nullopt;
            const auto high=boundary_->CompareSingleLessEqual(*reread,upper[axis]);
            if(!high)return std::nullopt;
            if(!*high)WriteTailWordForAnalysis(0x74+4*axis,upper[axis]);
        }
        for(unsigned bit=0;bit<8;++bit)
        {
            const auto word=ReadTailWordForAnalysis(0x50+4*bit);
            if(!word)return std::nullopt;
            WriteTailWordForAnalysis(0x80+4*bit,*word);
        }
        tail_[0xA0-0x48]=tail_[0x70-0x48];
        for(unsigned axis=0;axis<3;++axis)
        {
            const auto word=ReadTailWordForAnalysis(0x74+4*axis);
            if(!word)return std::nullopt;
            WriteTailWordForAnalysis(0xA4+4*axis,*word);
        }
        return true;
    }
    std::optional<bool> spPS2Mouse::PollForAnalysis(std::string* error)
    {
        auto unresolved=[&]()->std::optional<bool>{if(error)*error="Original PS2 mouse poll payload, semaphore or EE arithmetic is unresolved";return std::nullopt;};
        if(!GetField44ForAnalysis())return true;
        if(!boundary_||!WaitForAnalysis())return unresolved();
        const auto countStatus=CallForAnalysis(1,0x4B7780);
        if(!countStatus)return unresolved();
        if(*countStatus){boundary_->Report(0x45BA00,*countStatus);return false;}
        if(!WaitForAnalysis()||!SignalForAnalysis())return unresolved();
        const auto firstCount=boundary_->ReadByte(0x4B7780);
        if(!firstCount)return unresolved();
        if(!*firstCount)return true;
        std::uint32_t index=0;
        for(;;)
        {
            const auto available=boundary_->ReadByte(0x4B7782+index);
            if(!available)return unresolved();
            if(*available)
            {
                if(!WaitForAnalysis())return unresolved();
                boundary_->WriteWord(abi::PS2MouseRpcSend,index);
                const auto infoStatus=CallForAnalysis(3,0x4B7800);
                if(!infoStatus)return unresolved();
                if(*infoStatus)boundary_->Report(0x45B9D0,*infoStatus);
                else
                {
                    if(!WaitForAnalysis()||!SignalForAnalysis())return unresolved();
                    WriteTailWordForAnalysis(0x7C,0);WriteTailWordForAnalysis(0x74,0);WriteTailWordForAnalysis(0x78,0);
                    for(;;)
                    {
                        if(!WaitForAnalysis())return unresolved();
                        boundary_->WriteWord(abi::PS2MouseRpcSend,index);
                        const auto eventStatus=CallForAnalysis(2,0x4B7880);
                        if(!eventStatus)return unresolved();
                        if(*eventStatus){boundary_->Report(0x45BA20,*eventStatus);break;}
                        if(!WaitForAnalysis()||!SignalForAnalysis())return unresolved();
                        const auto type=boundary_->ReadByte(0x4B7880);
                        if(!type)return unresolved();
                        if(!SignedByte(*type))break;
                        if(!ProcessEventForAnalysis())return unresolved();
                    }
                }
            }
            ++index;
            const auto count=boundary_->ReadByte(0x4B7780); // Live after every device.
            if(!count)return unresolved();
            if(index>=*count)return true;
        }
    }
    std::optional<bool> spPS2Mouse::PhysicalSlot1ForAnalysis(std::uint32_t code) const
    {
        if(!GetField44ForAnalysis()||code<100||code>=108)return false;
        const auto value=ReadTailWordForAnalysis(0x50+4*(code-100));
        if(!value)return std::nullopt;return *value==1;
    }
    std::optional<bool> spPS2Mouse::PhysicalSlot2ForAnalysis(std::uint32_t code) const
    {
        if(!GetField44ForAnalysis()||code<100||code>=108)return false;
        const auto value=ReadTailWordForAnalysis(0xB0+4*(code-100));
        if(!value)return std::nullopt;return *value==1;
    }
    std::optional<std::uint32_t> spPS2Mouse::PhysicalSlot3ForAnalysis(std::uint32_t code) const
    {
        if(!GetField44ForAnalysis()||code<108||code>=110)return 0U;
        const auto value=ReadTailWordForAnalysis(0x74+4*(code-108));
        if(!value||!boundary_)return std::nullopt;
        const auto upper=boundary_->CompareSingleLessEqual(0x4F000000,*value);
        if(!upper)return std::nullopt;
        auto operand=value;
        if(*upper)operand=boundary_->SubtractSingle(*value,0x4F000000);
        if(!operand)return std::nullopt;
        const auto converted=boundary_->ConvertSingleToWord(*operand);
        if(!converted)return std::nullopt;
        return static_cast<std::uint32_t>(*converted)|(*upper?0x80000000U:0U);
    }
    std::optional<std::int32_t> spPS2Mouse::PhysicalSlot4ForAnalysis(std::uint32_t code) const
    {
        if(!GetField44ForAnalysis()||code<108||code>=111)return 0;
        const auto value=ReadTailWordForAnalysis(0x74+4*(code-108));
        if(!value||!boundary_)return std::nullopt;
        return boundary_->ConvertSingleToWord(*value);
    }
    spPS2Mouse::QueriesForAnalysis spPS2Mouse::GetQueriesForAnalysis() const
    {
        QueriesForAnalysis queries;
        queries.slot1=[this](auto code){return Require(PhysicalSlot1ForAnalysis(code));};
        queries.slot2=[this](auto code){return Require(PhysicalSlot2ForAnalysis(code));};
        queries.slot3=[this](auto code){return Require(PhysicalSlot3ForAnalysis(code));};
        queries.slot4=[this](auto code){return Require(PhysicalSlot4ForAnalysis(code));};
        queries.slot5=[this](auto code,auto a,auto b,auto c){PhysicalSlot5ForAnalysis(code,a,b,c);};
        queries.slot6=[this](auto code){return PhysicalSlot6ForAnalysis(code);};
        return queries;
    }
}

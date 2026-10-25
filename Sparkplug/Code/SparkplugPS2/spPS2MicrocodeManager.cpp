#include "spPS2MicrocodeManager.h"

#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        spPS2MicrocodeManager* MicrocodeSingleton = nullptr;
        std::unique_ptr<spBaseObject> CreateMicrocodeManager()
        {
            return std::make_unique<spPS2MicrocodeManager>();
        }
        const spRTTIRecord Record{spPS2MicrocodeManager::ClassID,
            spNamedObject::ClassID, "spPS2MicrocodeManager", &spNamedObject::StaticRTTI(),
            &CreateMicrocodeManager, nullptr};
        const bool Registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(Record);

        bool ContainsQword(const PS2PipelineCommandBufferForAnalysis& buffer,
            std::uint64_t address) noexcept
        {
            if (address > 0xFFFFFFFFULL || address < buffer.baseAddress) return false;
            const auto relative = address - buffer.baseAddress;
            return relative / 4 <= buffer.wordCount
                && buffer.wordCount - static_cast<std::size_t>(relative / 4) >= 4;
        }
        std::uint32_t* Qword(PS2PipelineCommandBufferForAnalysis& buffer,
            std::uint32_t address) noexcept
        {
            return buffer.words + (address - buffer.baseAddress) / 4;
        }
        void StoreTag(std::uint32_t* output, std::uint32_t tag,
            std::uint32_t address = 0) noexcept
        {
            output[0] = tag;
            output[1] = address;
            output[2] = 0;
            output[3] = 0;
        }
    }

    spPS2MicrocodeManager::spPS2MicrocodeManager() noexcept
    {
        // Factory205E10 writes gp-4570 and zeroes native field18.
        MicrocodeSingleton = this;
    }
    spPS2MicrocodeManager::~spPS2MicrocodeManager()
    {
        // Native205CA8 clears unconditionally, including older live instances.
        MicrocodeSingleton = nullptr;
    }
    const spRTTIRecord& spPS2MicrocodeManager::StaticRTTI() noexcept
    {
        (void)Registered;
        return Record;
    }
    const spRTTIRecord& spPS2MicrocodeManager::vfunc_18() const noexcept { return Record; }
    std::unique_ptr<spBaseObject> spPS2MicrocodeManager::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPS2MicrocodeManager>();
        manager.RegisterCloneForAnalysis(*this, *clone);
        // 205D10 calls inherited105DC0: name copy, fresh current packet zero.
        return vfunc_14(*clone, manager) ? std::move(clone) : nullptr;
    }
    spPS2MicrocodeManager* spPS2MicrocodeManager::SingletonForAnalysis() noexcept
    {
        return MicrocodeSingleton;
    }
    std::uint32_t spPS2MicrocodeManager::CurrentPacketForAnalysis() const noexcept
    {
        return currentPacket_;
    }
    bool spPS2MicrocodeManager::AppendPipelineCommandsForAnalysis(
        std::uint32_t packetAddress, PS2PipelineCommandBufferForAnalysis& buffer) noexcept
    {
        const bool changed = currentPacket_ != packetAddress;
        const std::uint64_t lastWrite = std::uint64_t{buffer.writeAddress} + (changed ? 16U : 0U);
        // No partially written packet on a rejected borrowed host span.
        if (!buffer.words || ((buffer.baseAddress | buffer.writeAddress | buffer.tailAddress) & 15U)
            || buffer.tailAddress >= buffer.writeAddress
            || lastWrite + 16U > 0xFFFFFFFFULL
            || !ContainsQword(buffer, buffer.tailAddress)
            || !ContainsQword(buffer, buffer.writeAddress)
            || !ContainsQword(buffer, lastWrite)) return false;

        if (changed)
        {
            // PCPYLD with zero upper64 produces these four stored32-bit words.
            StoreTag(Qword(buffer, buffer.tailAddress), 0x50000000U, packetAddress & 0x7FFFFFFFU);
            buffer.tailAddress = buffer.writeAddress;
            StoreTag(Qword(buffer, buffer.tailAddress), 0x70000000U);
            buffer.writeAddress += 16U;
            currentPacket_ = packetAddress;
        }

        const auto previousWrite = buffer.writeAddress;
        auto* tag = Qword(buffer, buffer.tailAddress);
        StoreTag(tag, 0x10000001U);
        // SUBU/SRL/ADDIU/SH: only low16 of the computed QWC are stored.
        const auto qwc = ((previousWrite - buffer.tailAddress) >> 4) - 1U;
        tag[2] = 0;
        tag[3] = 0x14000000U; // MSCAL micro instruction0, also on a cache hit.
        tag[0] = (tag[0] & 0xFFFF0000U) | (qwc & 0xFFFFU);
        StoreTag(Qword(buffer, previousWrite), 0x70000000U);
        buffer.tailAddress = previousWrite;
        buffer.writeAddress = previousWrite + 16U;
        return true;
    }
}

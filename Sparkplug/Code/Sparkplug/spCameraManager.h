#pragma once

// Portable bounded declaration. Native factory/RTTI and Base inheritance are
// known; the original header, secondary C++ base and registry element type are
// not. Host-final excludes unqualified derived Copy/Clone, not native finality.
#include "../SparkBase/spBaseObject.h"
#include <array>
#include <optional>

namespace sparkplug::reconstruction
{
    struct spCameraManagerCapturedRegistryForAnalysis final
    {
        enum class Profile {Unqualified,PCNonOwningRawWords};
        Profile profile=Profile::Unqualified;
        std::uint32_t field14=0;
        std::optional<std::uint32_t> capturedOpaque18;
        std::size_t count=0;
        std::array<std::uint32_t,8> rawWords{};
    };

    class spCameraManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x33800A37;
        static constexpr std::size_t InitialLogicalWordSlotsForAnalysis=32;
        spCameraManager();
        ~spCameraManager() override;
        spCameraManager(const spCameraManager&)=delete;
        spCameraManager& operator=(const spCameraManager&)=delete;
        spCameraManager(spCameraManager&&)=delete;
        spCameraManager& operator=(spCameraManager&&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        [[nodiscard]] static spCameraManager* GetInstance() noexcept{return instance_;}

        // Supplied postconstruction metadata, never a recovered Add/Load API.
        // Raw words can encode native addresses but are never cast or owned.
        // Caller provides synchronous exclusive use, no allocator callbacks.
        [[nodiscard]] bool ApplyCapturedRegistryForAnalysis(const spCameraManagerCapturedRegistryForAnalysis&);
        [[nodiscard]] std::uint32_t GetField14ForAnalysis() const noexcept{return field14_;}
        [[nodiscard]] const std::optional<std::uint32_t>& GetCapturedOpaque18ForAnalysis() const noexcept{return opaque18_;}
        [[nodiscard]] const std::vector<std::uint32_t>& GetRawRegistryWordsForAnalysis() const noexcept{return words_;}
        // Physical host capacity is >=32; it is not native ABI/allocation size.
        [[nodiscard]] std::size_t GetHostWordCapacityForAnalysis() const noexcept{return words_.capacity();}
        [[nodiscard]] const void* GetHostStorageForAnalysis() const noexcept{return words_.data();}
    private:
        static spCameraManager* instance_;
        std::uint32_t field14_=0;
        std::optional<std::uint32_t> opaque18_;
        std::vector<std::uint32_t> words_;
    };
}

#pragma once

// Class identity and PC constructor/destructor are recovered. The original
// header path, method names and record member types have not been recovered.
#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTextureManager : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x2F434D04;
        static constexpr std::uint32_t NativeMaximumRecordCount = 0x15555555;

        // Native PC reserve relocates each record as three opaque words.
        // Their meanings and a producer/registration operation remain unknown.
        using RecordForAnalysis = std::array<std::uint32_t, 3>;

        ~spTextureManager() override;
        spTextureManager(const spTextureManager&) = delete;
        spTextureManager& operator=(const spTextureManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spTextureManager* GetInstance() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Original primary slots +1C/+20 are pure-call entries in the base
        // table. Their names/roles are unknown; the PC leaf implements no-ops.
        virtual void NativeHook1CForAnalysis() noexcept = 0;
        virtual void NativeHook20ForAnalysis() noexcept = 0;

        // PC005D65C0 writes the raw byte/count before optional vector reserve.
        // false is an explicit HOST failure result; the native method has no
        // recovered success return and may throw on allocation/length failure.
        [[nodiscard]] bool ConfigureReserveForAnalysis(
            std::uint8_t reserveFlag, std::uint32_t recordCount) noexcept;
        [[nodiscard]] std::uint8_t GetReserveFlagForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetRequestedRecordCountForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetRecordCapacityForAnalysis() const noexcept;
        [[nodiscard]] const std::vector<RecordForAnalysis>& GetRecordsForAnalysis() const noexcept;

        // Explicit host input for an already populated opaque record vector.
        // This is NOT a reconstructed insertion/texture registration method.
        // It transfers only storage; words that resemble addresses stay opaque.
        void SetRecordStorageForAnalysis(std::vector<RecordForAnalysis> records) noexcept;

    protected:
        spTextureManager() noexcept;

    private:
        static spTextureManager* instance_;
        std::uint8_t reserveFlag_ = 0;
        std::uint32_t requestedRecordCount_ = 0;
        std::vector<RecordForAnalysis> records_;
    };
}

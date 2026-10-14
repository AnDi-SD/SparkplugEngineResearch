#include "spTextureManager.h"

#include <new>
#include <stdexcept>
#include <utility>

namespace sparkplug::reconstruction
{
    namespace
    {
        const spRTTIRecord TextureManagerRecord{
            spTextureManager::ClassID, spBaseObject::ClassID, "spTextureManager",
            &spBaseObject::StaticRTTI(), nullptr, nullptr,
        };
        const bool TextureManagerRegistered =
            spRTTIManager::Instance().RegisterDeferredForAnalysis(TextureManagerRecord);
    }

    spTextureManager* spTextureManager::instance_ = nullptr;

    spTextureManager::spTextureManager() noexcept
    {
        // PC005D65E0 publishes the whole-object pointer, then clears flag14,
        // count18 and begin/end/capacity20/24/28. It has no instance guard.
        instance_ = this;
    }

    spTextureManager::~spTextureManager()
    {
        // PC005D6540 releases only vector allocation, not its opaque records'
        // possible pointees, then clears the support singleton unconditionally.
        std::vector<RecordForAnalysis>{}.swap(records_);
        instance_ = nullptr;
    }

    const spRTTIRecord& spTextureManager::StaticRTTI() noexcept
    {
        (void)TextureManagerRegistered;
        return TextureManagerRecord;
    }

    spTextureManager* spTextureManager::GetInstance() noexcept
    {
        return instance_;
    }

    const spRTTIRecord& spTextureManager::vfunc_18() const noexcept
    {
        return TextureManagerRecord;
    }

    bool spTextureManager::ConfigureReserveForAnalysis(
        const std::uint8_t reserveFlag, const std::uint32_t recordCount) noexcept
    {
        reserveFlag_ = reserveFlag;
        requestedRecordCount_ = recordCount;
        if (reserveFlag == 0)
        {
            return true;
        }
        // The original 32-bit vector rejects counts greater than 0x15555555
        // before allocation. Preserve that limit on a 64-bit host too.
        if (recordCount > NativeMaximumRecordCount)
        {
            return false;
        }
        try
        {
            records_.reserve(recordCount);
            return true;
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        catch (const std::length_error&)
        {
            return false;
        }
    }

    std::uint8_t spTextureManager::GetReserveFlagForAnalysis() const noexcept
    {
        return reserveFlag_;
    }

    std::uint32_t spTextureManager::GetRequestedRecordCountForAnalysis() const noexcept
    {
        return requestedRecordCount_;
    }

    std::size_t spTextureManager::GetRecordCapacityForAnalysis() const noexcept
    {
        return records_.capacity();
    }

    const std::vector<spTextureManager::RecordForAnalysis>&
    spTextureManager::GetRecordsForAnalysis() const noexcept
    {
        return records_;
    }

    void spTextureManager::SetRecordStorageForAnalysis(
        std::vector<RecordForAnalysis> records) noexcept
    {
        records_ = std::move(records);
    }
}

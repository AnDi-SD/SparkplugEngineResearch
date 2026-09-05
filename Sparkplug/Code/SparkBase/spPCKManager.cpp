#include "spPCKManager.h"

#include "spStream.h"
#if defined(_WIN32)
#include "../SparkBasePC/spPCFileStream.h"
#endif

#include <algorithm>
#include <cstring>
#include <limits>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreatePCKManager()
        {
            return std::make_unique<spPCKManager>();
        }

        const spRTTIRecord PCKManagerRecord{
            spPCKManager::ClassID,
            spBaseObject::ClassID,
            "spPCKManager",
            &spBaseObject::StaticRTTI(),
            &CreatePCKManager,
            nullptr,
        };

        [[nodiscard]] std::uint32_t ReadLittleEndian32(
            const std::uint8_t* bytes) noexcept
        {
            return static_cast<std::uint32_t>(bytes[0])
                | (static_cast<std::uint32_t>(bytes[1]) << 8U)
                | (static_cast<std::uint32_t>(bytes[2]) << 16U)
                | (static_cast<std::uint32_t>(bytes[3]) << 24U);
        }

        [[nodiscard]] bool TryReadString(
            const std::uint8_t* block,
            const std::size_t blockSize,
            const std::uint32_t offset,
            std::string& result)
        {
            if (offset >= blockSize)
            {
                return false;
            }

            const auto* first = reinterpret_cast<const char*>(block + offset);
            const auto* terminator = static_cast<const char*>(
                std::memchr(first, '\0', blockSize - offset));
            if (terminator == nullptr)
            {
                return false;
            }

            result.assign(first, terminator);
            return true;
        }

        [[nodiscard]] std::string NormalizeResourceName(const char* resourceName)
        {
            std::string normalized = resourceName == nullptr ? "" : resourceName;
            if (normalized.size() >= 2
                && normalized[0] == '.'
                && (normalized[1] == '/' || normalized[1] == '\\'))
            {
                normalized.erase(0, 2);
            }

            for (auto& character : normalized)
            {
                if (character == '\\')
                {
                    character = '/';
                }
                else if (character >= 'A' && character <= 'Z')
                {
                    character = static_cast<char>(character + ('a' - 'A'));
                }
            }
            return normalized;
        }
    }

    spPCKManager* spPCKManager::instance_ = nullptr;

    spPCKManager::spPCKManager() noexcept
    {
        // Both native constructors reserve 30 package entries and publish the
        // complete object through a process-global pointer.
        packages_.reserve(30);
        instance_ = this;
    }

    spPCKManager::~spPCKManager()
    {
        // Native singleton-support destructors clear the global
        // unconditionally.  std::vector deliberately fixes the native PS2
        // destructor's apparent half-removal leak.
        instance_ = nullptr;
    }

    const spRTTIRecord& spPCKManager::StaticRTTI() noexcept
    {
        return PCKManagerRecord;
    }

    spPCKManager* spPCKManager::GetInstance() noexcept
    {
        return instance_;
    }

    std::unique_ptr<spBaseObject> spPCKManager::vfunc_10(
        spCloneManager& manager) const
    {
        auto clone = std::make_unique<spPCKManager>();
        manager.RegisterClone(*this, *clone);
        if (!vfunc_14(*clone, manager))
        {
            return nullptr;
        }
        return clone;
    }

    bool spPCKManager::vfunc_14(
        spBaseObject& destination,
        spCloneManager& manager) const
    {
        // The native class routes this slot to the empty spBaseObject copy
        // implementation, so mounted packages are intentionally not cloned.
        return destination.IsKindOf(ClassID)
            && spBaseObject::vfunc_14(destination, manager);
    }

    const spRTTIRecord& spPCKManager::vfunc_18() const noexcept
    {
        return PCKManagerRecord;
    }

    bool spPCKManager::vfunc_AddPackage(
        const char* packageName,
        const std::uint32_t priority)
    {
        if (packageName == nullptr)
        {
            return false;
        }

        if (std::any_of(packages_.begin(), packages_.end(),
                [packageName](const auto& package)
                {
                    return package.packageName == packageName;
                }))
        {
            // Both native implementations treat an exact duplicate as an
            // already-satisfied request.
            return true;
        }

#if defined(_WIN32)
        spPCFileStream source;
        if (!source.Open(packageName))
        {
            return false;
        }
        const bool result = ReadAndAddPackage(packageName, priority, source);
        (void)source.Close();
        return result;
#else
        // A native spPS2FileStream reconstruction is intentionally not faked
        // by host filesystem I/O.  Tests use AddPackageImage for this common
        // parser until that platform leaf is runnable.
        (void)priority;
        return false;
#endif
    }

    bool spPCKManager::vfunc_Resolve(
        const char* resourceName,
        char* packageName,
        std::uint32_t* packagePhysicalSize,
        std::uint32_t* packagePhysicalOrigin,
        std::uint32_t* logicalSector,
        std::uint32_t* byteOffset,
        std::uint32_t* byteCount) const
    {
        if (resourceName == nullptr || packageName == nullptr
            || packagePhysicalSize == nullptr || packagePhysicalOrigin == nullptr
            || logicalSector == nullptr || byteOffset == nullptr
            || byteCount == nullptr)
        {
            return false;
        }

        const auto normalized = NormalizeResourceName(resourceName);
        const auto separator = normalized.rfind('/');
        const auto directory = separator == std::string::npos
            ? std::string{}
            : normalized.substr(0, separator);
        const auto fileName = separator == std::string::npos
            ? normalized
            : normalized.substr(separator + 1);

        for (const auto& package : packages_)
        {
            const auto first = std::lower_bound(
                package.files.begin(), package.files.end(), fileName,
                [](const spPCKFileRecord& record, const std::string& candidate)
                {
                    return record.fileName < candidate;
                });

            for (auto current = first;
                 current != package.files.end() && current->fileName == fileName;
                 ++current)
            {
                if (current->directory != directory)
                {
                    continue;
                }

                std::memcpy(packageName, package.packageName.c_str(),
                    package.packageName.size() + 1);
                *packagePhysicalSize = package.physicalSize;
                *packagePhysicalOrigin = package.physicalOrigin;
                *logicalSector = current->logicalSector;
                *byteOffset = current->byteOffset;
                *byteCount = current->byteCount;
                return true;
            }
        }

        return false;
    }

    bool spPCKManager::AddPackageImage(
        const char* packageName,
        const std::uint32_t priority,
        const std::uint8_t* image,
        const std::size_t imageSize,
        const std::uint32_t physicalOrigin)
    {
        if (packageName == nullptr || image == nullptr
            || imageSize > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        if (std::any_of(packages_.begin(), packages_.end(),
                [packageName](const auto& package)
                {
                    return package.packageName == packageName;
                }))
        {
            return true;
        }

        spPCKPackageRecord package;
        package.packageName = packageName;
        package.priority = priority;
        package.physicalOrigin = physicalOrigin;
        package.physicalSize = static_cast<std::uint32_t>(imageSize);
        if (!ParsePackageImage(image, imageSize, package))
        {
            return false;
        }

        const auto insertion = std::find_if(packages_.begin(), packages_.end(),
            [priority](const auto& existing)
            {
                // Native code inserts before the first equal-priority entry.
                return existing.priority <= priority;
            });
        packages_.insert(insertion, std::move(package));
        return true;
    }

    bool spPCKManager::RemovePackage(const char* packageName) noexcept
    {
        if (packageName == nullptr)
        {
            return false;
        }

        const auto found = std::find_if(packages_.begin(), packages_.end(),
            [packageName](const auto& package)
            {
                return package.packageName == packageName;
            });
        if (found == packages_.end())
        {
            return false;
        }

        // sub_00113500/PC counterpart replace the removed entry with the last
        // one instead of preserving priority order.
        if (found != packages_.end() - 1)
        {
            *found = std::move(packages_.back());
        }
        packages_.pop_back();
        return true;
    }

    std::size_t spPCKManager::GetPackageCount() const noexcept
    {
        return packages_.size();
    }

    const spPCKPackageRecord* spPCKManager::GetPackage(
        const std::size_t index) const noexcept
    {
        return index < packages_.size() ? &packages_[index] : nullptr;
    }

    bool spPCKManager::ReadAndAddPackage(
        const char* packageName,
        const std::uint32_t priority,
        spStream& source)
    {
        std::uint32_t size = 0;
        if (!source.GetSize(&size))
        {
            return false;
        }

        std::vector<std::uint8_t> image(size);
        if (size != 0 && !source.ReadData(image.data(), size))
        {
            return false;
        }
        return AddPackageImage(packageName, priority, image.data(), image.size());
    }

    bool spPCKManager::ParsePackageImage(
        const std::uint8_t* image,
        const std::size_t imageSize,
        spPCKPackageRecord& package)
    {
        if (imageSize < 8)
        {
            return false;
        }

        const auto stringBytes = ReadLittleEndian32(image);
        const auto stringEnd = std::size_t{4} + stringBytes;
        if (stringEnd > imageSize || imageSize - stringEnd < 4)
        {
            return false;
        }

        const auto fileCount = ReadLittleEndian32(image + stringEnd);
        const auto recordsStart = stringEnd + 4;
        constexpr std::size_t RecordSize = 0x14;
        if (fileCount > (imageSize - recordsStart) / RecordSize)
        {
            return false;
        }

        package.files.clear();
        package.files.reserve(fileCount);
        const auto* strings = image + 4;
        for (std::uint32_t index = 0; index < fileCount; ++index)
        {
            const auto* serialized = image + recordsStart + index * RecordSize;
            spPCKFileRecord record;
            if (!TryReadString(strings, stringBytes,
                    ReadLittleEndian32(serialized), record.directory)
                || !TryReadString(strings, stringBytes,
                    ReadLittleEndian32(serialized + 4), record.fileName))
            {
                return false;
            }

            record.logicalSector = ReadLittleEndian32(serialized + 8);
            record.byteOffset = ReadLittleEndian32(serialized + 0x0C);
            record.byteCount = ReadLittleEndian32(serialized + 0x10);
            if (record.logicalSector != record.byteOffset / 0x800U
                || record.byteOffset > imageSize
                || record.byteCount > imageSize - record.byteOffset)
            {
                return false;
            }

            if (!package.files.empty()
                && package.files.back().fileName > record.fileName)
            {
                // The native binary-search resolver assumes this ordering.
                return false;
            }
            package.files.push_back(std::move(record));
        }

        return true;
    }
}

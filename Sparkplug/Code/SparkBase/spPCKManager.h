#pragma once

// Inferred header path.  The class name and its RTTI identity are exact, but
// neither shipped executable preserves an original source/header path.  The
// method names below describe the recovered behavior and are not claimed as
// original spellings.

#include "spBaseObject.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    class spStream;

    struct spPCKFileRecord final
    {
        std::string directory;
        std::string fileName;
        std::uint32_t logicalSector = 0;
        std::uint32_t byteOffset = 0;
        std::uint32_t byteCount = 0;
    };

    struct spPCKPackageRecord final
    {
        std::string packageName;
        std::uint32_t priority = 0;
        std::vector<spPCKFileRecord> files;
        std::uint32_t physicalOrigin = 0;
        std::uint32_t physicalSize = 0;
    };

    // Common PC/PS2 package index and resource resolver.  Host storage is
    // deliberately idiomatic C++; byte-exact native layouts remain in the
    // Analysis platform headers.
    class spPCKManager : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x2B9D1649;

        spPCKManager() noexcept;
        ~spPCKManager() override;

        spPCKManager(const spPCKManager&) = delete;
        spPCKManager& operator=(const spPCKManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spPCKManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // First class-local virtual slot.  Both native leaves open the package
        // through the platform file-stream factory and then read the common
        // index format.  The portable implementation uses the reconstructed
        // PC stream when it is available.
        [[nodiscard]] virtual bool vfunc_AddPackage(
            const char* packageName,
            std::uint32_t priority);

        // Second class-local virtual slot.  This seven-argument signature and
        // every output are proved independently by PC and PS2 callers.
        [[nodiscard]] virtual bool vfunc_Resolve(
            const char* resourceName,
            char* packageName,
            std::uint32_t* packagePhysicalSize,
            std::uint32_t* packagePhysicalOrigin,
            std::uint32_t* logicalSector,
            std::uint32_t* byteOffset,
            std::uint32_t* byteCount) const;

        // Analytical safe seam used to test the common parser without a
        // platform file backend.  It consumes exactly the bytes that the
        // native AddPackage method reads from spFileStream.
        [[nodiscard]] bool AddPackageImage(
            const char* packageName,
            std::uint32_t priority,
            const std::uint8_t* image,
            std::size_t imageSize,
            std::uint32_t physicalOrigin = 0);

        [[nodiscard]] bool RemovePackage(const char* packageName) noexcept;
        [[nodiscard]] std::size_t GetPackageCount() const noexcept;
        [[nodiscard]] const spPCKPackageRecord* GetPackage(
            std::size_t index) const noexcept;

    private:
        [[nodiscard]] bool ReadAndAddPackage(
            const char* packageName,
            std::uint32_t priority,
            spStream& source);
        [[nodiscard]] static bool ParsePackageImage(
            const std::uint8_t* image,
            std::size_t imageSize,
            spPCKPackageRecord& package);

        static spPCKManager* instance_;
        std::vector<spPCKPackageRecord> packages_;
    };
}

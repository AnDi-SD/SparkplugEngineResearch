#pragma once

// Exact original translation-unit path recovered from the PC executable:
// Z:\Sparkplug\Code\Sparkplug\spSerializerManager.cpp
// The separate header path and original declarations are not present in the
// shipped binaries. Names ending in ForAnalysis are therefore explicit host
// seams around behavior recovered from PC and PS2, not claimed spellings.

#include "spSerializer.h"
#include "../SparkBase/spStream.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>

namespace sparkplug::reconstruction
{
    class spResourceFATHelperForAnalysis;
    // LoadSceneGraph reads exactly these seven words before asking the FAT to
    // consume its own index. The ObjectCount word at file offset 0x1C belongs
    // to that following index and is deliberately not part of this structure.
    struct spSerializerFileHeader final
    {
        std::uint32_t signature = 0;
        std::uint32_t version = 0;
        std::uint32_t exportTag = 0;
        std::uint32_t declaredFileSize = 0;
        std::uint32_t platformMask = 0;
        std::uint32_t dataOffset = 0;
        std::uint32_t dataSize = 0;
    };

    static_assert(sizeof(spSerializerFileHeader) == 0x1C);

    enum class spSerializerFileHeaderStatus : std::uint32_t
    {
        Valid,
        HeaderReadFailed,
        StreamSizeUnavailable,
        WrongFileType,
        WrongVersion,
        UnsupportedPlatform,
        DataOffsetBeyondEnd,
    };

    class spSerializerManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0xE422E9EB;
        static constexpr std::uint32_t FileSignature = 0x53504646;
        static constexpr std::uint32_t FileVersion = 0x26;

        static constexpr std::uint32_t PlatformCommon = 0x01;
        static constexpr std::uint32_t PlatformPC = 0x02;
        static constexpr std::uint32_t PlatformPS2 = 0x08;
        static constexpr std::uint32_t PlatformAll = 0xFF;
        static constexpr std::uint32_t OperationLoad = 0x01;
        static constexpr std::uint32_t OperationSave = 0x02;
        static constexpr std::uint32_t OperationBoth =
            OperationLoad | OperationSave;

        spSerializerManager() noexcept;
        ~spSerializerManager() override;

        spSerializerManager(const spSerializerManager&) = delete;
        spSerializerManager& operator=(const spSerializerManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spSerializerManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Native sub_00182070/PC counterpart appends a 0x18-byte list node.
        // Multiple nodes may alias one serializer; native teardown deletes the
        // instance once and erases every node carrying that pointer. shared_ptr
        // expresses this grouped ownership without reproducing raw-pointer risk.
        [[nodiscard]] bool RegisterForAnalysis(
            spClassID targetClassID,
            std::shared_ptr<spSerializer> serializer,
            std::uint32_t platformMask,
            std::uint32_t operationMask);

        // Native lookup is stable and first-match: duplicate target IDs are
        // expected for platform and direction-specific implementations.
        [[nodiscard]] spSerializer* FindForAnalysis(
            spClassID targetClassID) const noexcept;
        [[nodiscard]] spSerializer* FindForAnalysis(
            const spBaseObject& object) const noexcept;

        void SetDispatchContextForAnalysis(
            std::uint32_t platformMask,
            std::uint32_t operationMask) noexcept;
        [[nodiscard]] std::uint32_t GetPlatformMaskForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetOperationMaskForAnalysis() const noexcept;
        [[nodiscard]] std::uint32_t GetSerializationPolicyForAnalysis() const noexcept;
        [[nodiscard]] std::size_t GetRegistrationCountForAnalysis() const noexcept;
        [[nodiscard]] spResourceFATHelperForAnalysis* GetFATForAnalysis() noexcept;
        [[nodiscard]] const spResourceFATHelperForAnalysis*
            GetFATForAnalysis() const noexcept;

        [[nodiscard]] static spSerializerFileHeaderStatus
        ValidateFileHeaderForAnalysis(
            const spSerializerFileHeader& header,
            std::uint32_t streamSize,
            std::uint32_t nativePlatformMask) noexcept;

        // Safe reconstruction of the common front of both native load paths:
        // read 0x1C bytes, query stream size, validate, then install the file's
        // platform mask and load direction for serializer dispatch. Loading the
        // owned FAT and materializing objects remain separate explicit stages.
        [[nodiscard]] spSerializerFileHeaderStatus
        ReadAndValidateHeaderForAnalysis(
            spStream& source,
            std::uint32_t nativePlatformMask,
            spSerializerFileHeader* header = nullptr);

    private:
        struct Registration final
        {
            spClassID targetClassID = 0;
            std::uint32_t platformMask = 0;
            std::uint32_t operationMask = 0;
            std::shared_ptr<spSerializer> serializer;
        };

        static spSerializerManager* instance_;
        void ClearRegistrationsInNativeOrder() noexcept;
        std::uint32_t platformMask_ = 0;
        std::uint32_t operationMask_ = OperationLoad;
        // Both constructors write literal 2. PS2 mesh-writer consumers test
        // this field for 0 or 2 before emitting optional/default fields. The
        // original enum and the meanings of the remaining values are unknown.
        std::uint32_t serializationPolicy_ = 2;
        std::list<Registration> registrations_;
        std::unique_ptr<spResourceFATHelperForAnalysis> fat_;
    };
}

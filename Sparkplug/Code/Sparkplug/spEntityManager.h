#pragma once

// Inferred header/TU path. The class identity and container operations are
// executable-backed; the public spellings below remain analytical.

#include "../SparkBase/spBaseObject.h"

#include <cstddef>
#include <list>
#include <memory>

namespace sparkplug::reconstruction
{
    class spEntityManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x48A15BCB;

        spEntityManager() noexcept;
        ~spEntityManager() override;

        spEntityManager(const spEntityManager&) = delete;
        spEntityManager& operator=(const spEntityManager&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spEntityManager* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Portable ownership-safe facades for native add/remove/clear slots.
        [[nodiscard]] bool AddForAnalysis(std::unique_ptr<spBaseObject> entity);
        [[nodiscard]] std::unique_ptr<spBaseObject> RemoveForAnalysis(
            const spBaseObject& entity);
        void ClearForAnalysis() noexcept;
        void DispatchForAnalysis(const void* notification) noexcept;

        [[nodiscard]] std::size_t GetEntityCountForAnalysis() const noexcept;
        [[nodiscard]] bool ContainsForAnalysis(const spBaseObject& entity) const noexcept;

    private:
        static spEntityManager* instance_;
        std::list<std::unique_ptr<spBaseObject>> entities_;
    };
}

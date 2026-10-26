#pragma once

#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/spEntityHost.h"

#include <cstdint>
#include <memory>

namespace sparkplug::reconstruction
{
    class spEntity : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x22875AA1;

        // Native constructor takes field20 explicitly; the registered Entity
        // factory supplies 3, while GUIObject supplies 1 on both platforms.
        explicit spEntity(spEntityHost& host, std::uint32_t initialField20 = 3);
        ~spEntity() override;
        spEntity(const spEntity&) = delete;
        spEntity& operator=(const spEntity&) = delete;

        static void SetFactoryHostForAnalysis(spEntityHost* host) noexcept;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;

        // Native virtual slot 9, PC 419D00 / PS2 14E150.
        virtual void SetReferenceForAnalysis(void* next);
        [[nodiscard]] void* GetReferenceForAnalysis() const noexcept { return reference18_; }
        [[nodiscard]] std::uint32_t GetField20ForAnalysis() const noexcept
        { return field20_; }

    protected:
        [[nodiscard]] spEntityHost& HostForAnalysis() const noexcept { return host_; }

    private:
        spEntityHost& host_;
        void* reference18_ = nullptr;
        std::uint32_t field20_ = 3;
    };
}

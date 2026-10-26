#pragma once

// Recovered PC/PS2 class identity and bounded operations. Original source-level
// method names/header path remain unknown; portable storage is not native ABI.
#include "spEntity.h"
#include "Analysis/Host/spGUIHost.h"
#include <utility>
#include <vector>

namespace sparkplug::reconstruction
{
    class spGUIObject : public spEntity
    {
    public:
        static constexpr spClassID ClassID = 0x7339573D;
        explicit spGUIObject(spGUIHost& host);
        ~spGUIObject() override;
        static void SetFactoryHostForAnalysis(spGUIHost*) noexcept;
        [[nodiscard]] static spGUIHost* GetFactoryHostForAnalysis() noexcept;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;

        // PC4294B0. Saved collision groups append on every bind, including the
        // same Node. Rebinding restores from the beginning of that history.
        void SetReferenceForAnalysis(void* next) override;
        [[nodiscard]] spNode* GetNodeForGUIForAnalysis() const noexcept;
        // PC4290B0. Both raw-enabled branches write collision group4 in
        // the first recursive substring match for "GUICollision".
        virtual void SetEnabledForAnalysis(std::uint8_t value);
        [[nodiscard]] std::uint8_t GetEnabledByteForAnalysis() const noexcept { return enabled_; }
        [[nodiscard]] const std::vector<std::uint32_t>& GetSavedCollisionGroupsForAnalysis() const noexcept { return saved_; }

        // Explicit host fixture input, not a recovered setter or restore call.
        void SetSavedCollisionGroupsForAnalysis(std::vector<std::uint32_t> value) { saved_=std::move(value); }
        void SetEnabledByteForAnalysis(std::uint8_t value) noexcept { enabled_=value; }
        // The base protocol only handles18 and1C and ignores other codes.
        // Derived native protocols are not substituted by this base helper.
        void NotifyBaseForAnalysis(const spGUINotificationForAnalysis&);

    protected:
        [[nodiscard]] spGUIHost& GUIHostForAnalysis() const noexcept;
        void StoreEnabledByteForAnalysis(std::uint8_t value) noexcept { enabled_=value; }
        void CopyGUIStateToForAnalysis(spGUIObject&) const;

    private:
        std::uint8_t enabled_ = 1;
        std::vector<std::uint32_t> saved_;
    };
}

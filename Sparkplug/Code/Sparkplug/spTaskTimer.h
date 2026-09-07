#pragma once

// Original PC class name/ID. Source/header placement and API spellings inferred.
// Native3C timer is not a spTimer subclass; its RTTI directly names spBaseObject.
#include "../SparkBase/spBaseObject.h"
#include <functional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTaskTimer : public spBaseObject
    {
      public:
        static constexpr spClassID ClassID = 0x1ACD36E2;
        struct StateForAnalysis
        {
            bool active = false;
            bool relative = true;
            std::uint32_t currentMilliseconds = 0;
            std::uint32_t startMilliseconds = 0;
            std::uint32_t pausedMilliseconds = 0;
            float deltaSeconds = 0;
        };
        struct ClockReadingForAnalysis
        {
            std::uint32_t rawTicks;
            std::uint32_t divisor;
        };
        // Explicit global75F65C refresh/read boundary. No host clock/OS calls;
        // a provider returns original75F670/74E050 after any required refresh.
        using ClockSourceForAnalysis = std::function<ClockReadingForAnalysis()>;

        explicit spTaskTimer(spTaskTimer* borrowedSource = nullptr) noexcept;
        ~spTaskTimer() override = default;
        spTaskTimer(const spTaskTimer&) = delete;
        spTaskTimer& operator=(const spTaskTimer&) = delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;

        [[nodiscard]] virtual bool UpdateForAnalysis(const ClockSourceForAnalysis&,
                                                     std::string* error = nullptr);
        [[nodiscard]] virtual bool StartForAnalysis(const ClockSourceForAnalysis&,
                                                    std::string* error = nullptr);
        virtual void PauseForAnalysis() noexcept;
        virtual void ResetForAnalysis() noexcept;
        [[nodiscard]] const StateForAnalysis& GetStateForAnalysis() const noexcept
        {
            return state_;
        }
        void SetStateForAnalysis(const StateForAnalysis& value) noexcept
        {
            state_ = value;
        }
        [[nodiscard]] spTaskTimer* GetSourceForAnalysis() const noexcept
        {
            return source_;
        }

        // Literal non-owning child-list seam, not a recovered native Attach.
        // Source and children must outlive use; native destructor neither
        // deletes nor detaches them. Host preflight rejects cycles/duplicates,
        // null children, >4096 visits and depth>128. No thread/reentry guarantee.
        [[nodiscard]] bool SetChildrenForAnalysis(std::vector<spTaskTimer*> children,
                                                  std::string* error = nullptr);
        // Proven inline append in core41C300: append in order and replace
        // child's borrowed clock source with this timer. Unlike the native
        // unchecked operation, validates the resulting host child graph.
        // Caller must ensure child is not a member of another parent's list.
        [[nodiscard]] bool AppendClockChildForAnalysis(spTaskTimer& child,
                                                       std::string* error = nullptr);
        [[nodiscard]] const std::vector<spTaskTimer*>& GetChildrenForAnalysis() const noexcept
        {
            return children_;
        }

      private:
        [[nodiscard]] bool ValidateTreeForAnalysis(std::string*) const;
        StateForAnalysis state_{};
        spTaskTimer* source_ = nullptr;
        std::vector<spTaskTimer*> children_;
    };
} // namespace sparkplug::reconstruction

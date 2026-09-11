#pragma once

// Inferred declaration path. The native class name and its position between
// spNamedObject and spModel are present in both shipped executables; no
// original header/translation-unit path has been recovered.

#include "Code/SparkBase/spBaseObject.h"

#include <cstdint>
#include <array>
#include <memory>
#include <vector>

namespace sparkplug::reconstruction
{
    class spCamera;

    class spRenderable : public spNamedObject
    {
      public:
        static constexpr spClassID ClassID = 0x4FDA4542;
        using BoundingSphere = std::array<float, 4>;
        using BoundsPosition = std::array<float, 3>;
        enum class CallbackPhaseForAnalysis
        {
            Pre,
            Post
        };
        // Original typedef names are unknown. PC cdecl signatures/low-byte
        // direct result versus full-word grouped result are independently run.
        using DirectCallbackForAnalysis = std::int32_t (*)(spRenderable*, spCamera*, void*);
        using GroupCallbackForAnalysis = std::int32_t (*)(spRenderable*, spCamera*, void*,
                                                          std::uint32_t, void*);

        spRenderable() noexcept = default;
        ~spRenderable() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // Native spRenderable has no RTTI factory and its clone slot is null.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        // Portable ownership facade. Concrete spMaterialData/spFog now exist,
        // but the common native relationship accepts their abstract families;
        // keeping spBaseObject here avoids inventing an unproved C++ signature.
        void SetMaterialForAnalysis(std::shared_ptr<spBaseObject> material) noexcept;
        void SetFogForAnalysis(std::shared_ptr<spBaseObject> fog) noexcept;
        [[nodiscard]] const std::shared_ptr<spBaseObject>& GetMaterialForAnalysis() const noexcept;
        [[nodiscard]] const std::shared_ptr<spBaseObject>& GetFogForAnalysis() const noexcept;

        void SetAlphaSortEnabledForAnalysis(bool enabled) noexcept;
        [[nodiscard]] bool IsAlphaSortEnabledForAnalysis() const noexcept;
        // PC423FD0 material/pass/object portion of the alpha routing gate.
        // Renderer flushing/sort flags and queue ownership remain with caller.
        [[nodiscard]] virtual bool RequiresPCAlphaQueueForAnalysis() const noexcept;
        void SetPriorityForAnalysis(std::uint32_t priority) noexcept;
        [[nodiscard]] std::uint32_t GetPriorityForAnalysis() const noexcept;

        // Native ctor obtains this DWORD from DebugManager41D4E0; the host
        // default0 is explicit cold state, not a claimed native palette default.
        void SetField28ForAnalysis(std::uint32_t value) noexcept;
        [[nodiscard]] std::uint32_t GetField28ForAnalysis() const noexcept;
        bool SetDirectCallbackForAnalysis(CallbackPhaseForAnalysis phase,
                                          DirectCallbackForAnalysis callback) noexcept;
        bool AddGroupCallbackForAnalysis(CallbackPhaseForAnalysis phase,
                                         GroupCallbackForAnalysis callback, void* user);
        bool ClearGroupCallbacksForAnalysis(CallbackPhaseForAnalysis phase) noexcept;
        void SetGroupCallbacksEnabledForAnalysis(CallbackPhaseForAnalysis phase,
                                                 bool enabled) noexcept;
        [[nodiscard]] std::size_t GetGroupCallbackCountForAnalysis(
            CallbackPhaseForAnalysis phase) const noexcept;
        // Callback phase only, NOT full pre/post material state or GPU draw.
        // A group stop does not cancel the direct callback; -1 stable-erases,
        // ordinal still increments. Reentry/vector mutation is host-guarded.
        bool DispatchCallbackPhaseForAnalysis(CallbackPhaseForAnalysis phase, spCamera* camera,
                                              void* support);

        // PC callable slots2C/30. Base sphere is the shared zero sphere;
        // base extents are sphere center +/- radius, not abstract stubs.
        [[nodiscard]] virtual const BoundingSphere& GetBoundingSphereForAnalysis() const noexcept;
        virtual void GetBoundsForAnalysis(BoundsPosition& minimum,
                                          BoundsPosition& maximum) const noexcept;

      protected:
        // Base423B60 zeros14, but PC spModel overrides this with48EAA0 no-op.
        virtual void InvalidateRuntimeModeForAnalysis() noexcept;
        [[nodiscard]] std::uint32_t GetRuntimeModeForAnalysis() const noexcept;

      private:
        struct CallbackRecordForAnalysis
        {
            GroupCallbackForAnalysis callback;
            void* user;
        };
        struct CallbackGroupForAnalysis
        {
            DirectCallbackForAnalysis direct = nullptr;
            std::vector<CallbackRecordForAnalysis> records;
            bool enabled = false;
        };
        [[nodiscard]] static std::size_t CallbackIndex(CallbackPhaseForAnalysis phase) noexcept;
        std::array<CallbackGroupForAnalysis, 2> callbackGroups_;
        bool dispatchActive_ = false;
        std::uint32_t field28_ = 0;
        std::uint32_t runtimeMode_ = 0;
        bool alphaSortEnabled_ = true;
        std::uint32_t priority_ = 0;
        std::shared_ptr<spBaseObject> material_;
        std::shared_ptr<spBaseObject> fog_;
    };
} // namespace sparkplug::reconstruction

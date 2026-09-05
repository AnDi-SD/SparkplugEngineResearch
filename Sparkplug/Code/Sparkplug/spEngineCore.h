#pragma once

// The translation-unit path is exact on PC:
// Z:\Sparkplug\Code\Sparkplug\spEngineCore.cpp.  No original header path or
// declarations survive.  Names ending in ForAnalysis are explicit portable
// seams and are not claimed as original API spellings.

#include "../SparkBase/spBaseObject.h"

#include <array>
#include <cstddef>
#include <memory>

namespace sparkplug::reconstruction
{
    class spEngineCore : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x0E9F6B8C;
        static constexpr std::size_t InitializationStageCount = 4;

        using AnalysisCallback = bool (*)(void* context);

        struct AnalysisStage final
        {
            AnalysisCallback callback = nullptr;
            void* context = nullptr;
        };

        using AnalysisStages = std::array<AnalysisStage, InitializationStageCount>;

        spEngineCore() noexcept;
        ~spEngineCore() override;

        spEngineCore(const spEngineCore&) = delete;
        spEngineCore& operator=(const spEngineCore&) = delete;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] static spEngineCore* GetInstance() noexcept;

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        [[nodiscard]] bool IsInitializedForAnalysis() const noexcept;

        // Both native main-initialization paths short-circuit four consecutive
        // virtual stages and set the byte flag only after all four succeed.
        // Global manager creation before those stages is outside this isolated
        // reconstruction and therefore supplied as injectable callbacks.
        [[nodiscard]] bool InitializeForAnalysis(const AnalysisStages& stages);
        void ShutdownForAnalysis() noexcept;

        // Native constructors initialize two nullable no-argument callbacks.
        // Their source names are unknown; PC uses +0x30/+0x34 and PS2 uses
        // +0x2c/+0x30 because the preceding container ABI differs.
        void SetFrameCallbacksForAnalysis(
            AnalysisCallback first,
            void* firstContext,
            AnalysisCallback second,
            void* secondContext) noexcept;
        [[nodiscard]] bool InvokeFirstFrameCallbackForAnalysis() const;
        [[nodiscard]] bool InvokeSecondFrameCallbackForAnalysis() const;

    private:
        static spEngineCore* instance_;

        bool initialized_ = false;
        AnalysisStage firstFrameCallback_{};
        AnalysisStage secondFrameCallback_{};
    };
}

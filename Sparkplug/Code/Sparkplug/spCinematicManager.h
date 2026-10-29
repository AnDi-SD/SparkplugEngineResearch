#pragma once

// Explicit captured ordered-registry profile. Original construction, secondary
// base, embedded Timer, matrices, factory and global lifetime remain open.
#include "../SparkBase/spBaseObject.h"
#include <array>

namespace sparkplug::reconstruction
{
    class spCinematic;
    class spCinematicCapturedOwnerContextForAnalysis;
    struct spCinematicCapturedOwnerStateForAnalysis;

    class spCinematicManager final : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID=0x48E66610;
        static constexpr std::size_t MaximumCapturedCinematicsForAnalysis=4;
        explicit spCinematicManager(spCinematicCapturedOwnerContextForAnalysis&);
        ~spCinematicManager() override;
        spCinematicManager(const spCinematicManager&)=delete;
        spCinematicManager& operator=(const spCinematicManager&)=delete;
        spCinematicManager(spCinematicManager&&)=delete;
        spCinematicManager& operator=(spCinematicManager&&)=delete;
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        // Primary0C is the existing literal Base Copy40ECE0; it does not copy
        // the registry, construct a native Manager or qualify Manager Clone.

        // PC453020: admitted miss=false/hit=true (native AL only). Admission
        // failure throws before effects. Context/external Name owners outlive
        // this manager; synchronous exclusive access/no callbacks or reentry.
        [[nodiscard]] bool RemoveByNamePcForAnalysis(std::string_view name);
        [[nodiscard]] std::size_t GetCinematicCountForAnalysis() const noexcept{return count_;}
        // Borrowed identity. During a passive free observer this can be a
        // destroyed payload's opaque address before unlink; never dereference.
        [[nodiscard]] const spCinematic* GetCinematicForAnalysis(std::size_t index) const noexcept;
        [[nodiscard]] bool IsContextLiveForAnalysis() const noexcept{return !context_.expired();}

    private:
        void ValidateForOperation(const std::shared_ptr<spCinematicCapturedOwnerStateForAnalysis>&) const;
        std::weak_ptr<spCinematicCapturedOwnerStateForAnalysis> context_;
        std::array<spCinematic*,MaximumCapturedCinematicsForAnalysis> registry_{};
        std::size_t count_=0;
        bool removing_=false; // Host safety only, no native field claim.
    };
}

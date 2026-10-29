#pragma once

// Our captured owner adapter. Native Manager teardown owns list nodes only;
// this external context's disposal is separate caller payload cleanup.
#include "../../Code/Sparkplug/spCinematic.h"
#include <array>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace sparkplug::reconstruction
{
    class spCinematicManager;
    struct spCinematicCapturedOwnersForAnalysis final
    {
        enum class Profile {Unqualified,EmptyCinematicsWithRetainedPlainNames};
        Profile profile=Profile::Unqualified;
        std::vector<std::unique_ptr<spCinematic>> owners;
    };

    struct spCinematicCapturedOwnerStateForAnalysis final
    {
        spCinematicCapturedOwnerStateForAnalysis()=default;
    private:
        friend class spCinematicCapturedOwnerContextForAnalysis;
        friend class spCinematicManager;
        std::array<std::unique_ptr<spCinematic>,4> owners;
        std::size_t initialCount=0;
        const spCinematicManager* binding=nullptr;
    };

    class spCinematicCapturedOwnerContextForAnalysis final
    {
    public:
        static constexpr std::size_t MaximumCapturedOwners=4;
        // A host admission bound, not a native string-format limit.
        static constexpr std::size_t MaximumPlainNameBytes=63;
        explicit spCinematicCapturedOwnerContextForAnalysis(spCinematicCapturedOwnersForAnalysis&& input)
        {
            using Profile=spCinematicCapturedOwnersForAnalysis::Profile;
            if(input.profile!=Profile::EmptyCinematicsWithRetainedPlainNames||input.owners.size()>MaximumCapturedOwners)
                throw std::logic_error("Cinematic owner context requires its captured profile/count");
            // Validate the whole batch before moving even its first owner.
            for(std::size_t i=0;i<input.owners.size();++i)
            {
                const auto* value=input.owners[i].get();std::string_view name;
                if(!value||value->GetField14ForAnalysis()!=0||value->GetOwnedRecordCountForAnalysis()!=0
                    ||!ReadPlainNameForAnalysis(value->GetName(),name))
                    throw std::logic_error("Cinematic captured owner must be empty with a retained plain Name");
                for(std::size_t j=0;j<i;++j)if(input.owners[j].get()==value)
                    throw std::logic_error("Cinematic captured owners must have unique identity");
            }
            auto state=std::make_shared<spCinematicCapturedOwnerStateForAnalysis>();
            state->initialCount=input.owners.size();
            for(std::size_t i=0;i<input.owners.size();++i)state->owners[i]=std::move(input.owners[i]);
            state_=std::move(state);input.owners.clear(); // No growth/allocation after first move.
        }
        ~spCinematicCapturedOwnerContextForAnalysis()
        {
            // Caller cleanup, not the Manager destructor or native node ABI.
            // Qualified use destroys the borrowed Manager before this context.
            for(auto& owner:state_->owners)owner.reset();
            state_.reset();
        }
        spCinematicCapturedOwnerContextForAnalysis(const spCinematicCapturedOwnerContextForAnalysis&)=delete;
        spCinematicCapturedOwnerContextForAnalysis& operator=(const spCinematicCapturedOwnerContextForAnalysis&)=delete;
        spCinematicCapturedOwnerContextForAnalysis(spCinematicCapturedOwnerContextForAnalysis&&)=delete;
        spCinematicCapturedOwnerContextForAnalysis& operator=(spCinematicCapturedOwnerContextForAnalysis&&)=delete;

        [[nodiscard]] std::size_t GetOwnedCountForAnalysis() const noexcept
        {std::size_t count=0;for(const auto& value:state_->owners)count+=bool(value);return count;}
        [[nodiscard]] const spCinematic* GetOwnedForAnalysis(std::size_t originalIndex) const noexcept
        {return originalIndex<state_->initialCount?state_->owners[originalIndex].get():nullptr;}
        [[nodiscard]] bool IsBoundForAnalysis() const noexcept{return state_->binding!=nullptr;}
        [[nodiscard]] static bool ReadPlainNameForAnalysis(const char* text,std::string_view& result) noexcept
        {
            if(!text)return false;
            for(std::size_t i=0;i<=MaximumPlainNameBytes;++i)
            {
                const auto value=static_cast<unsigned char>(text[i]);
                if(!value){if(!i)return false;result=std::string_view(text,i);return true;}
                if(value>127)return false;
            }
            return false;
        }
        [[nodiscard]] static bool IsPlainQueryForAnalysis(std::string_view text) noexcept
        {
            if(text.empty()||text.size()>MaximumPlainNameBytes)return false;
            for(unsigned char value:text)if(!value||value>127)return false;
            return true;
        }
    private:
        friend class spCinematicManager;
        std::shared_ptr<spCinematicCapturedOwnerStateForAnalysis> state_;
    };
}

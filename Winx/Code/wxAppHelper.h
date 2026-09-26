#pragma once
#include "Code/SparkBase/spBaseObject.h"
#include "Analysis/Host/wxAppHelperHost.h"
#include <array>
#include <vector>

namespace winx::reconstruction
{
    class wxAppHelper : public sparkplug::reconstruction::spBaseObject
    {
    public:
        static constexpr sparkplug::reconstruction::spClassID ClassID = 0x41EB3DB5;
        using Handle = wxAppHelperHost::Handle;
        struct Configuration
        {
            bool showCinematics = true, fullScreen = true, useGamePad = false;
            bool enableSound = true, enableDialog = true, enableShadows = false;
            bool loadFromPCK = false, buildPCK = false;
            std::uint32_t firstPCKLevel = 0;
            bool loadFromCD = false;
            std::uint32_t language = 0, displayType = 0, territory = 0, startLevel = 0;
            std::uint32_t unknown64 = 0, unknown68 = 0;
            bool testCinematic = false;
            std::uint32_t cinematicToTest = 0;
        };
        struct Runtime
        {
            Handle field14 = 0, field18 = 0;
            std::uint32_t field74 = 0, field78 = 0, field7C = 0, field80 = 0;
            bool active = false;
            std::uint32_t countdown = 0;
            bool menuReady = false;
            float field90 = 1, field94 = 1;
        };
        explicit wxAppHelper(wxAppHelperHost&);
        ~wxAppHelper() override;
        wxAppHelper(const wxAppHelper&) = delete;
        wxAppHelper& operator=(const wxAppHelper&) = delete;
        static void SetFactoryHostForAnalysis(wxAppHelperHost*) noexcept;
        static wxAppHelper* GetInstance() noexcept;
        static const sparkplug::reconstruction::spRTTIRecord& StaticRTTI() noexcept;
        const sparkplug::reconstruction::spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<sparkplug::reconstruction::spBaseObject> vfunc_10(
            sparkplug::reconstruction::spCloneManager&) const override;
        static constexpr std::uint32_t GetStageCountForAnalysis() noexcept { return 22; }
        static bool ParseBooleanForAnalysis(std::string_view) noexcept;
        static std::uint32_t ParseLanguageForAnalysis(std::string_view) noexcept;
        static std::uint32_t ParseDisplayTypeForAnalysis(std::string_view) noexcept;
        static std::uint32_t ParseTerritoryForAnalysis(std::string_view) noexcept;
        void LoadConfigurationForAnalysis();
        void ParseConfigurationForAnalysis(std::string_view);
        static Configuration PS2ConfigurationForAnalysis(std::uint32_t systemLanguage) noexcept;
        const Configuration& GetConfigurationForAnalysis() const;
        void SetConfigurationForAnalysis(const Configuration& c) { config_ = c; }
        Runtime& GetRuntimeForAnalysis() noexcept { return runtime_; }
        const Runtime& GetRuntimeForAnalysis() const noexcept { return runtime_; }
        // Leaves output untouched when both flags are disabled, as in the PC code.
        void FormatPackagePathForAnalysis(std::string_view name, std::string& output);
        void OpenPackageForAnalysis(std::string_view name, std::uint32_t mode);
        void ClosePackageForAnalysis(std::string_view name);
        static constexpr std::int32_t QueryPackageForAnalysis(std::string_view) noexcept { return -1; }
        static void ReleasePackageQueryForAnalysis(std::string_view, std::int32_t) noexcept {}
        void LoadResourceTreesForAnalysis(const std::vector<std::string>& names);
        void CheckOneLinersForAnalysis();
        void BuildInGamePackageForAnalysis();
        void ResetMenuForAnalysis();
        bool ResetSessionForAnalysis();
        bool InitializeForAnalysis();
        bool RunStageForAnalysis(std::uint32_t stage);
        void InitializeMenuForAnalysis();
        bool UpdateForAnalysis();
        void BuildPackagesStepForAnalysis();
        void ShutdownForAnalysis();
    private:
        static wxAppHelper& Current();
        Handle Service(std::uint32_t id) { return host_.ResolveForAnalysis(id, true); }
        Handle Call(std::uint32_t operation, std::uint32_t service,
            std::initializer_list<Handle> args = {});
        void PrepareSceneStorage();
        void GameSlot(std::uint32_t field, std::uint32_t slot);
        wxAppHelperHost& host_;
        std::optional<Configuration> config_; // Original constructor leaves settings uninitialized.
        Runtime runtime_;
    };
}

#include "Code/wxAssetManager.h"
#include "Analysis/PC/wxAssetManagerAbi.h"
#include "Analysis/PS2/wxAssetManagerAbi.h"
#include <stdexcept>
#include <vector>

using namespace winx::reconstruction;
using namespace sparkplug::reconstruction;
namespace
{
    void Check(bool condition, const char* message)
    { if (!condition) throw std::runtime_error(message); }

    struct Host;
    struct Stream : wxAssetFileStreamForAnalysis
    {
        explicit Stream(Host& owner) : owner(owner) {}
        bool OpenForAnalysis(std::string_view, std::uint32_t) override;
        Host& owner;
    };
    struct Host : wxAssetManagerHost
    {
        bool ps2 = false, opened = true;
        std::uint32_t language = 0, mode = 0;
        std::optional<std::string> root = std::string("C:\\Game\\");
        std::optional<std::string> languageId;
        std::string path, error;
        std::vector<std::string> reads;
        bool IsPs2ForAnalysis() const noexcept override { return ps2; }
        std::uint32_t AppLanguageForAnalysis() const noexcept override { return language; }
        void SetAppLanguageForAnalysis(std::uint32_t value) noexcept override { language = value; }
        std::optional<std::string> ReadPcRegistryValueForAnalysis(
            std::string_view key, std::string_view value) override
        {
            Check(key == "Software\\Konami\\Winx Club", "registry key");
            reads.emplace_back(value);
            if (value == "MediaPath") return root;
            if (value == "LanguageID") return languageId;
            throw std::runtime_error("registry value");
        }
        std::unique_ptr<wxAssetFileStreamForAnalysis> CreateFileStreamForAnalysis() override
        { return std::make_unique<Stream>(*this); }
        void ReportOpenFailureForAnalysis(std::string_view value) noexcept override
        { error = value; }
    };
    bool Stream::OpenForAnalysis(std::string_view value, std::uint32_t requested)
    { owner.path = value; owner.mode = requested; return owner.opened; }

    void ExpectPath(wxAssetManager& manager, std::uint32_t category,
        std::uint32_t subcategory, std::string_view expected)
    {
        std::string output = "unchanged";
        Check(manager.BuildPathForAnalysis(output, 0x12c, category, subcategory, "test.bin"),
            "path build");
        Check(output == expected, "path contents");
        Check(!manager.BuildPathForAnalysis(output, expected.size(), category, subcategory,
            "test.bin") && output == expected, "strict capacity and unchanged output");
    }
}

int main()
{
    static_assert(sizeof(winx::analysis::pc::wxAssetManagerAbi) == 0x180);
    static_assert(sizeof(winx::analysis::ps2::wxAssetManagerAbi) == 0x180);
    Host host;
    host.language = 2;
    host.languageId = "1036";
    wxAssetManager::SetFactoryHostForAnalysis(&host);
    {
        wxAssetManager manager(host);
        Check(wxAssetManager::GetInstanceForAnalysis() == &manager, "singleton");
        Check(manager.vfunc_18().classID == wxAssetManager::ClassID, "RTTI");
        std::string output;
        Check(!manager.BuildPathForAnalysis(output, 0x12c, 1, 1, "x"), "uninitialized");
        manager.InitializeForAnalysis();
        Check(host.reads == std::vector<std::string>{"MediaPath", "LanguageID"}, "registry order");
        Check(host.language == 1 && manager.LanguageDirectoryForAnalysis() == "French\\", "language override");
        ExpectPath(manager, 1, 1, "C:\\Game\\Levels\\Bloom\\test.bin");
        ExpectPath(manager, 3, 0x33, "C:\\Game\\Sounds\\French\\test.bin");
        ExpectPath(manager, 5, 0x44, "C:\\Game\\Movies\\SUBTITLE\\test.bin");
        ExpectPath(manager, 5, 1, "C:\\Game\\Movies\\Bloom\\French\\test.bin");
        ExpectPath(manager, 7, 0, "C:\\Game\\Voices\\French\\test.bin");
        ExpectPath(manager, 13, 0x44, "C:\\Game\\Fonts\\SUBTITLE\\test.bin");
        ExpectPath(manager, 17, 0x44, "C:\\Game\\OneLiners\\SUBTITLE\\French\\test.bin");
        Check(wxAssetManager::CategoryForAnalysis(8).empty(), "empty category slot");
        Check(wxAssetManager::SubcategoryForAnalysis(31) == "Bloom\\", "repeated subcategory");
        Check(!manager.BuildPathForAnalysis(output, 0x12c, 19, 0, "x"), "category bound");
        Check(!manager.BuildPathForAnalysis(output, 0x12c, 1, 69, "x"), "subcategory bound");
        auto stream = manager.OpenFileStreamForAnalysis(1, 1, "test.bin", false);
        Check(stream && host.mode == 1 && host.path == "C:\\Game\\Levels\\Bloom\\test.bin", "open mode 1");
        stream = manager.OpenFileStreamForAnalysis(1, 1, "test.bin", true);
        Check(stream && host.mode == 2, "open mode 2");
        host.opened = false;
        stream = manager.OpenFileStreamForAnalysis(1, 1, "missing.bin", false);
        Check(!stream && host.error == "C:\\Game\\Levels\\Bloom\\missing.bin", "open failure");
        spCloneManager clones;
        auto clone = manager.vfunc_10(clones);
        Check(clone && wxAssetManager::GetInstanceForAnalysis() == clone.get(), "clone singleton");
        Check(static_cast<wxAssetManager*>(clone.get())->RootForAnalysis() == nullptr, "clone resets catalog");
        clone.reset();
        Check(wxAssetManager::GetInstanceForAnalysis() == nullptr, "unconditional singleton clear");
    }
    host.ps2 = true;
    host.language = 3;
    host.reads.clear();
    {
        wxAssetManager manager(host);
        manager.InitializeForAnalysis();
        Check(host.reads.empty() && std::string_view(manager.RootForAnalysis()) == "data\\", "PS2 root");
        ExpectPath(manager, 9, 68, "data\\Scripts\\SUBTITLE\\test.bin");
        ExpectPath(manager, 7, 1, "data\\Voices\\Bloom\\Italian\\test.bin");
    }
    host.ps2 = false;
    host.root.reset();
    {
        wxAssetManager manager(host);
        bool failed = false;
        try { manager.InitializeForAnalysis(); } catch (const std::runtime_error&) { failed = true; }
        Check(failed, "missing MediaPath");
    }
}

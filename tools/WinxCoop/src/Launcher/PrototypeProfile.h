#pragma once
// Read-only installed-game paths and own-module integrity. No prepared copy,
// game INI, whole-game hash or full resource scan is a compatibility gate.
#include "../Gameplay/GameplayProtocol.h"
#include <cstdint>
#include <memory>
#include <string>

namespace winxcoop::launcher
{
    struct PrototypeFilePins;
    struct PrototypePaths final
    {
        std::wstring gameRoot, media, adapter, helper, outer, caseRoot;
        std::uint32_t seconds = 0;
        bool menuFirst = true; // Local service policy, not a wire/IPC field.
    };
    struct PrototypeProfile final
    {
        PrototypePaths paths;
        std::wstring game;
        gameplay::Compatibility compatibility;
        gameplay::LevelDescriptor level;
        std::uint64_t mediaFiles{}, mediaBytes{};
        // Game source stays read-pinned without a hash comparison. The adapter
        // is our own module: its SHA remains an integrity/version pin. Neither
        // handle is exported to IPC/network; the definition is private to .cpp.
        std::shared_ptr<const PrototypeFilePins> filePins;
    };
    // Paths in the adjacent WinxCoopLauncher.ini are relative to that INI.
    // [Paths]: gameRoot/media/adapter/helper/outer/caseRoot. Empty gameRoot waits
    // for explicit user selection. [Prototype]: menuFirst and legacy seconds.
    // Command-line overrides are applied by the UI before Check is constructed.
    PrototypePaths ReadPrototypePaths();
    std::wstring ResolvePrototypePath(const std::wstring& path);
    // Shared by the UI for its own settings/logs. Does not create directories.
    std::wstring LocalPrototypeDataDirectory();

    // This worker checks selected paths, game PE shape and our adapter module.
    // No transport, protocol, IPC, native operation or UI access runs on it.
    // Poll on the UI thread; Cancel then keep polling during orderly shutdown.
    // A Windows 7 thread handle stays owned until the worker has been joined.
    class PrototypeProfileCheck final
    {
    public:
        explicit PrototypeProfileCheck(PrototypePaths);
        ~PrototypeProfileCheck();
        PrototypeProfileCheck(const PrototypeProfileCheck&) = delete;
        PrototypeProfileCheck& operator=(const PrototypeProfileCheck&) = delete;
        void Cancel() noexcept;
        bool Finished() const noexcept;
        bool Successful() const noexcept;
        const PrototypeProfile& Profile() const;
        const std::string& Failure() const;
    private:
        struct Impl;
        std::unique_ptr<Impl> implementation_;
    };
}

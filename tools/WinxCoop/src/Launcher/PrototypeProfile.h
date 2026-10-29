#pragma once
// Read-only qualification for the single Stage P scene. This module never
// copies game resources, changes INI files or prepares the registry.
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
        std::uint32_t seconds = 900;
    };
    struct PrototypeProfile final
    {
        PrototypePaths paths;
        std::wstring game, ini;
        gameplay::Compatibility compatibility;
        gameplay::LevelDescriptor level;
        std::uint64_t mediaFiles{}, mediaBytes{};
        // Exact EXE/DLL bytes stay open read-only, denying replacement from
        // qualification through every game using this profile. No OS handle is
        // exported to IPC or the network; the definition is private to .cpp.
        std::shared_ptr<const PrototypeFilePins> filePins;
    };
    // Paths in the adjacent WinxCoopLauncher.ini are relative to that INI.
    // [Paths]: gameRoot/media/adapter/helper/outer/caseRoot. [Prototype]: seconds.
    // Command-line overrides are applied by the UI before Check is constructed.
    PrototypePaths ReadPrototypePaths();
    std::wstring ResolvePrototypePath(const std::wstring& path);

    // Only this worker performs the potentially long, read-only tree hashing.
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

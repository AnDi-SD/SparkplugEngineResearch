#include "Code/SparkplugDX/spDXTextureManager.h"
#include "Analysis/PC/spTextureManagerAbi.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0;
    void Check(const bool value, const char* const message)
    {
        ++checks;
        if (!value) throw std::runtime_error(message);
    }
    const char* const Modes[]{
        "defaults", "disabled", "zero", "one", "startup", "raw-flag",
        "sequence", "seed-growth", "seed-shrink", "copy", "clone",
        "hooks", "support-delete", "singleton-replaced",
    };
    using Record = spTextureManager::RecordForAnalysis;
    const std::vector<Record> Records{
        {0x11111111, 0x22222222, 0x33333333},
        {0xFFFFFFFF, 0x80000000, 0},
    };
    std::string Snapshot(const spDXTextureManager& value)
    {
        std::ostringstream out;
        out << '[' << unsigned(value.GetReserveFlagForAnalysis()) << ','
            << value.GetRequestedRecordCountForAnalysis() << ','
            << value.GetRecordsForAnalysis().size() << ','
            << value.GetRecordCapacityForAnalysis() << ','
            << (spTextureManager::GetInstance() == &value) << ",[";
        bool separator = false;
        for (const auto& record : value.GetRecordsForAnalysis())
        {
            for (const auto word : record)
            {
                if (separator) out << ',';
                out << word;
                separator = true;
            }
        }
        out << "]]";
        return out.str();
    }
    std::string Run(const std::string& mode)
    {
        auto source = std::make_unique<spDXTextureManager>();
        Check(source->IsExactly(spDXTextureManager::ClassID)
            && source->IsKindOf(spTextureManager::ClassID)
            && source->IsKindOf(spBaseObject::ClassID), "native RTTI hierarchy");
        Check(source->GetReserveFlagForAnalysis() == 0
            && source->GetRequestedRecordCountForAnalysis() == 0
            && source->GetRecordsForAnalysis().empty()
            && source->GetRecordCapacityForAnalysis() == 0, "native constructor defaults");
        Check(spTextureManager::GetInstance() == source.get(), "constructor publishes singleton");
        std::vector<std::string> states;
        auto state = [&](const spDXTextureManager& value) { states.push_back(Snapshot(value)); };
        auto configure = [&](const std::uint8_t flag, const std::uint32_t count)
        {
            Check(source->ConfigureReserveForAnalysis(flag, count), "bounded reserve completes");
            Check(source->GetReserveFlagForAnalysis() == flag
                && source->GetRequestedRecordCountForAnalysis() == count, "raw reserve settings retained");
            state(*source);
        };
        if (mode == "defaults") state(*source);
        else if (mode == "disabled") configure(0, 0xFFFFFFFF);
        else if (mode == "zero") configure(1, 0);
        else if (mode == "one") configure(1, 1);
        else if (mode == "startup") configure(1, 0x400);
        else if (mode == "raw-flag") configure(0xFF, 3);
        else if (mode == "sequence")
        {
            configure(1, 8);
            configure(0, 0xFFFFFFFF);
            configure(0x80, 3);
            configure(2, 10);
        }
        else if (mode == "seed-growth" || mode == "seed-shrink")
        {
            source->SetRecordStorageForAnalysis(Records);
            state(*source);
            configure(1, mode == "seed-growth" ? 9 : 1);
            Check(source->GetRecordsForAnalysis() == Records, "reserve retains every opaque record word");
        }
        else if (mode == "copy" || mode == "clone" || mode == "singleton-replaced")
        {
            source->SetRecordStorageForAnalysis(Records);
            Check(source->ConfigureReserveForAnalysis(0xFF, 9), "configure populated source");
            spCloneManager manager;
            auto destination = mode == "clone"
                ? source->vfunc_10(manager)
                : spRTTIManager::Instance().Create(spDXTextureManager::ClassID);
            Check(bool(destination), "native factory/clone returns object");
            auto* target = dynamic_cast<spDXTextureManager*>(destination.get());
            Check(target != nullptr, "concrete destination identity");
            if (mode == "clone")
                Check(manager.FindClone(*source) == target, "clone map precedes inherited copy");
            if (mode == "copy")
            {
                Check(target->ConfigureReserveForAnalysis(2, 4), "configure copy destination");
                Check(source->vfunc_14(*target, manager), "root copy returns true");
                Check(target->GetReserveFlagForAnalysis() == 2
                    && target->GetRequestedRecordCountForAnalysis() == 4
                    && target->GetRecordCapacityForAnalysis() == 4, "copy does not change destination settings");
            }
            else if (mode == "clone")
            {
                Check(target->GetRecordsForAnalysis().empty()
                    && target->GetRecordCapacityForAnalysis() == 0
                    && target->GetReserveFlagForAnalysis() == 0
                    && target->GetRequestedRecordCountForAnalysis() == 0, "clone resets source-specific storage and settings");
            }
            state(*source); state(*target);
            if (mode == "singleton-replaced")
            {
                source.reset();
                Check(spTextureManager::GetInstance() == nullptr,
                    "older instance destructor clears newer singleton unconditionally");
                state(*target);
            }
            destination.reset();
            if (source) state(*source);
        }
        else if (mode == "hooks")
        {
            source->SetRecordStorageForAnalysis(Records);
            Check(source->ConfigureReserveForAnalysis(0x80, 9), "configure no-op source");
            const auto before = Snapshot(*source);
            const auto* const storage = source->GetRecordsForAnalysis().data();
            spTextureManager& base = *source;
            base.NativeHook1CForAnalysis();
            base.NativeHook20ForAnalysis();
            std::uint32_t notification = 0x12345678;
            source->vfunc_0C(&notification);
            Check(Snapshot(*source) == before
                && source->GetRecordsForAnalysis().data() == storage,
                "both original device hooks and notification leave object/storage unchanged");
            state(*source);
        }
        else if (mode == "support-delete")
        {
            Check(source->ConfigureReserveForAnalysis(1, 5), "support-view deletion has allocated vector");
            state(*source);
            // Portable host normalizes the native support+10 view to its base.
            spTextureManager* support = source.release();
            delete support;
        }
        else throw std::runtime_error("unknown case");
        source.reset();
        Check(spTextureManager::GetInstance() == nullptr, "all completed lifetimes clear singleton");
        std::ostringstream out;
        out << "{\"case\":\"" << mode << "\",\"states\":[";
        for (std::size_t index = 0; index < states.size(); ++index)
        {
            if (index) out << ',';
            out << states[index];
        }
        out << "],\"singletonCleared\":true}";
        return out.str();
    }
}

int main(int argc, char** argv)
{
    try
    {
        static_assert(std::is_abstract_v<spTextureManager>);
        static_assert(!std::is_abstract_v<spDXTextureManager>);
        static_assert(sizeof(Record) == sizeof(sparkplug::evidence::pc::spTextureManagerRecordLayout));
        (void)spDXTextureManager::StaticRTTI();
        Check(spTextureManager::StaticRTTI().factory == nullptr, "base registration remains nonallocating");
        if (argc == 2 && std::string(argv[1]) == "--batch")
        {
            std::string mode;
            while (std::cin >> mode) std::cout << Run(mode) << '\n';
            return 0;
        }
        for (const auto* mode : Modes) (void)Run(mode);
        {
            spDXTextureManager value;
            Check(!value.ConfigureReserveForAnalysis(1, 0x15555556), "native 32-bit length limit rejects count");
            Check(value.GetReserveFlagForAnalysis() == 1
                && value.GetRequestedRecordCountForAnalysis() == 0x15555556
                && value.GetRecordCapacityForAnalysis() == 0, "length failure retains prior settings writes");
            Check(value.ConfigureReserveForAnalysis(0, 0xFFFFFFFF), "disabled reserve does not validate count");
        }
        std::cout << "PASS " << checks << '/' << checks
            << ": texture manager lifetime, reserve and original PC no-op hooks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}

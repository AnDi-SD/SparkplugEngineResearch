#include "Code/wxAudioListener.h"
#include "Analysis/PC/wxAudioListenerAbi.h"
#include "Analysis/PS2/wxAudioListenerAbi.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    using namespace winx::reconstruction;
    using namespace sparkplug::reconstruction;
    void Check(bool okay, const char* label)
    {
        if (!okay) { std::cerr << label << '\n'; std::exit(1); }
    }
    struct Host final : wxAudioListenerHost
    {
        std::string trace;
        int serial = 0;
        bool teardown = false;
        bool linked = true;
        bool copyOkay = true;
        unsigned copies = 0;
        void ConstructEntityForAnalysis(wxAudioListener&, bool value) override
        { Check(value, "entity constructor argument"); trace += 'E'; }
        void ConstructContainerForAnalysis(wxAudioListener&) override { trace += 'C'; }
        void* AcquireListenerForAnalysis(wxAudioListener&) override
        { trace += 'A'; return reinterpret_cast<void*>(static_cast<std::uintptr_t>(++serial)); }
        void ReleaseListenerForAnalysis(wxAudioListener&, void* p) noexcept override
        { Check(p != nullptr, "listener retained"); trace += 'r'; }
        void DestroyContainerForAnalysis(wxAudioListener&) noexcept override { trace += 'c'; }
        void DestroyEntityForAnalysis(wxAudioListener&) noexcept override { trace += 'e'; }
        bool CopyEntityForAnalysis(const wxAudioListener&, wxAudioListener&,
            spCloneManager&) const override
        { ++const_cast<Host*>(this)->copies; return copyOkay; }
        bool IsAudioTeardownForAnalysis() const noexcept override { return teardown; }
        void StopListenerForAnalysis(wxAudioListener&, void*) noexcept override { trace += 'S'; }
        void UnregisterListenerForAnalysis(wxAudioListener&, void*) noexcept override { trace += 'U'; }
        bool HasLinkedEntityForAnalysis(const wxAudioListener&) const noexcept override { return linked; }
        void UnlinkEntityForAnalysis(wxAudioListener&) noexcept override { trace += 'L'; }
        void AttachEntityForAnalysis(wxAudioListener&) override { trace += 'T'; }
        void RegisterListenerForAnalysis(wxAudioListener&, void*) override { trace += 'R'; }
        void ActivateListenerForAnalysis(wxAudioListener&, void*) override { trace += 'V'; }
    };
}

int main()
{
    Check(sizeof(winx::evidence::pc::wxAudioListenerLayout) == 0x148, "PC size");
    Check(sizeof(winx::evidence::ps2::wxAudioListenerLayout) == 0x160, "PS2 size");
    Check(wxAudioListener::StaticRTTI().classID == wxAudioListener::ClassID, "class ID");
    Check(wxAudioListener::StaticRTTI().baseClassID == 0x796A1869, "registration base");
    Host host;
    wxAudioListener::SetFactoryHostForAnalysis(&host);
    {
        wxAudioListener listener(host);
        Check(host.trace == "ECA" && !listener.IsInitializedForAnalysis(), "constructor order");
        std::uint32_t code = 0x1D;
        listener.vfunc_0C(&code);
        Check(host.trace == "ECA", "unhandled notification");
        code = 0x1C;
        listener.vfunc_0C(&code);
        listener.vfunc_0C(&code);
        Check(host.trace == "ECATRV" && listener.IsInitializedForAnalysis(),
            "one-shot initialization");
        spCloneManager manager;
        auto clone = listener.vfunc_10(manager);
        Check(clone && host.copies == 1 && host.trace == "ECATRVECA", "clone construction");
        auto* typed = dynamic_cast<wxAudioListener*>(clone.get());
        Check(typed && !typed->IsInitializedForAnalysis()
            && typed->ListenerForAnalysis() != listener.ListenerForAnalysis(),
            "clone keeps fresh audio state");
        host.linked = false;
        clone.reset();
        Check(host.trace == "ECATRVECASUcre", "clone teardown skips unlink");
        host.teardown = true;
        host.copyOkay = false;
        Check(!listener.vfunc_10(manager) && host.copies == 2
            && host.trace == "ECATRVECASUcreECAcre", "failed copy destroys clone");
    }
    Check(host.trace == "ECATRVECASUcreECAcrecre", "global teardown suppresses audio calls");
    host.trace.clear();
    host.teardown = false;
    host.linked = true;
    auto factoryObject = spRTTIManager::Instance().Create(wxAudioListener::ClassID);
    Check(factoryObject && host.trace == "ECA", "registered factory");
    factoryObject.reset();
    Check(host.trace == "ECASULcre", "linked entity teardown order");
    wxAudioListener::SetFactoryHostForAnalysis(nullptr);
    return 0;
}

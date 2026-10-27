#pragma once

// Original PC/PS2 identity. Header path and property names are analytical;
// the PC TextWidget adds no own bytes to the native Widget54 layout.
#include "spWidget.h"

namespace sparkplug::reconstruction
{
    class spTextNode;
    class spTextWidget final : public spWidget
    {
    public:
        static constexpr spClassID ClassID = 0x0A3053A8;
        static constexpr std::uint32_t MissingColourForAnalysis = 0xFF000000;
        explicit spTextWidget(spGUIHost& host) : spWidget(host) {}
        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&, spCloneManager&) const override;

        // PC436290/4362E0 choose the root if it is a TextNode, even when its
        // cached Text is null; otherwise only NORMAL is queried. The returned
        // string is borrowed from the actual TextRenderable and may be null.
        [[nodiscard]] const char* GetTextForAnalysis() const;
        [[nodiscard]] std::uint32_t GetColourForAnalysis() const;
        // PC436060 always reads NORMAL, independently of root type.
        [[nodiscard]] std::uint32_t GetNormalColourForAnalysis() const;

        // PC4360E0/4361D0 choose the typed root exclusively; otherwise visit
        // NORMAL, DISABLED, HIGHLIGHTED in order, retaining repeated targets.
        // Text uses the actual shared TextRenderable layout and RenderNode
        // bounds rebuild. A nonempty layout requires an established shared
        // FontManager; cold platform acquisition remains unqualified.
        [[nodiscard]] bool SetTextForAnalysis(const char*, std::string* error = nullptr);
        void SetColourForAnalysis(std::uint32_t);

    private:
        [[nodiscard]] static spTextNode* TextNodeForAnalysis(spNode*);
        [[nodiscard]] spTextNode* ReadNodeForAnalysis() const;
    };
}

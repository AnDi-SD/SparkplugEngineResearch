#include "spTextWidget.h"
#include "spTextNode.h"
#include "spTextRenderable.h"
#include "spFontManager.h"
#include <stdexcept>

namespace sparkplug::reconstruction
{
    namespace
    {
        std::unique_ptr<spBaseObject> CreateTextWidget()
        {
            auto* host = spGUIObject::GetFactoryHostForAnalysis();
            if (!host) throw std::logic_error("spTextWidget requires an explicit GUI host");
            return std::make_unique<spTextWidget>(*host);
        }
        const spRTTIRecord record{spTextWidget::ClassID, spWidget::ClassID,
            "spTextWidget", &spWidget::StaticRTTI(), &CreateTextWidget, nullptr};
        const bool registered = spRTTIManager::Instance().RegisterDeferredForAnalysis(record);
    }
    const spRTTIRecord& spTextWidget::StaticRTTI() noexcept { (void)registered; return record; }
    const spRTTIRecord& spTextWidget::vfunc_18() const noexcept { return record; }
    std::unique_ptr<spBaseObject> spTextWidget::vfunc_10(spCloneManager& manager) const
    {
        auto clone = std::make_unique<spTextWidget>(GUIHostForAnalysis());
        manager.RegisterCloneForAnalysis(*this, *clone);
        if (!vfunc_14(*clone, manager)) return nullptr;
        return clone;
    }
    bool spTextWidget::vfunc_14(spBaseObject& destination, spCloneManager& manager) const
    {
        auto* target = dynamic_cast<spTextWidget*>(&destination);
        if (!target || !spWidget::vfunc_14(destination, manager)) return false;
        // The base shallow-copies state nodes but retains the destination's
        // Entity root. Both property getters execute AFTER that base copy;
        // colour is read only after the text setters and bounds rebuilds.
        if (!target->SetTextForAnalysis(GetTextForAnalysis())) return false;
        target->SetColourForAnalysis(GetColourForAnalysis());
        return true;
    }
    spTextNode* spTextWidget::TextNodeForAnalysis(spNode* node)
    {
        if (!node || !node->IsKindOf(spTextNode::ClassID)) return nullptr;
        auto* result = dynamic_cast<spTextNode*>(node);
        if (!result) throw std::logic_error("TextNode identity requires the actual shared TextNode type");
        return result;
    }
    spTextNode* spTextWidget::ReadNodeForAnalysis() const
    {
        if (auto* root = TextNodeForAnalysis(GetNodeForGUIForAnalysis())) return root;
        return TextNodeForAnalysis(GetStateNodesForAnalysis()[0]);
    }
    const char* spTextWidget::GetTextForAnalysis() const
    {
        auto* node = ReadNodeForAnalysis();
        auto* text = node ? node->GetTextRenderableForAnalysis() : nullptr;
        return text && text->GetTextForAnalysis() ? text->GetTextForAnalysis()->c_str() : nullptr;
    }
    std::uint32_t spTextWidget::GetColourForAnalysis() const
    {
        auto* node = ReadNodeForAnalysis();
        auto* text = node ? node->GetTextRenderableForAnalysis() : nullptr;
        return text ? text->GetColorForAnalysis() : MissingColourForAnalysis;
    }
    std::uint32_t spTextWidget::GetNormalColourForAnalysis() const
    {
        auto* node = TextNodeForAnalysis(GetStateNodesForAnalysis()[0]);
        auto* text = node ? node->GetTextRenderableForAnalysis() : nullptr;
        return text ? text->GetColorForAnalysis() : MissingColourForAnalysis;
    }
    bool spTextWidget::SetTextForAnalysis(const char* value, std::string* error)
    {
        if (error) error->clear();
        const auto update = [&](spTextNode* node)
        {
            auto* text = node ? node->GetTextRenderableForAnalysis() : nullptr;
            if (!text) return true;
            if (!text->SetTextForAnalysis(value, spFontManager::GetInstance(), error)) return false;
            node->RebuildRenderableBoundsForAnalysis();
            return true;
        };
        if (auto* root = TextNodeForAnalysis(GetNodeForGUIForAnalysis())) return update(root);
        for (std::size_t i = 0; i != 3; ++i)
            if (!update(TextNodeForAnalysis(GetStateNodesForAnalysis()[i]))) return false;
        return true;
    }
    void spTextWidget::SetColourForAnalysis(std::uint32_t value)
    {
        const auto update = [value](spTextNode* node)
        {
            if (node)
                if (auto* text = node->GetTextRenderableForAnalysis()) text->SetColorForAnalysis(value);
        };
        if (auto* root = TextNodeForAnalysis(GetNodeForGUIForAnalysis())) { update(root); return; }
        for (std::size_t i = 0; i != 3; ++i) update(TextNodeForAnalysis(GetStateNodesForAnalysis()[i]));
    }
}

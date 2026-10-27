#pragma once

// Inferred path/API. PC41A520 creates the native80-byte Widget descendant;
// portable owning buffers and typed borrowed nodes do not reproduce that ABI.
#include "spWidget.h"
#include <optional>
#include <string>
#include <vector>

namespace sparkplug::reconstruction
{
    class spTextNode;
    class spEditBox final : public spWidget
    {
    public:
        static constexpr spClassID ClassID=0x253C432D;
        explicit spEditBox(spGUIHost& host):spWidget(host) {}
        ~spEditBox() override = default;
        static const spRTTIRecord& StaticRTTI() noexcept;
        const spRTTIRecord& vfunc_18() const noexcept override;
        std::unique_ptr<spBaseObject> vfunc_10(spCloneManager&) const override;
        bool vfunc_14(spBaseObject&,spCloneManager&) const override;
        void vfunc_0C(const void*) noexcept override;

        const char* GetInitialTextForAnalysis() const noexcept;
        void SetInitialTextForAnalysis(const char*);
        const char* GetTextForAnalysis() const noexcept;
        bool SetTextForAnalysis(const char*,std::string* error=nullptr);
        std::uint32_t GetTextCapacityForAnalysis() const noexcept;
        std::uint32_t GetTextLengthForAnalysis() const noexcept {return length_;}
        float GetMaximumTextExtentForAnalysis() const noexcept {return maximum_;}
        void SetMaximumTextExtentForAnalysis(float value) noexcept {maximum_=value;}
        std::uint8_t GetOverrideByteForAnalysis() const noexcept {return override_;}
        void SetOverrideByteForAnalysis(std::uint8_t value) noexcept {override_=value;}
        const char* GetPasswordCharacterForAnalysis() const noexcept {return password_.data();}
        void SetPasswordCharacterForAnalysis(const char*) noexcept;

        bool UpdateTextForAnalysis(std::string* error=nullptr);
        bool UpdateCursorForAnalysis(std::string* error=nullptr);
        bool AppendCharacterForAnalysis(std::uint8_t,std::string* error=nullptr);
        bool DeleteCharacterForAnalysis(std::string* error=nullptr);
        void RefreshSelectionForAnalysis() override;
        // Key14 Escape/54 consumes the real GUIManager focus operation.
        // Own1C resolves an existing stable typed graph; own0F consumes the
        // real focus manager then the current borrowed notification target.
        // Missing dependencies/cold NORMAL_TEXT creation explicitly refuse.
        void NotifyEditBoxForAnalysis(const spGUINotificationForAnalysis&);

        const std::array<spTextNode*,3>& GetTextStateNodesForAnalysis() const noexcept {return textNodes_;}
        spTextNode* GetCurrentTextNodeForAnalysis() const noexcept {return currentNode_;}
        spNode* GetCursorNodeForAnalysis() const noexcept {return cursor_;}
        // Existing resolved graph input only: no lookup, ownership or native
        // node-binding/graph-producer claim. All borrowed objects remain live.
        void SetTextGraphForFixture(std::array<spTextNode*,3> nodes,
            spTextNode* current,spNode* cursor) noexcept;

    private:
        void BindTextNodesForAnalysis();
        // Reverse destruction order releases current68 before initial78.
        std::optional<std::string> initial_;
        std::vector<char> buffer_;
        std::uint32_t length_=0;
        float maximum_=0;
        std::uint8_t override_=0;
        std::array<char,2> password_{};
        std::array<spTextNode*,3> textNodes_{};
        spTextNode* currentNode_=nullptr;
        spNode* cursor_=nullptr;
    };
}

#include "Code/Sparkplug/spTextWidget.h"
#include "Code/Sparkplug/spTextNode.h"
#include "Code/Sparkplug/spTextRenderable.h"
#include "Code/Sparkplug/spFont.h"
#include "Code/Sparkplug/spFontSerializer.h"
#include "Code/Sparkplug/spFontManager.h"
#include "Code/Sparkplug/spSerializerManager.h"
#include "Code/Sparkplug/spResourceManager.h"
#include "Code/SparkBase/spMemoryStream.h"
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sparkplug::reconstruction;
namespace
{
    unsigned checks = 0, comparisons = 0, nativeCases = 0;
    void Check(bool value, const char* reason)
    { ++checks; if (!value) throw std::runtime_error(reason); }
    void Match(std::uint32_t actual, std::uint32_t expected, const char* reason)
    {
        ++comparisons;
        if (actual != expected) std::cerr << reason << " actual " << std::hex << actual << " expected " << expected << std::dec << '\n';
        Check(actual == expected, reason);
    }
    std::uint32_t Bits(float value)
    { std::uint32_t result; std::memcpy(&result, &value, 4); return result; }
    struct Host final : spGUIHost
    {
        std::map<void*, std::uint16_t> references;
        unsigned unresolved = 0;
        void AddToEngineManagerForAnalysis(spEntity&) override {}
        void DestroyEntityForAnalysis(spEntity& entity) noexcept override
        { if (auto* node = entity.GetReferenceForAnalysis()) --references[node]; }
        std::uint16_t GetReferenceCountForAnalysis(void* p) override { return references[p]; }
        void SetReferenceCountForAnalysis(void* p, std::uint16_t count) override { references[p] = count; }
        void DeleteReferenceForAnalysis(void*) override { throw std::logic_error("Fixture has external owners"); }
        void SubscribeGUINodeForAnalysis(spNode&, spGUIObject&) override {}
        void UnsubscribeGUINodeForAnalysis(spNode&, spGUIObject&) noexcept override {}
        void DestroyGUIForAnalysis(spGUIObject&) noexcept override {}
        bool IsGUINodeEnabledForAnalysis(const spNode& node) override { return node.IsEnabledForAnalysis(); }
        void SetGUINodeEnabledForAnalysis(spNode& node, std::uint8_t value, bool recursive) override
        { node.SetEnabledForAnalysis(value != 0, recursive); }
        void* GetGUINotificationTargetForAnalysis(const spGUIObject&) override { return nullptr; }
        std::uint32_t ReadGUIInputWord18ForAnalysis(std::uintptr_t) override { throw std::logic_error("No input event"); }
        void DispatchNotificationForAnalysis(void*, const spNotificationForAnalysis&) override { throw std::logic_error("No notification target"); }
        void UnrecoveredGUINotificationForAnalysis(spGUIObject&, const void*, const char*) noexcept override { ++unresolved; }
    };
    struct Reader
    {
        std::ifstream input;
        explicit Reader(const char* path) : input(path, std::ios::binary) { Check(bool(input), "Open native TextWidget fixture"); }
        std::uint32_t U32()
        { std::uint32_t value = 0; input.read(reinterpret_cast<char*>(&value), 4); Check(bool(input), "TextWidget fixture word"); return value; }
        std::string Blob()
        {
            auto size = U32(); Check(size <= 65536, "TextWidget blob limit");
            std::string result(size, '\0'); if (size) input.read(result.data(), size); Check(bool(input), "TextWidget blob extent"); return result;
        }
        std::optional<std::string> Optional()
        { auto present = U32(); auto data = Blob(); Check(present <= 1, "Optional text flag"); return present ? std::optional<std::string>(std::move(data)) : std::nullopt; }
    };
    void MatchText(const char* actual, const std::optional<std::string>& expected, const char* reason)
    {
        ++comparisons; Check((actual != nullptr) == expected.has_value(), reason);
        if (expected) { ++comparisons; Check(*expected == actual, reason); }
    }
    void Basic()
    {
        Host host; spGUIObject::SetFactoryHostForAnalysis(&host);
        auto object = spRTTIManager::Instance().Create(spTextWidget::ClassID);
        auto* widget = dynamic_cast<spTextWidget*>(object.get());
        Check(widget && widget->IsKindOf(spWidget::ClassID), "TextWidget factory/type chain");
        Check(widget->GetNodeForGUIForAnalysis() == nullptr && widget->GetStateNodesForAnalysis() == std::array<spNode*,3>{}, "TextWidget inherited defaults");
        Check(widget->GetTextForAnalysis() == nullptr && widget->GetColourForAnalysis() == 0xFF000000 && widget->GetNormalColourForAnalysis() == 0xFF000000, "TextWidget default getters");
        Check(widget->SetTextForAnalysis(nullptr), "No target does not dereference a null text");
        widget->SetColourForAnalysis(0x12345678);
        auto clone = widget->Clone(); Check(clone && clone->IsExactly(spTextWidget::ClassID), "Blank TextWidget concrete clone");
        spNode plain; spTextNode empty; host.references[&empty] = 2;
        widget->SetReferenceForAnalysis(&empty);
        Check(widget->SetTextForAnalysis(nullptr), "Typed root without cached text remains a no-op");
        widget->SetReferenceForAnalysis(nullptr);
        spGUIObject::SetFactoryHostForAnalysis(nullptr);
    }
    void Replay(const char* path)
    {
        Reader input(path); Check(input.U32() == 0x31575454, "TTW1 fixture"); const auto fontBytes = input.Blob();
        const auto count = input.U32(); Check(count <= 128, "Native TextWidget case bound");
        for (unsigned caseIndex = 0; caseIndex != count; ++caseIndex)
        {
            const auto label = input.Blob();
            try
            {
                std::array<unsigned, 4> routing{}; for (auto& value : routing) value = input.U32();
                const auto nodeCount = input.U32(); Check(nodeCount <= 16, "TextWidget node bound");
                std::vector<std::array<unsigned, 2>> definitions(nodeCount);
                for (auto& value : definitions) { value[0] = input.U32(); value[1] = input.U32(); }
                const auto operation = input.U32(), colour = input.U32(); const auto value = input.Blob();
                Host host; spSerializerManager serializers; spResourceManager resources; spFontManager manager;
                spSerializerReadContextForAnalysis context(serializers, resources); context.fontManager = &manager;
                spFont font; spMemoryStream stream; std::string error;
                Check(stream.ResizeAndSetSize(static_cast<unsigned>(fontBytes.size())), "Font stream allocation");
                std::memcpy(stream.GetBuffer(), fontBytes.data(), fontBytes.size());
                Check(stream.Seek(spStream::SeekSource::essStart, 0), "Font stream rewind");
                Check(spFontSerializer{}.ReadPayloadForAnalysis(context, stream, static_cast<unsigned>(fontBytes.size()), font, &error), "Actual shared Font payload reader");
                manager.SetLayoutDefaultFontForAnalysis(&font);
                std::vector<std::shared_ptr<spNode>> nodes(1); std::vector<std::shared_ptr<spTextRenderable>> texts(nodeCount + 1);
                for (unsigned i = 1; i <= nodeCount; ++i)
                {
                    const auto& d = definitions[i-1];
                    nodes.push_back(d[0] ? std::static_pointer_cast<spNode>(std::make_shared<spTextNode>()) : std::make_shared<spNode>());
                    host.references[nodes[i].get()] = 2;
                    if (!d[1]) continue;
                    auto* node = dynamic_cast<spTextNode*>(nodes[i].get()); Check(node != nullptr, "Cached text belongs to actual TextNode");
                    auto text = std::make_shared<spTextRenderable>(); const char initial[]{static_cast<char>(64+i), '\0'};
                    Check(text->SetTextForAnalysis(initial, &manager, &error), "Initial shared Text layout"); text->SetColorForAnalysis(0x11000000+i);
                    Check(node->AttachRenderableForAnalysis(text), "Actual shared TextNode attachment"); texts[i] = std::move(text);
                }
                spTextWidget widget(host); widget.SetReferenceForAnalysis(nodes.at(routing[0]).get());
                widget.SetSelectionStateForAnalysis(1, 1, 0, nullptr, {nodes.at(routing[1]).get(), nodes.at(routing[2]).get(), nodes.at(routing[3]).get()}, 0);
                if (operation == 0) Check(widget.SetTextForAnalysis(value.c_str(), &error), error.c_str());
                else if (operation == 1) widget.SetColourForAnalysis(colour);
                else Check(operation == 2, "Known TextWidget fixture operation");
                MatchText(widget.GetTextForAnalysis(), input.Optional(), "Original TextWidget text getter");
                Match(widget.GetColourForAnalysis(), input.U32(), "Original TextWidget colour getter");
                Match(widget.GetNormalColourForAnalysis(), input.U32(), "Original TextWidget normal-only colour getter");
                for (unsigned i = 1; i <= nodeCount; ++i)
                {
                    const auto& text = texts[i]; MatchText(text && text->GetTextForAnalysis() ? text->GetTextForAnalysis()->c_str() : nullptr, input.Optional(), "Original target text");
                    Match(text ? text->GetColorForAnalysis() : 0, input.U32(), "Original target colour");
                    const auto words = input.U32(); Check(words == (text ? 7u : 0u), "Text layout words count");
                    if (text)
                    {
                        Match(text->GetWrapWidthForAnalysis(), input.U32(), "Original target wrap");
                        Match(text->GetAlignmentForAnalysis(), input.U32(), "Original target alignment");
                        Match(text->GetMeasuredWidthForAnalysis(), input.U32(), "Original target width");
                        for (auto f : text->GetBoundingSphereForAnalysis()) Match(Bits(f), input.U32(), "Original target sphere");
                    }
                    auto* node = dynamic_cast<spTextNode*>(nodes[i].get());
                    for (bool world : {false, true})
                    {
                        const auto sphereSize = input.U32(); Check(sphereSize == (node ? 4u : 0u), "Node sphere words count");
                        if (node)
                            for (auto f : world ? node->GetWorldBoundingSphereForAnalysis() : node->GetLocalBoundingSphereForAnalysis())
                                Match(Bits(f), input.U32(), world ? "Original TextNode world sphere" : "Original TextNode local sphere");
                    }
                }
                ++nativeCases;
            }
            catch (const std::exception& error) { throw std::runtime_error(label + ": " + error.what()); }
        }
        Check(input.input.peek() == std::char_traits<char>::eof(), "Complete TextWidget fixture read");
    }
    void ReplayCopy(const char* path)
    {
        Reader input(path); Check(input.U32() == 0x31435754, "TWC1 fixture"); const auto fontBytes = input.Blob();
        const auto count = input.U32(); Check(count <= 128, "Native TextWidget copy case bound");
        for (unsigned caseIndex = 0; caseIndex != count; ++caseIndex)
        {
            const auto label = input.Blob();
            try
            {
                const auto sourceRoot = input.U32(), destinationRoot = input.U32();
                std::array<unsigned, 3> states{}; for (auto& value : states) value = input.U32();
                const auto clone = input.U32(); Check(clone <= 1, "TextWidget clone flag");
                Host host; spSerializerManager serializers; spResourceManager resources; spFontManager layout;
                spSerializerReadContextForAnalysis context(serializers, resources); context.fontManager = &layout;
                spFont font; spMemoryStream stream; std::string error;
                Check(stream.ResizeAndSetSize(static_cast<unsigned>(fontBytes.size())), "Copy font stream allocation");
                std::memcpy(stream.GetBuffer(), fontBytes.data(), fontBytes.size());
                Check(stream.Seek(spStream::SeekSource::essStart, 0), "Copy font stream rewind");
                Check(spFontSerializer{}.ReadPayloadForAnalysis(context, stream, static_cast<unsigned>(fontBytes.size()), font, &error), "Copy actual shared Font reader");
                layout.SetLayoutDefaultFontForAnalysis(&font);
                std::vector<std::shared_ptr<spNode>> nodes(1); std::vector<std::shared_ptr<spTextRenderable>> texts(6);
                for (unsigned i = 1; i != 6; ++i)
                {
                    nodes.push_back(i == 4 ? std::make_shared<spNode>() : std::static_pointer_cast<spNode>(std::make_shared<spTextNode>()));
                    host.references[nodes[i].get()] = 4;
                    if (i >= 4) continue;
                    auto text = std::make_shared<spTextRenderable>(); const char initial[]{static_cast<char>(64+i), static_cast<char>(65+i), '\0'};
                    Check(text->SetTextForAnalysis(initial, &layout, &error), "Copy initial actual Text layout"); text->SetColorForAnalysis(0x11000000+i);
                    Check(static_cast<spTextNode*>(nodes[i].get())->AttachRenderableForAnalysis(text), "Copy actual TextNode attachment"); texts[i] = std::move(text);
                }
                auto source = std::make_unique<spTextWidget>(host);
                source->SetReferenceForAnalysis(nodes.at(sourceRoot).get());
                source->SetSelectionStateForAnalysis(128, 23, 255, nodes[2].get(),
                    {nodes.at(states[0]).get(), nodes.at(states[1]).get(), nodes.at(states[2]).get()}, 0x87654321);
                spCloneManager manager; std::unique_ptr<spBaseObject> destination;
                if (!clone)
                {
                    auto target = std::make_unique<spTextWidget>(host); target->SetReferenceForAnalysis(nodes.at(destinationRoot).get());
                    target->SetSelectionStateForAnalysis(1, 0x25, 0, nullptr, {}, 0); destination = std::move(target);
                }
                // These counts are the literal pre-call reference state of the
                // native fixture, independent of the fixture binding producer.
                for (unsigned i = 1; i != 6; ++i) host.references[nodes[i].get()] = 4;
                bool returned = true;
                if (clone) destination = source->vfunc_10(manager);
                else returned = source->vfunc_14(*destination, manager);
                auto* target = dynamic_cast<spTextWidget*>(destination.get()); Check(target != nullptr, "Copy concrete TextWidget destination");
                Match(returned, input.U32(), "Original TextWidget copy return");
                const auto idof = [&](const spNode* node)
                { for (unsigned i = 0; i != nodes.size(); ++i) if (nodes[i].get() == node) return i; throw std::logic_error("Unknown graph pointer"); };
                Match(idof(target->GetNodeForGUIForAnalysis()), input.U32(), "Original destination root retained");
                Match(target->GetEnabledByteForAnalysis(), input.U32(), "Original copied enabled byte");
                Match(target->GetVisibleByteForAnalysis(), input.U32(), "Original destination visible byte retained");
                Match(target->GetHighlightByteForAnalysis(), input.U32(), "Original copied highlight byte");
                Match(idof(target->GetSelectedNodeForAnalysis()), input.U32(), "Original copied selection");
                for (const auto* node : target->GetStateNodesForAnalysis()) Match(idof(node), input.U32(), "Original shallow state node copy");
                Match(target->GetSelectionIndexForAnalysis(), input.U32(), "Original copied selection index");
                Match(manager.FindClone(*source) == target, input.U32(), "Original concrete clone map registration");
                for (unsigned i = 1; i != 4; ++i)
                {
                    const auto& text = texts[i]; MatchText(text->GetTextForAnalysis()->c_str(), input.Optional(), "Original copied target text");
                    Match(text->GetColorForAnalysis(), input.U32(), "Original copied target colour");
                    Check(input.U32() == 7, "Copied Text layout words count");
                    Match(text->GetWrapWidthForAnalysis(), input.U32(), "Original copied target wrap");
                    Match(text->GetAlignmentForAnalysis(), input.U32(), "Original copied target alignment");
                    Match(text->GetMeasuredWidthForAnalysis(), input.U32(), "Original copied target width");
                    for (auto f : text->GetBoundingSphereForAnalysis()) Match(Bits(f), input.U32(), "Original copied target sphere");
                    const auto* node = static_cast<const spTextNode*>(nodes[i].get());
                    for (bool world : {false, true})
                    {
                        Check(input.U32() == 4, "Copied Node sphere words count");
                        for (auto f : world ? node->GetWorldBoundingSphereForAnalysis() : node->GetLocalBoundingSphereForAnalysis())
                            Match(Bits(f), input.U32(), world ? "Original copied world sphere" : "Original copied local sphere");
                    }
                }
                for (unsigned i = 1; i != 6; ++i) Match(host.references[nodes[i].get()], input.U32(), "Original reference before widget deletion");
                destination.reset(); source.reset(); Match(1, input.U32(), "Original widget allocations released");
                for (unsigned i = 1; i != 6; ++i) Match(host.references[nodes[i].get()], input.U32(), "Original reference after widget deletion");
                ++nativeCases;
            }
            catch (const std::exception& error) { throw std::runtime_error(label + ": " + error.what()); }
        }
        Check(input.input.peek() == std::char_traits<char>::eof(), "Complete TextWidget copy fixture read");
    }
}
int main(int argc, char** argv)
{
    try
    {
        Basic();
        for (int i = 1; i < argc; ++i)
        {
            Reader header(argv[i]); const auto magic = header.U32();
            if (magic == 0x31435754) ReplayCopy(argv[i]);
            else Replay(argv[i]);
        }
        std::cout << "TextWidget checks=" << checks << " nativeCases=" << nativeCases << " scalarComparisons=" << comparisons << '\n'; return 0;
    }
    catch (const std::exception& error) { std::cerr << "TextWidget failure: " << error.what() << '\n'; return 1; }
}

#include "Code/SparkplugDX/spDXRenderTarget.h"
#include "Code/SparkplugDX/spDXCubeRenderTarget.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace sparkplug::reconstruction;
using sparkplug::analysis::host::RenderTargetSurfaceDescription;

namespace
{
    unsigned checks = 0;
    void Check(const bool result, const char* const label)
    {
        ++checks;
        if (!result) throw std::runtime_error(label);
    }

    struct SurfaceProvider final
    {
        RenderTargetSurfaceDescription description;
        unsigned calls = 0;
        bool valid = true;
        std::uint32_t hresult = 0;

        static bool Read(void* const context,
            RenderTargetSurfaceDescription& description)
        {
            auto& self = *static_cast<SurfaceProvider*>(context);
            ++self.calls;
            if (!self.valid) return false;
            description = self.description;
            // Original GetDesc HRESULT is discarded. Only initialized fields
            // determine whether our HOST provider can expose the descriptor.
            return true;
        }
    };

    struct OriginalMeasurement final
    {
        std::uint32_t format, width, height, ordinary, cube;
    };

    // Outcomes selected from complete original descriptor->return operations.
    // None is a texture allocation estimate: height and six face descriptions
    // do not participate. Odd DXT widths use native floor division.
    constexpr std::array<OriginalMeasurement, 20> Original{
        OriginalMeasurement{0x15, 256, 123, 1024, 6144},
        {0x16, 7, 123, 28, 168},
        {0x17, 7, 123, 14, 84},
        {0x1A, 256, 123, 512, 3072},
        {0x18, 256, 123, 256, 1536},
        {0x19, 256, 123, 256, 1536},
        {0x31545844, 0, 123, 8, 48},
        {0x31545844, 3, 123, 8, 48},
        {0x31545844, 7, 123, 8, 48},
        {0x33545844, 7, 123, 16, 96},
        {0x35545844, 256, 123, 1024, 6144},
        {0, 7, 123, 7, 42},
        {0xFFFFFFFF, 256, 123, 256, 1536},
        {0x15, 0x40000001, 123, 4, 24},
        {0x16, 0xFFFFFFFF, 123, 0xFFFFFFFC, 0xFFFFFFE8},
        {0x17, 0xFFFFFFFF, 123, 0xFFFFFFFE, 0xFFFFFFF4},
        {0x31545844, 0xFFFFFFFF, 123, 0xFFFFFFF8, 0xFFFFFFD0},
        {0x33545844, 0xFFFFFFFF, 123, 0xFFFFFFF0, 0xFFFFFFA0},
        {0x35545844, 0x40000001, 123, 0, 0},
        {0x19, 0xFFFFFFFF, 123, 0xFFFFFFFF, 0xFFFFFFFA},
    };
}

int main(const int argc, const char* const argv[])
{
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--surface-table")
        {
            constexpr std::array<std::uint32_t, 11> formats{
                0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x31545844,
                0x33545844, 0x35545844, 0, 0xFFFFFFFF};
            constexpr std::array<std::uint32_t, 8> widths{
                0, 1, 3, 4, 7, 256, 0x40000001, 0xFFFFFFFF};
            for (unsigned cube = 0; cube < 2; ++cube)
                for (const auto format : formats)
                    for (const auto width : widths)
                    {
                        SurfaceProvider provider{{format, width, 123}};
                        std::uint32_t size = 0;
                        const auto success = cube
                            ? spDXCubeRenderTarget::ReadBackingTextureSizeForAnalysis(
                                &provider, SurfaceProvider::Read, size)
                            : spDXRenderTarget::ReadBackingTextureSizeForAnalysis(
                                &provider, SurfaceProvider::Read, size);
                        Check(success && provider.calls == 1,
                            "one complete provider call per exported measurement");
                        std::cout << cube << ' ' << format << ' ' << width
                            << ' ' << size << '\n';
                    }
            return 0;
        }
        Check(argc == 1, "recognized test arguments only");
        for (const auto& original : Original)
        {
            for (const auto hresult : {0u, 0x8876086Cu})
            {
                SurfaceProvider provider{
                    {original.format, original.width, original.height},
                    0, true, hresult};
                std::uint32_t ordinary = 0xDEADBEEF, cube = 0xDEADBEEF;
                Check(spDXRenderTarget::ReadBackingTextureSizeForAnalysis(
                    &provider, SurfaceProvider::Read, ordinary),
                    "ordinary complete descriptor operation");
                Check(ordinary == original.ordinary && provider.calls == 1,
                    "ordinary matches original return and one GetDesc");
                Check(spDXCubeRenderTarget::ReadBackingTextureSizeForAnalysis(
                    &provider, SurfaceProvider::Read, cube),
                    "cube complete descriptor operation");
                Check(cube == original.cube && provider.calls == 2,
                    "cube matches original return with only face-zero GetDesc");
                provider.description.height = 0xFFFFFFFF;
                Check(spDXRenderTarget::CalculateBackingTextureSizeForAnalysis(
                    provider.description) == original.ordinary
                    && spDXCubeRenderTarget::CalculateBackingTextureSizeForAnalysis(
                        provider.description) == original.cube,
                    "height does not change the native size value");
            }
        }
        SurfaceProvider provider{{0x15, 256, 256}, 0, false, 0x8876086C};
        std::uint32_t size = 0xDEADBEEF;
        Check(!spDXRenderTarget::ReadBackingTextureSizeForAnalysis(
            &provider, SurfaceProvider::Read, size) && size == 0xDEADBEEF,
            "HOST refuses unwritten descriptor without inventing native success");
        Check(!spDXCubeRenderTarget::ReadBackingTextureSizeForAnalysis(
            &provider, SurfaceProvider::Read, size) && size == 0xDEADBEEF,
            "HOST cube refusal preserves caller output");
        Check(!spDXRenderTarget::ReadBackingTextureSizeForAnalysis(
            &provider, nullptr, size)
            && !spDXCubeRenderTarget::ReadBackingTextureSizeForAnalysis(
                &provider, nullptr, size)
            && size == 0xDEADBEEF && provider.calls == 2,
            "missing backend descriptor dependency never invokes a default");
        std::cout << "PASS " << checks << '/' << checks
            << ": render-target surface descriptor operations\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

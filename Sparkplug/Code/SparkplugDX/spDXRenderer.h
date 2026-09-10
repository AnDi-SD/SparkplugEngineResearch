#pragma once

// Inferred header path. The exact surviving implementation path is
// Z:\Sparkplug\Code\SparkplugDX\spDXRenderer_Init.cpp.

#include "../Sparkplug/spRenderer.h"
#include "spDXShader.h"
#include "../Sparkplug/spLightManager.h"
#include "Analysis/PC/spRendererMatrixState.h"
#include <map>
#include <memory>
#include <array>
#include <optional>
#include <vector>

namespace sparkplug::reconstruction
{
    class spPCVertexDeclaration;
    class spDXMaterial;
    class spDXTexture;
    class spDXLight;
    class spMaterialPassLayer;
    class spMaterialTexture;
    class spPCShaderManager;
    struct spPCShaderGenerationForAnalysis;
    class spMaterial;
    class spFog;
    class spDXIndexBuffer;
    class spDXVertexBuffer;
    class spDXRenderer : public spRenderer
    {
      public:
        static constexpr spClassID ClassID = 0x46004EE1;
        static constexpr std::size_t TextureStateCacheCount = 72;

        using MatrixStateForAnalysis=sparkplug::evidence::pc::RendererMatrixStateForAnalysis;
        // Original4AD540 and lazy getters4AD640/4AD680/4AD660; analytical
        // completion/pointer APIs, not native return-value or layout claims.
        [[nodiscard]] static bool RefreshMatricesForAnalysis(MatrixStateForAnalysis&) noexcept;
        [[nodiscard]] static const MatrixStateForAnalysis::RawMatrix* GetCachedMatrixForAnalysis(
            MatrixStateForAnalysis&,unsigned cacheIndex) noexcept;
        using MatrixInputSubmitForAnalysis=std::int32_t (*)(void*,std::uint32_t,const MatrixStateForAnalysis::RawMatrix&) noexcept;
        // PC4BBB60/4BBB20/4BBAE0. Matrix copy + dirty set BEFORE device call;
        // no value comparison. World setter's second argument is unused.
        [[nodiscard]] static bool SetInputMatrixForAnalysis(MatrixStateForAnalysis&,unsigned inputIndex,
            const MatrixStateForAnalysis::RawMatrix&,std::uint32_t unusedWorldArgument,
            MatrixInputSubmitForAnalysis,void*) noexcept;
        [[nodiscard]] static const MatrixStateForAnalysis::RawMatrix* GetInputMatrixForAnalysis(
            const MatrixStateForAnalysis&,unsigned inputIndex) noexcept;

        ~spDXRenderer() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

        using RenderStateSubmitForAnalysis = std::int32_t (*)(void* context, std::uint32_t index,
                                                              std::uint32_t value) noexcept;
        struct SceneStateForAnalysis final
        {
            std::uint32_t counter40=0;
            std::uint32_t resetWord4C=0; // reset at BeginScene; full semantics not named
            std::uint8_t activeCBC0=0;
        };
        using DeviceSceneCallForAnalysis=std::int32_t (*)(void*,bool begin) noexcept;
        // PC4BB950/4BB9D0. Original device call happens BEFORE cache mutation,
        // HRESULT is ignored; no begin/end nesting guard in these wrappers.
        [[nodiscard]] static bool BeginSceneForAnalysis(SceneStateForAnalysis&,DeviceSceneCallForAnalysis,void*) noexcept;
        [[nodiscard]] static bool EndSceneForAnalysis(SceneStateForAnalysis&,DeviceSceneCallForAnalysis,void*) noexcept;
        using DeviceClearForAnalysis=std::int32_t (*)(void*,std::uint32_t flags,std::uint32_t argb,float depth,std::uint32_t stencil) noexcept;
        [[nodiscard]] static bool ClearForAnalysis(std::uint32_t flags,std::uint32_t argb,std::uint32_t stencil,DeviceClearForAnalysis,void*) noexcept;
        using DevicePresentForAnalysis=std::int32_t (*)(void*,bool testCooperativeLevel) noexcept;
        struct PresentBoundaryForAnalysis final
        {
            bool completed=false,nativeResult=false,resetRequired=false;
            std::array<std::uint32_t,8> resetParameters{};
        };
        // PC4BB9F0: reset-needed path stops BEFORE internal slot8/4BD810.
        // completed=false/resetRequired=true is NOT native success or reset.
        [[nodiscard]] static PresentBoundaryForAnalysis PresentBeforeResetForAnalysis(
            std::uint32_t inhibitC1C0,const std::array<std::uint32_t,8>& parameters,
            DevicePresentForAnalysis,void*) noexcept;
        // PC4B0A90, analytical method name. Caller supplies the entry at
        // native E4F4+4*index; full array extent/startup are NOT invented.
        // Device HRESULT is ignored and a changed entry is cached regardless.
        // Null callback with a changed value is a host-only rejection guard.
        [[nodiscard]] static bool ApplyRenderStateCacheEntryForAnalysis(
            std::uint32_t& cachedEntry, std::uint32_t index, std::uint32_t value,
            RenderStateSubmitForAnalysis submit, void* context) noexcept;
        struct FogStateForAnalysis final
        {
            const spFog* current=nullptr; // borrowed PC CA20
            const spFog* fallback=nullptr; // borrowed PC E4A0
        };
        // PC4AD390 secondary receiver. Identity changes are published before
        // type validation; repeat identity suppresses mutable payload reads.
        [[nodiscard]] static bool ApplyFogForAnalysis(FogStateForAnalysis&,const spFog*,
            std::array<std::uint32_t,256>& deviceStates,RenderStateSubmitForAnalysis,void*) noexcept;
        struct LightingStateForAnalysis final
        {
            // Caller-supplied E4A4/E4B4/E4C4/E4D4 andE4E4; NOT ctor defaults.
            std::array<float,4> diffuse{},ambient{},specular{},emissive{};
            float specularPower=0;
            std::uint32_t packedColorC194=0,diffuseSource=0,ambientSource=0;
            std::uint32_t globalBlackARGB=0xff000000; // mutable original73FE98
        };
        // PC4BE180: borrow owner and copy17 words BEFORE DX4A9530 update.
        // Uninitialized native power/NULL input remain explicit host guards.
        [[nodiscard]] static bool InstallMaterialForAnalysis(LightingStateForAnalysis&,
            spDXMaterial*& borrowedOwner,spDXMaterial*,std::uint32_t frame,bool* evaluated=nullptr);
        using MaterialStateWordsForAnalysis=std::array<std::uint32_t,11>;
        struct MaterialStateOverridesForAnalysis final
        {
            // Declared source slots, NOT a claim about native array extent.
            // Slot0 is always replaced by the selected material state array.
            const MaterialStateWordsForAnalysis* const* sources=nullptr;
            std::size_t count=0;
            std::array<std::uint32_t,10> selectors{};
        };
        // PC4BB890; force raw lighting cache255, then changed indices1..10.
        [[nodiscard]] static bool ApplyMaterialStateSetForAnalysis(
            MaterialStateWordsForAnalysis& raw,const MaterialStateWordsForAnalysis& material,
            const MaterialStateOverridesForAnalysis*,LightingStateForAnalysis&,
            RenderStateSubmitForAnalysis,void*) noexcept;
        [[nodiscard]] static bool ApplyMaterialColorSourceForAnalysis(
            std::uint32_t& rawSource,bool ambient,std::uint32_t value,
            RenderStateSubmitForAnalysis dispatch,void*) noexcept;
        [[nodiscard]] static bool ApplyMaterialLightingForAnalysis(
            LightingStateForAnalysis&,std::uint32_t mode,RenderStateSubmitForAnalysis,void*) noexcept;
        // PC4B0AD0 maps one raw material state to native device-state indices.
        // Caller supplies C868+4*index and a dispatcher through4B0A90's cache
        // equivalent. State8 requires explicit lighting input (NULL host false).
        // Out-of-table values for indices2/3/6/10 are guarded, not native OOB.
        [[nodiscard]] static bool ApplyMaterialRenderStateForAnalysis(
            std::uint32_t& engineCacheEntry,std::uint32_t index,std::uint32_t value,
            RenderStateSubmitForAnalysis dispatch,void* context,
            LightingStateForAnalysis* lighting=nullptr) noexcept;
        struct TextureStageCacheForAnalysis final
        {
            std::array<std::uint32_t,9> raw{}; // caller-declared input, not ctor defaults
            std::uint32_t coordinateIndex=0,transformFlags=0;
        };
        using TextureStateSubmitForAnalysis=std::int32_t (*)(void*,bool sampler,
            std::uint32_t stage,std::uint32_t index,std::uint32_t value) noexcept;
        // PC4BB1F0: rawC898 stage stride9; device cache E920/E954 stride100.
        // overrideCoordinates represents E454[E474]!=0, not an inferred enum.
        [[nodiscard]] static bool ApplyTextureStateForAnalysis(
            TextureStageCacheForAnalysis&,std::uint32_t stage,std::uint32_t index,
            std::uint32_t value,bool overrideCoordinates,TextureStateSubmitForAnalysis,void*) noexcept;
        struct ResolvedTextureBindingForAnalysis final
        {
            // Explicit consumer input AFTER native RTTI/resource resolution.
            // Opaque tokens are not live host COM pointers/owned resources.
            std::uintptr_t identity=0,deviceTexture=0;
            std::optional<std::uint32_t> paletteIndex;
        };
        using DeviceTextureBindForAnalysis=std::int32_t (*)(void*,std::uint32_t,std::uintptr_t) noexcept;
        // Only the external COM handle is supplied; class, identity and palette
        // are resolved from the actual reconstructed DXTexture by the caller.
        using DeviceTextureHandleForAnalysis=std::optional<std::uintptr_t> (*)(void*,const spDXTexture&) noexcept;
        using DevicePaletteSelectForAnalysis=std::int32_t (*)(void*,std::uint32_t) noexcept;
        [[nodiscard]] static bool SelectPaletteForAnalysis(std::uint32_t& cachedIndex,
            std::optional<std::uint32_t> index,DevicePaletteSelectForAnalysis,void*) noexcept;
        // PC4BB650 identity-cache/COM portion, plus actual4BB1C0. Full native
        // alternate resource RTTI branches are NOT replaced with fake objects.
        [[nodiscard]] static bool BindResolvedTextureForAnalysis(std::uintptr_t& cachedIdentity,
            std::uint32_t& cachedPalette,std::uint32_t stage,const ResolvedTextureBindingForAnalysis&,
            bool debugDisable,DeviceTextureBindForAnalysis,DevicePaletteSelectForAnalysis,void*) noexcept;
        using TextureStateBlockForAnalysis=std::array<std::array<std::uint32_t,9>,8>;
        struct PassTextureStateForAnalysis final
        {
            TextureStateBlockForAnalysis desired{};
            std::array<TextureStageCacheForAnalysis,8> cache{};
            std::array<std::uintptr_t,8> desiredTextures{},boundTextures{};
            std::array<std::uint8_t,8> dirty{};
            std::uint32_t palette=0;
        };
        struct PassTextureOverridesForAnalysis final
        {
            const TextureStateBlockForAnalysis* sources=nullptr;
            std::size_t count=0;
            TextureStateBlockForAnalysis selectors{};
        };
        // PC4BBBA0 on known texture-layer family. Bindings are explicit resolved
        // device inputs keyed by actual texture identity; mismatch is rejected.
        // Fallbacks are C9C0's first pass's first two material textures.
        [[nodiscard]] static bool ApplyPassTextureStatesForAnalysis(PassTextureStateForAnalysis&,
            const spMaterialPassLayer&,const std::array<const spMaterialTexture*,2>& fallbacks,
            const std::array<ResolvedTextureBindingForAnalysis,8>& bindings,const PassTextureOverridesForAnalysis*,
            bool shaderActive,bool debugDisable,DeviceTextureBindForAnalysis,DevicePaletteSelectForAnalysis,
            TextureStateSubmitForAnalysis,void*);
        // The final part of4BC410, called AFTER successful4BBBA0. Mutates
        // selected material state7 even when an override supplies device value.
        [[nodiscard]] static bool ApplyPassBlendForAnalysis(MaterialStateWordsForAnalysis& raw,
            spDXMaterial&,const spMaterialPassLayer&,const MaterialStateOverridesForAnalysis*,
            RenderStateSubmitForAnalysis,void*) noexcept;
        struct ShaderBindingForAnalysis final {std::uintptr_t identity=0,deviceShader=0;};
        // PC4BE310 prefix, before manager4C8980. Inputs preserve raw native
        // shifts/overlapping bits; oversized light arrays are a host guard.
        [[nodiscard]] static std::optional<std::array<std::uint32_t,2>> BuildShaderKeyForAnalysis(
            std::uint32_t componentFlags,std::uint32_t rawColorMode,float specularPower,
            const std::vector<std::uint32_t>* lightTypes,const std::array<std::uint32_t,8>& rawUVFlags) noexcept;
        struct ShaderStackForAnalysis final
        {
            std::array<ShaderBindingForAnalysis,4> entries{};
            std::uint32_t top=0;
        };
        // Original push/pop has no bounds check; portable guard avoids OOB.
        [[nodiscard]] static bool PushShaderForAnalysis(ShaderStackForAnalysis&,ShaderBindingForAnalysis) noexcept;
        [[nodiscard]] static bool PopShaderForAnalysis(ShaderStackForAnalysis&) noexcept;
        struct DrawStateForAnalysis final
        {
            ShaderStackForAnalysis vertex,pixel;
            std::uintptr_t boundVertex=0,boundPixel=0;
            bool pixelEnabled=false;
            std::vector<std::array<std::uint32_t,4>> vertexConstants,pixelConstants;
        };
        using DeviceShaderBindForAnalysis=std::int32_t (*)(void*,bool pixel,std::uintptr_t handle) noexcept;
        using DeviceShaderConstantsForAnalysis=std::int32_t (*)(void*,bool pixel,const std::array<std::uint32_t,4>*,std::size_t rows) noexcept;
        using DevicePrimitiveDrawForAnalysis=std::int32_t (*)(void*,bool indexed,const std::array<std::uint32_t,6>& args) noexcept;
        // Actual4BC290 branch with non-NULL preselected vertex shader only.
        // NULL refuses automatic4BE310/4BE2B0 path; no fake generated shader.
        // Seven raw arguments retain original positions pending full API names.
        [[nodiscard]] static bool DrawPreselectedForAnalysis(DrawStateForAnalysis&,
            const std::array<std::uint32_t,7>& args,DeviceShaderBindForAnalysis,
            DeviceShaderConstantsForAnalysis,DevicePrimitiveDrawForAnalysis,void*) noexcept;
        // Actual NULL-selection branch of4BC290 for no blend weights only.
        // Uses the common key/manager and resolved draw core. Manager injection
        // is a host lifetime boundary, not reconstructed process startup.
        [[nodiscard]] static bool DrawWithoutBlendWeightsForAnalysis(DrawStateForAnalysis&,
            const std::array<std::uint32_t,7>& args,spPCShaderManager&,const spMaterial&,
            std::uint32_t rawColorMode,const std::vector<std::uint32_t>* lights,
            const std::array<std::uint32_t,8>& uv,DeviceShaderBindForAnalysis,
            DeviceShaderConstantsForAnalysis,DevicePrimitiveDrawForAnalysis,void*) noexcept;
        // Complete4BC290 automatic cache HIT, including weighted parameters.
        // A generating miss and uninitialized scratch words remain refused.
        // Auto selection is cleared AFTER draw; device binding is retained.
        [[nodiscard]] static bool DrawCachedAutomaticForAnalysis(DrawStateForAnalysis&,
            const std::array<std::uint32_t,7>&,spPCShaderManager&,
            const spDXShader::ConstantInputsForAnalysis&,std::uint32_t rawColorMode,
            const std::vector<std::uint32_t>* lightTypes,const std::array<std::uint32_t,8>& uv,
            DeviceShaderBindForAnalysis,DeviceShaderConstantsForAnalysis,DevicePrimitiveDrawForAnalysis,void*);
        // Automatic draw with optional explicit compiler/device generation.
        // Existing bounds on fully initialized constant scratch remain.
        [[nodiscard]] static bool DrawAutomaticForAnalysis(DrawStateForAnalysis&,
            const std::array<std::uint32_t,7>&,spPCShaderManager&,
            const spDXShader::ConstantInputsForAnalysis&,std::uint32_t rawColorMode,
            const std::vector<std::uint32_t>* lightTypes,const std::array<std::uint32_t,8>& uv,
            DeviceShaderBindForAnalysis,DeviceShaderConstantsForAnalysis,DevicePrimitiveDrawForAnalysis,void*,
            const spPCShaderGenerationForAnalysis* generation=nullptr);

        using TextureMatrix4ForAnalysis=std::array<float,16>;
        using TextureMatrix3ForAnalysis=std::array<float,9>;
        using TextureTransformSubmitForAnalysis=std::int32_t (*)(void*,std::uint32_t,const TextureMatrix4ForAnalysis&) noexcept;
        // PC4BB590/4BB4B0. Caller supplies exactly the cache entry at native
        // F0F4+64*stage; full storage extent/startup is not invented here.
        // Both always cache and submit stage+16, ignoring device HRESULT.
        [[nodiscard]] static bool ApplyTextureTransform4ForAnalysis(
            TextureMatrix4ForAnalysis& cache,std::uint32_t stage,const TextureMatrix4ForAnalysis& matrix,
            TextureTransformSubmitForAnalysis submit,void* context) noexcept;
        [[nodiscard]] static bool ApplyTextureTransform3ForAnalysis(
            TextureMatrix4ForAnalysis& cache,std::uint32_t stage,const TextureMatrix3ForAnalysis& matrix,
            TextureTransformSubmitForAnalysis submit,void* context) noexcept;

        struct GeometryBindingForAnalysis final
        {
            const spPCVertexDeclaration* declaration=nullptr; // borrowed C9FC
            std::shared_ptr<spDXIndexBuffer> indices; // retained CA04
            std::shared_ptr<spDXVertexBuffer> vertices; // retained CA08
        };
        struct SubmissionStateForAnalysis final
        {
            GeometryBindingForAnalysis geometry;
            MaterialStateWordsForAnalysis raw{};
            LightingStateForAnalysis lighting;
            spDXMaterial* installedMaterial=nullptr;
            PassTextureStateForAnalysis textures;
            DrawStateForAnalysis draw;
            std::array<std::uint32_t,256> deviceStates{}; // declared subset, not ctor defaults
            std::array<TextureMatrix4ForAnalysis,8> uv{};
            std::uint32_t frame=0;
        };
        // PC-only known subset after fresh4C5AB0 then4BCF20. Cold original
        // selector431760 rejected all768 IDs: invalid cache words are NOT
        // device defaults. Five desired writes use the actual cached writer;
        // HRESULT is ignored. NULL callback is a host refusal before mutation.
        // This does NOT create a ready-to-draw context. Preserve unknown or
        // deliberately omitted caller inputs (including native E47C identity):
        // geometry, installed material, lighting payload/power/packed color/
        // globalBlackARGB, desired texture identities, draw state
        // and UV matrices. Their validity is the consumer's responsibility.
        // Fog/light caches outside this DTO and live-device startup are omitted.
        [[nodiscard]] static bool InitializePCSubmissionCachesForAnalysis(
            SubmissionStateForAnalysis&,RenderStateSubmitForAnalysis,void*) noexcept;
        using DeviceGeometryBindForAnalysis=std::int32_t (*)(void*,unsigned kind,
            std::uintptr_t handle,std::uint32_t stride) noexcept; // 0 declaration,1 indices,2 stream0
        using DeviceMaterialForAnalysis=std::int32_t (*)(void*,const LightingStateForAnalysis&) noexcept;
        struct DeviceLightInputForAnalysis final
        {
            std::uintptr_t identity=0;
            std::uint32_t type=0;
            bool enabled=false;
            std::array<float,4> color{};
            std::array<std::uint32_t,26> deviceWords{}; // original F0..157, producer separate
            const spDXLight* sourceObject=nullptr; // optional original light fields for shader consumer
        };
        struct LightListInputForAnalysis final
        {
            std::vector<const DeviceLightInputForAnalysis*> lights;
            const DeviceLightInputForAnalysis* ambient=nullptr;
        };
        // Resolve the actual selected cache to the same light objects consumed
        // by4BDE50/4AE930. Caller keeps cache, objects and storage alive. Unknown
        // device words and optional identity mapping are explicit boundary inputs.
        using LightIdentityForAnalysis=std::uintptr_t (*)(const spLight&) noexcept;
        [[nodiscard]] static bool ResolveLightCacheForAnalysis(const spLightManager::CacheForAnalysis&,
            std::array<DeviceLightInputForAnalysis,9>& storage,LightListInputForAnalysis&,
            std::uint32_t unknownDeviceWord,LightIdentityForAnalysis identity=nullptr);
        struct LightSubmissionStateForAnalysis final
        {
            std::uint32_t previousCount=0; // native process-global764340, not ctor default
            std::uintptr_t borrowedList=0; // C190: non-NULL call leaves it unchanged!
            std::array<std::uintptr_t,24> grouped{}; // three groups of eight, F2F8
            std::array<float,4> ambient{};
            std::uint32_t deviceAmbient=0;
        };
        using DeviceLightSubmitForAnalysis=std::int32_t (*)(void*,std::uint32_t,const std::array<std::uint32_t,26>&) noexcept;
        using DeviceLightEnableForAnalysis=std::int32_t (*)(void*,std::uint32_t,bool) noexcept;
        struct LightSubmissionForAnalysis final
        {
            LightSubmissionStateForAnalysis state;
            const LightListInputForAnalysis* input=nullptr;
            std::uint32_t fallbackARGB=0;
        };
        // PC4BDE50: NULL disables8 but preserves grouped/previousCount; an
        // empty non-NULL list clears groups and updates previousCount to0.
        [[nodiscard]] static bool SubmitLightsForAnalysis(LightSubmissionStateForAnalysis&,
            const LightListInputForAnalysis*,std::uint32_t fallbackARGB,
            DeviceLightSubmitForAnalysis,DeviceLightEnableForAnalysis,RenderStateSubmitForAnalysis,void*) noexcept;
        struct SubmissionDeviceForAnalysis final
        {
            DeviceGeometryBindForAnalysis geometry=nullptr;
            RenderStateSubmitForAnalysis render=nullptr;
            DeviceMaterialForAnalysis material=nullptr;
            DeviceTextureBindForAnalysis texture=nullptr;
            DevicePaletteSelectForAnalysis palette=nullptr;
            TextureStateSubmitForAnalysis textureState=nullptr;
            TextureTransformSubmitForAnalysis transform=nullptr;
            DeviceShaderBindForAnalysis shader=nullptr;
            DeviceShaderConstantsForAnalysis constants=nullptr;
            DevicePrimitiveDrawForAnalysis draw=nullptr;
            void* context=nullptr;
            DeviceTextureHandleForAnalysis textureHandle=nullptr;
            DeviceLightSubmitForAnalysis light=nullptr;
            DeviceLightEnableForAnalysis lightEnable=nullptr;
        };
        // PC4BC4A0 full chain for actual material state8==2 and no blend weights.
        // Shared ownership is the safe host analogue of native uint16 refs.
        // handles={declaration,index,vertex} are explicit COM consumer inputs.
        // All known internal steps reuse their reconstructed implementations;
        // lit/unknown texture resources are refused, not fake success. Weighted
        // path requires constants plus an existing cache or explicit compiler/
        // device generation inputs for the original miss path.
        [[nodiscard]] static bool SubmitUnlitGeometryForAnalysis(SubmissionStateForAnalysis&,
            const spPCVertexDeclaration*,const std::shared_ptr<spDXIndexBuffer>&,
            const std::shared_ptr<spDXVertexBuffer>&,const std::array<std::uintptr_t,3>& handles,
            std::uint32_t stride,const std::array<std::uint32_t,7>& drawArgs,
            spDXMaterial&,spDXMaterial& fallback,spPCShaderManager&,const SubmissionDeviceForAnalysis&,
            const spDXShader::ConstantInputsForAnalysis* constants=nullptr,
            const spPCShaderGenerationForAnalysis* generation=nullptr);
        // Full PC4BC4A0 light branch uses the original shared light consumer.
        // Caller-owned light selection/cache mirrors C190/764340/F2F8; it is
        // separate from material color state. Disabled lights still enter key.
        [[nodiscard]] static bool SubmitGeometryForAnalysis(SubmissionStateForAnalysis&,
            const spPCVertexDeclaration*,const std::shared_ptr<spDXIndexBuffer>&,
            const std::shared_ptr<spDXVertexBuffer>&,const std::array<std::uintptr_t,3>&,
            std::uint32_t,const std::array<std::uint32_t,7>&,spDXMaterial&,spDXMaterial&,
            spPCShaderManager&,const SubmissionDeviceForAnalysis&,
            const spDXShader::ConstantInputsForAnalysis* constants=nullptr,
            const spPCShaderGenerationForAnalysis* generation=nullptr,
            LightSubmissionForAnalysis* lights=nullptr);

        // PC4AE0E0: key is engine component mask, value is a declaration object,
        // not a numeric FVF handle. Portable CPU declaration only, no COM/GPU.
        // Shared host ownership deliberately keeps borrowed mesh references
        // safe if the analytical renderer is destroyed before those meshes.
        [[nodiscard]] std::shared_ptr<spPCVertexDeclaration>
            GetVertexDeclarationForAnalysis(std::uint32_t componentFlags);
        [[nodiscard]] std::size_t GetVertexDeclarationCountForAnalysis() const noexcept
        { return vertexDeclarations_.size(); }
        // PC4AE140 ends the lookup map lifetime, leaving borrowed declarations
        // untouched. Host shared owners preserve declarations retained by meshes.
        void ClearVertexDeclarationsForAnalysis() noexcept { vertexDeclarations_.clear(); }

      protected:
        spDXRenderer();
      private:
        [[nodiscard]] static bool DrawResolvedForAnalysis(DrawStateForAnalysis&,
            const std::array<std::uint32_t,7>&,DeviceShaderBindForAnalysis,
            DeviceShaderConstantsForAnalysis,DevicePrimitiveDrawForAnalysis,void*) noexcept;
        std::map<std::uint32_t,std::shared_ptr<spPCVertexDeclaration>> vertexDeclarations_;
    };
} // namespace sparkplug::reconstruction

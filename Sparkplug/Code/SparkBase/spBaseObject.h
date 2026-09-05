#pragma once

// Inferred header path.  The original translation unit name is proven, but no
// original header path or C++ namespace has been recovered.  The
// sparkplug::reconstruction namespace and the slot-based method names below are
// deliberately analytical; the leaf class names are present in both shipped
// executables.

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace sparkplug::reconstruction
{
    using spClassID = std::uint32_t;

    class spBaseObject;
    class spCloneManager;
    class spMemoryStream;
    class spApp;
    class spPCFileStream;
    class spPCAsyncFileStreamManager;
    class spPCKManager;
    class spPS2Helper;
    class spPS2IOPModuleManager;
    class spEngineCore;
    class spError;
    class spDXSharedMeshData;
    class spDXCombinedVB;
    class spDXSharedMeshDataSerializer;
    class spDXMesh;
    class spDXMeshSerializer;
    class spRenderNode;
    class spRenderNodeSerializer;
    class spSkin;
    class spSkinSerializer;
    class spPCRenderer;
    class spPS2Renderer;
    class spDXCubeRenderTarget;
    class spPCRenderTarget;
    class spPS2RenderTarget;
    class spPS2CubeRenderTarget;
    class spPCRenderTargetManager;
    class spPS2RenderTargetManager;
    class spMaterialTexture;
    class spMaterialRenderTargetTexture;
    class spMaterialCameraViewTexture;
    class spMaterialCubeMapTexture;
    class spMaterialPassLayer;
    class spMaterialTextureLayer;
    class spStdLayer;

    // Analytical, portable representation of the native 32-bit registration
    // record.  The byte-exact evidence structure lives in Analysis/PS2.
    struct spRTTIRecord final
    {
        using Factory = std::unique_ptr<spBaseObject> (*)();
        using PropertyRegistrar = bool (*)();

        spClassID classID;
        spClassID baseClassID;
        const char* className;
        const spRTTIRecord* base;
        Factory factory;
        PropertyRegistrar propertyRegistrar;

        [[nodiscard]] bool IsExactly(spClassID candidate) const noexcept;
        [[nodiscard]] bool IsKindOf(spClassID candidate) const noexcept;
    };

    class spRTTIManager final
    {
    public:
        // Native class identity is confirmed on PC and PS2.  This portable
        // behavior facade does not yet claim the native multiple-inheritance
        // layout.
        static constexpr spClassID NativeClassID = 0x5EA0637A;

        static spRTTIManager& Instance();

        bool Register(const spRTTIRecord& record);
        [[nodiscard]] const spRTTIRecord* Find(spClassID classID) const noexcept;
        [[nodiscard]] std::unique_ptr<spBaseObject> Create(spClassID classID) const;
        [[nodiscard]] std::size_t GetRegistrationCount() const noexcept;

        spRTTIManager(const spRTTIManager&) = delete;
        spRTTIManager& operator=(const spRTTIManager&) = delete;

    private:
        spRTTIManager() = default;

        std::map<spClassID, const spRTTIRecord*> records_;
    };

    class spCloneManager final
    {
    public:
        static constexpr spClassID NativeClassID = 0xC4419F78;

        // This first portable slice owns a single root result.  Native
        // repeated/cyclic reference resolution needs a graph-owning API and
        // is intentionally not claimed by Clone yet.
        [[nodiscard]] std::unique_ptr<spBaseObject> Clone(const spBaseObject& source);
        [[nodiscard]] spBaseObject* FindClone(const spBaseObject& source) const noexcept;

        spCloneManager(const spCloneManager&) = delete;
        spCloneManager& operator=(const spCloneManager&) = delete;

    private:
        friend class spBaseObject;
        friend class spNamedObject;
        friend class spMemoryStream;
        friend class spApp;
        friend class spPCFileStream;
        friend class spPCAsyncFileStreamManager;
        friend class spPCKManager;
        friend class spPS2Helper;
        friend class spPS2IOPModuleManager;
        friend class spEngineCore;
        friend class spError;
        friend class spPCErrorManager;
        friend class spPS2ErrorManager;
        friend class spSubscriptionManager;
        friend class spPCFontManager;
        friend class spPS2FontManager;
        friend class spDXInputManager;
        friend class spPS2InputManager;
        friend class spDebugManager;
        friend class spEntityManager;
        friend class spTemplateManager;
        friend class spTemplateInstance;
        friend class spTemplateObject;
        friend class spTemplateSerializer;
        friend class spGameLevel;
        friend class spGameLevelSerializer;
        friend class spIndexBuffer;
        friend class spVertexBuffer;
        friend class spTextureBuffer;
        friend class spTextureData;
        friend class spMeshData;
        friend class spPlatformSpecificMeshData;
        friend class spDXMeshData;
        friend class spPS2MeshData;
        friend class spDXVertexBuffer;
        friend class spDXIndexBuffer;
        friend class spDXSharedMeshData;
        friend class spDXCombinedVB;
        friend class spDXSharedMeshDataSerializer;
        friend class spDXMesh;
        friend class spPS2Mesh;
        friend class spDXMeshSerializer;
        friend class spResource;
        friend class spResourceManager;
        friend class spModel;
        friend class spNode;
        friend class spRenderNode;
        friend class spLightData;
        friend class spLightDataSerializer;
        friend class spLightSerializer;
        friend class spMaterialSerializer;
        friend class spMaterialData;
        friend class spPS2Material;
        friend class spMaterialDataSerializer;
        friend class spDXMaterialDataSerializer;
        friend class spPS2MaterialDataSerializer;
        friend class spCameraSerializer;
        friend class spCameraDataSerializer;
        friend class spCameraData;
        friend class spDXCamera;
        friend class spPS2Camera;
        friend class spFogSerializer;
        friend class spFog;
        friend class spMatColorControllerSerializer;
        friend class spLightControllerSerializer;
        friend class spAnimTexControllerSerializer;
        friend class spUVControllerSerializer;
        friend class spTransFunctionEvalSerializer;
        friend class spFunctionEvalSerializer;
        friend class spColorFuncEvalSerializer;
        friend class spSphereBVSerializer;
        friend class spBoxBVSerializer;
        friend class spOBBBVSerializer;
        friend class spRenderableSerializer;
        friend class spModelSerializer;
        friend class spSkin;
        friend class spSkinSerializer;
        friend class spMeshDataSerializer;
        friend class spDXMeshDataSerializer;
        friend class spPS2MeshDataSerializer;
        friend class spTextureDataSerializer;
        friend class spDXTextureDataSerializer;
        friend class spPS2TextureDataSerializer;
        friend class spSerializer;
        friend class spSerializerManager;
        friend class spPS2SerializerHook;
        friend class spDXSerializerHook;
        friend class spNodeSerializer;
        friend class spRenderNodeSerializer;
        friend class spPCRenderer;
        friend class spPS2Renderer;
        friend class spDXCubeRenderTarget;
        friend class spPCRenderTarget;
        friend class spPS2RenderTarget;
        friend class spPS2CubeRenderTarget;
        friend class spPCRenderTargetManager;
        friend class spPS2RenderTargetManager;
        friend class spMaterialTexture;
        friend class spMaterialRenderTargetTexture;
        friend class spMaterialCameraViewTexture;
        friend class spMaterialCubeMapTexture;
        friend class spMaterialPassLayer;
        friend class spMaterialTextureLayer;
        friend class spStdLayer;

        spCloneManager() = default;
        void RegisterClone(const spBaseObject& source, spBaseObject& clone);

        std::map<const spBaseObject*, spBaseObject*> clones_;
        std::size_t depth_ = 0;
    };

    class spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x415352A1;

        spBaseObject() noexcept = default;
        virtual ~spBaseObject();

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // Analytical aliases for the native PS2 vtable slots.  Their observed
        // behavior is implemented; their original source-level names remain
        // unknown.
        // Both native ABIs pass one pointer-sized notification/context
        // argument.  Its exact original source type/name is not recovered;
        // the byte-exact PS2 layout is recorded in Analysis/PS2.
        virtual void vfunc_0C(const void* notification) noexcept;
        [[nodiscard]] virtual std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const;
        virtual bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const;
        [[nodiscard]] virtual const spRTTIRecord& vfunc_18() const noexcept;
        [[nodiscard]] virtual bool vfunc_1C(spClassID candidate) const noexcept;
        [[nodiscard]] virtual bool vfunc_20(spClassID candidate) const noexcept;

        // Portable convenience wrappers; these are not claimed as recovered
        // original declarations.
        [[nodiscard]] std::unique_ptr<spBaseObject> Clone() const;
        [[nodiscard]] bool IsExactly(spClassID candidate) const noexcept;
        [[nodiscard]] bool IsKindOf(spClassID candidate) const noexcept;
    };

    class spNamedObject : public spBaseObject
    {
    public:
        static constexpr spClassID ClassID = 0x44DE07FD;

        spNamedObject() noexcept = default;
        explicit spNamedObject(const char* name);
        ~spNamedObject() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        [[nodiscard]] const char* GetName() const noexcept;
        void SetName(const char* name);
        void SetName(std::string_view name);

        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        bool vfunc_14(spBaseObject& destination, spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;

    private:
        // Portable equivalent of the native +0x10 shared string-entry pointer.
        // It deliberately does not claim host ABI compatibility.
        std::shared_ptr<const std::string> name_;
    };

    // No independent source path for this class has been recovered.  Both
    // shipped builds place it directly between spNamedObject and the
    // platform-facing interfaces in the native RTTI graph.  The class adds no
    // instance storage on PS2; the portable declaration preserves that
    // behavioral role without claiming host ABI compatibility.
    class spCrossPlatform : public spNamedObject
    {
    public:
        static constexpr spClassID ClassID = 0x20A72504;

        spCrossPlatform() noexcept = default;
        ~spCrossPlatform() override;

        [[nodiscard]] static const spRTTIRecord& StaticRTTI() noexcept;

        // Native spCrossPlatform instances are deliberately not cloned.  PC
        // reuses the root null-clone implementation; PS2 emits an equivalent
        // class-local stub.
        [[nodiscard]] std::unique_ptr<spBaseObject> vfunc_10(
            spCloneManager& manager) const override;
        [[nodiscard]] const spRTTIRecord& vfunc_18() const noexcept override;
    };
}

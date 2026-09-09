#include <fbxsdk.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
constexpr std::array<char, 8> ExportMagic{'S', 'M', 'O', 'F', 'B', 'X', 'E', '1'};
constexpr std::uint32_t ProtocolVersion = 4;
constexpr std::uint32_t ResourceSkeleton = 2;
constexpr std::uint32_t ResourceMaterials = 4;
constexpr std::uint32_t ResourceTextures = 8;
constexpr std::uint32_t ResourceAnimations = 16;
constexpr std::uint32_t SceneModeLevelWithBakedObjects = 2;
constexpr std::uint32_t MaximumCount = 100'000'000;

std::string ToUtf8(const std::wstring& value)
{
    if (value.empty()) return {};
    int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("Cannot encode a Windows path as UTF-8.");
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

struct SdkDestroy
{
    void operator()(FbxManager* value) const noexcept
    {
        if (value != nullptr) value->Destroy();
    }
};

struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Vec4 { float x{}, y{}, z{}, w{}; };
struct Mat4 { std::array<float, 16> value{}; };

class BinaryReader
{
public:
    explicit BinaryReader(const fs::path& path) : stream_(path, std::ios::binary)
    {
        if (!stream_) throw std::runtime_error("Cannot open FBX export payload.");
    }

    void Bytes(void* data, std::size_t size)
    {
        if (size == 0) return;
        stream_.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
        if (!stream_) throw std::runtime_error("Unexpected end of FBX export payload.");
    }

    template<typename T>
    T Pod()
    {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};
        Bytes(&value, sizeof(T));
        return value;
    }

    bool Boolean()
    {
        std::uint8_t value = Pod<std::uint8_t>();
        if (value > 1) throw std::runtime_error("Invalid boolean in FBX export payload.");
        return value != 0;
    }

    std::uint32_t Count(const char* owner)
    {
        std::uint32_t value = Pod<std::uint32_t>();
        if (value > MaximumCount)
            throw std::runtime_error(std::string("Excessive ") + owner + " count in FBX payload.");
        return value;
    }

    std::string String()
    {
        std::uint32_t count = Count("string byte");
        std::string result(count, '\0');
        Bytes(result.data(), result.size());
        return result;
    }

    std::vector<std::uint8_t> ByteArray()
    {
        std::uint32_t count = Count("byte");
        std::vector<std::uint8_t> result(count);
        Bytes(result.data(), result.size());
        return result;
    }

    Vec2 Vector2() { return {Pod<float>(), Pod<float>()}; }
    Vec3 Vector3() { return {Pod<float>(), Pod<float>(), Pod<float>()}; }
    Vec4 Vector4() { return {Pod<float>(), Pod<float>(), Pod<float>(), Pod<float>()}; }

    Mat4 Matrix()
    {
        Mat4 result;
        for (float& value : result.value) value = Pod<float>();
        return result;
    }

    template<typename T, typename Read>
    std::vector<T> Array(const char* owner, Read read)
    {
        std::uint32_t count = Count(owner);
        std::vector<T> result;
        result.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index) result.push_back(read());
        return result;
    }

private:
    std::ifstream stream_;
};

struct TextureData
{
    std::int32_t objectIndex{};
    std::string name;
    std::int32_t width{};
    std::int32_t height{};
    std::vector<std::uint8_t> png;
    std::vector<std::uint8_t> opacity;
    std::vector<std::uint8_t> opaqueRgb;
};

struct MeshData
{
    std::int32_t transportIndex{}; // payload-local ordinal, separate from file identity
    std::int32_t objectIndex{};
    std::uint32_t objectId{};
    std::string name;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uv0;
    std::vector<Vec2> uv1;
    std::vector<Vec4> colors;
    std::vector<Vec4> weights;
    std::vector<Vec4> joints;
    std::vector<std::uint32_t> indices;
    std::optional<TextureData> texture;
    std::optional<TextureData> effectTexture;
    Vec4 materialColor{};
    bool usesAlpha{};
    std::int32_t skinObjectIndex{-1};
    std::int32_t parentObjectIndex{-1};
    Mat4 bindWorld{};
    Mat4 bindLocal{};
};

struct PlacementData
{
    std::int32_t meshTransportIndex{};
    std::int32_t containerObjectIndex{-1};
    std::int32_t memberSlot{-1};
    std::int32_t sceneObjectIndex{};
    std::string name;
    std::int32_t meshObjectIndex{};
    bool sharedInstance{};
    std::int32_t staticObjectIndex{-1};
    std::int32_t materialObjectIndex{-1};
    std::int32_t parentObjectIndex{-1};
    Mat4 world{};
    Mat4 local{};
};

template<class T> bool SameArray(const std::vector<T>& a, const std::vector<T>& b)
{
    static_assert(std::is_trivially_copyable_v<T>);
    return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}

bool SameRigidGeometry(const MeshData& a, const MeshData& b)
{
    return a.objectId == b.objectId && SameArray(a.positions,b.positions) && SameArray(a.normals,b.normals)
        && SameArray(a.uv0,b.uv0) && SameArray(a.uv1,b.uv1) && SameArray(a.colors,b.colors) && SameArray(a.indices,b.indices);
}

struct NodeData
{
    std::int32_t objectIndex{};
    std::string name;
    std::int32_t parentObjectIndex{-1};
    Mat4 bindWorld{};
    Mat4 bindLocal{};
};

struct SkinData
{
    std::int32_t objectIndex{};
    std::string name;
    std::vector<std::int32_t> joints;
    std::vector<Mat4> inverseBind;
};

struct VectorKey { float time{}; Vec3 value{}; };
struct RotationKey { float time{}; Vec4 value{}; };
struct TrackData
{
    std::int32_t nodeObjectIndex{};
    std::string nodeName;
    std::vector<VectorKey> positions;
    std::vector<RotationKey> rotations;
    std::vector<VectorKey> scales;
};
struct AnimationData
{
    std::string name;
    float duration{};
    std::vector<TrackData> tracks;
};
struct ExportData
{
    std::uint32_t resources{};
    std::uint32_t sceneMode{};
    std::string sourcePath;
    std::vector<MeshData> meshes;
    std::vector<PlacementData> placements;
    std::vector<NodeData> nodes;
    std::vector<SkinData> skins;
    std::vector<AnimationData> animations;
};

std::optional<TextureData> ReadTexture(BinaryReader& reader)
{
    if (!reader.Boolean()) return std::nullopt;
    TextureData result;
    result.objectIndex = reader.Pod<std::int32_t>();
    result.name = reader.String();
    result.width = reader.Pod<std::int32_t>();
    result.height = reader.Pod<std::int32_t>();
    result.png = reader.ByteArray();
    result.opacity = reader.ByteArray();
    result.opaqueRgb = reader.ByteArray();
    return result;
}

MeshData ReadMesh(BinaryReader& reader, std::uint32_t version)
{
    MeshData result;
    if (version >= 4) result.transportIndex = reader.Pod<std::int32_t>();
    result.objectIndex = reader.Pod<std::int32_t>();
    if (version < 4) result.transportIndex = result.objectIndex;
    result.objectId = reader.Pod<std::uint32_t>();
    result.name = reader.String();
    result.positions = reader.Array<Vec3>("position", [&] { return reader.Vector3(); });
    result.normals = reader.Array<Vec3>("normal", [&] { return reader.Vector3(); });
    result.uv0 = reader.Array<Vec2>("UV0", [&] { return reader.Vector2(); });
    result.uv1 = reader.Array<Vec2>("UV1", [&] { return reader.Vector2(); });
    result.colors = reader.Array<Vec4>("color", [&] { return reader.Vector4(); });
    result.weights = reader.Array<Vec4>("weight", [&] { return reader.Vector4(); });
    result.joints = reader.Array<Vec4>("joint", [&] { return reader.Vector4(); });
    result.indices = reader.Array<std::uint32_t>(
        "index", [&] { return reader.Pod<std::uint32_t>(); });
    result.texture = ReadTexture(reader);
    result.effectTexture = ReadTexture(reader);
    result.materialColor = reader.Vector4();
    result.usesAlpha = reader.Boolean();
    result.skinObjectIndex = reader.Pod<std::int32_t>();
    result.parentObjectIndex = reader.Pod<std::int32_t>();
    result.bindWorld = reader.Matrix();
    result.bindLocal = reader.Matrix();
    return result;
}

PlacementData ReadPlacement(BinaryReader& reader, std::uint32_t version)
{
    PlacementData result;
    if (version >= 4)
    {
        result.meshTransportIndex = reader.Pod<std::int32_t>();
        result.containerObjectIndex = reader.Pod<std::int32_t>();
        result.memberSlot = reader.Pod<std::int32_t>();
    }
    result.sceneObjectIndex = reader.Pod<std::int32_t>();
    result.name = reader.String();
    result.meshObjectIndex = reader.Pod<std::int32_t>();
    if (version < 4) result.meshTransportIndex = result.meshObjectIndex;
    result.sharedInstance = reader.Boolean();
    result.staticObjectIndex = reader.Pod<std::int32_t>();
    result.materialObjectIndex = reader.Pod<std::int32_t>();
    result.parentObjectIndex = reader.Pod<std::int32_t>();
    result.world = reader.Matrix();
    result.local = reader.Matrix();
    return result;
}

NodeData ReadNode(BinaryReader& reader)
{
    return {reader.Pod<std::int32_t>(), reader.String(), reader.Pod<std::int32_t>(),
            reader.Matrix(), reader.Matrix()};
}

SkinData ReadSkin(BinaryReader& reader)
{
    SkinData result;
    result.objectIndex = reader.Pod<std::int32_t>();
    result.name = reader.String();
    result.joints = reader.Array<std::int32_t>(
        "skin joint", [&] { return reader.Pod<std::int32_t>(); });
    result.inverseBind = reader.Array<Mat4>("inverse bind", [&] { return reader.Matrix(); });
    if (result.joints.size() != result.inverseBind.size())
        throw std::runtime_error("FBX skin joint and inverse-bind counts differ.");
    return result;
}

TrackData ReadTrack(BinaryReader& reader)
{
    TrackData result;
    result.nodeObjectIndex = reader.Pod<std::int32_t>();
    result.nodeName = reader.String();
    result.positions = reader.Array<VectorKey>("position key", [&]
    {
        return VectorKey{reader.Pod<float>(), reader.Vector3()};
    });
    result.rotations = reader.Array<RotationKey>("rotation key", [&]
    {
        return RotationKey{reader.Pod<float>(), reader.Vector4()};
    });
    result.scales = reader.Array<VectorKey>("scale key", [&]
    {
        return VectorKey{reader.Pod<float>(), reader.Vector3()};
    });
    return result;
}

AnimationData ReadAnimation(BinaryReader& reader)
{
    AnimationData result;
    result.name = reader.String();
    result.duration = reader.Pod<float>();
    result.tracks = reader.Array<TrackData>("animation track", [&] { return ReadTrack(reader); });
    return result;
}

ExportData ReadPayload(const fs::path& path)
{
    BinaryReader reader(path);
    std::array<char, 8> magic{};
    reader.Bytes(magic.data(), magic.size());
    if (magic != ExportMagic) throw std::runtime_error("Invalid FBX export payload signature.");
    const auto version = reader.Pod<std::uint32_t>();
    if (version != 3 && version != ProtocolVersion)
        throw std::runtime_error("Unsupported FBX export payload version.");
    ExportData result;
    result.resources = reader.Pod<std::uint32_t>();
    result.sceneMode = reader.Pod<std::uint32_t>();
    result.sourcePath = reader.String();
    result.meshes = reader.Array<MeshData>("mesh", [&] { return ReadMesh(reader, version); });
    result.placements = reader.Array<PlacementData>(
        "mesh placement", [&] { return ReadPlacement(reader, version); });
    result.nodes = reader.Array<NodeData>("node", [&] { return ReadNode(reader); });
    result.skins = reader.Array<SkinData>("skin", [&] { return ReadSkin(reader); });
    result.animations = reader.Array<AnimationData>(
        "animation", [&] { return ReadAnimation(reader); });
    return result;
}

FbxAMatrix FromRowMatrix(const Mat4& source)
{
    FbxAMatrix result;
    for (int row = 0; row < 4; ++row)
        result.SetRow(row, FbxVector4(
            source.value[static_cast<std::size_t>(row * 4)],
            source.value[static_cast<std::size_t>(row * 4 + 1)],
            source.value[static_cast<std::size_t>(row * 4 + 2)],
            source.value[static_cast<std::size_t>(row * 4 + 3)]));
    return result;
}

void ApplyLocalMatrix(FbxNode* node, const Mat4& source)
{
    FbxAMatrix matrix = FromRowMatrix(source);
    node->SetRotationOrder(FbxNode::eSourcePivot, eEulerXYZ);
    node->LclTranslation.Set(matrix.GetT());
    node->LclRotation.Set(matrix.GetR());
    node->LclScaling.Set(matrix.GetS());
}

std::string SafeName(const std::string& value, const std::string& fallback)
{
    return value.empty() ? fallback : value;
}

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        fs::path base = fs::temp_directory_path();
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            path_ = base / (L"smo-fbx-export-native-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt));
            std::error_code error;
            if (fs::create_directory(path_, error)) return;
        }
        throw std::runtime_error("Cannot create temporary FBX media directory.");
    }
    ~TemporaryDirectory()
    {
        std::error_code error;
        fs::remove_all(path_, error);
    }
    const fs::path& Path() const { return path_; }
private:
    fs::path path_;
};

void WriteBytes(const fs::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot stage an FBX texture.");
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("Cannot write an FBX texture.");
}

FbxFileTexture* CreateTexture(
    FbxScene* scene, const std::vector<std::uint8_t>& bytes,
    const fs::path& directory, const std::string& prefix, const char* uvSet)
{
    if (bytes.empty()) return nullptr;
    fs::path file = directory / (std::wstring(prefix.begin(), prefix.end()) + L".png");
    WriteBytes(file, bytes);
    FbxFileTexture* texture = FbxFileTexture::Create(scene, prefix.c_str());
    std::string utf8 = ToUtf8(file.wstring());
    texture->SetFileName(utf8.c_str());
    texture->SetRelativeFileName((prefix + ".png").c_str());
    texture->SetTextureUse(FbxTexture::eStandard);
    texture->SetMappingType(FbxTexture::eUV);
    texture->SetMaterialUse(FbxFileTexture::eModelMaterial);
    texture->SetSwapUV(false);
    texture->UVSet.Set(uvSet);
    return texture;
}

void AddMeshAttributes(FbxMesh* mesh, const MeshData& source)
{
    if (!source.normals.empty())
    {
        if (source.normals.size() != source.positions.size())
            throw std::runtime_error("FBX mesh normal count differs from vertex count.");
        auto* element = mesh->CreateElementNormal();
        element->SetMappingMode(FbxLayerElement::eByControlPoint);
        element->SetReferenceMode(FbxLayerElement::eDirect);
        for (const Vec3& value : source.normals)
            element->GetDirectArray().Add(FbxVector4(value.x, value.y, value.z, 0));
    }
    auto addUv = [&](const std::vector<Vec2>& values, const char* name)
    {
        if (values.empty()) return;
        if (values.size() != source.positions.size())
            throw std::runtime_error("FBX mesh UV count differs from vertex count.");
        auto* element = mesh->CreateElementUV(name);
        element->SetMappingMode(FbxLayerElement::eByControlPoint);
        element->SetReferenceMode(FbxLayerElement::eDirect);
        for (const Vec2& value : values)
            element->GetDirectArray().Add(FbxVector2(value.x, 1.0 - value.y));
    };
    addUv(source.uv0, "UVSet0");
    addUv(source.uv1, "UVSet1");
    if (!source.colors.empty())
    {
        if (source.colors.size() != source.positions.size())
            throw std::runtime_error("FBX mesh color count differs from vertex count.");
        auto* element = mesh->CreateElementVertexColor();
        element->SetMappingMode(FbxLayerElement::eByControlPoint);
        element->SetReferenceMode(FbxLayerElement::eDirect);
        for (const Vec4& value : source.colors)
            element->GetDirectArray().Add(FbxColor(value.x, value.y, value.z, value.w));
    }
}

struct SceneState
{
    FbxScene* scene{};
    const ExportData* data{};
    fs::path mediaDirectory;
    std::unordered_map<std::int32_t, FbxNode*> nodes;
    std::unordered_map<std::int32_t, std::vector<FbxNode*>> animatedNodes;
    std::unordered_map<std::int32_t, const SkinData*> skins;
    std::unordered_set<std::int32_t> jointObjects;
};

void BuildLogicalNodes(SceneState& state)
{
    std::unordered_map<std::int32_t, const NodeData*> nodeData;
    for (const NodeData& node : state.data->nodes)
        if (!nodeData.emplace(node.objectIndex, &node).second)
            throw std::runtime_error("Duplicate FBX node object index.");
    for (const SkinData& skin : state.data->skins)
    {
        state.skins.emplace(skin.objectIndex, &skin);
        // Preserve the full transform chain, including unweighted helpers.
        // This prevents nested armatures and avoids deriving armature bind
        // space from a non-bone ancestor's parent-relative matrix.
        for (std::int32_t joint : skin.joints)
        {
            std::unordered_set<std::int32_t> visited;
            auto cursor = nodeData.find(joint);
            if (cursor == nodeData.end())
                throw std::runtime_error("FBX skin references a missing joint node.");
            while (cursor != nodeData.end())
            {
                if (!visited.insert(cursor->first).second)
                    throw std::runtime_error("FBX skeleton hierarchy contains a cycle.");
                // Earlier completed paths are already validated to the root.
                if (!state.jointObjects.insert(cursor->first).second) break;
                cursor = nodeData.find(cursor->second->parentObjectIndex);
            }
        }
    }
    for (const NodeData& source : state.data->nodes)
    {
        if (state.nodes.contains(source.objectIndex))
            throw std::runtime_error("Duplicate FBX node object index.");
        FbxNode* node = FbxNode::Create(
            state.scene, SafeName(source.name, "node_" + std::to_string(source.objectIndex)).c_str());
        if (state.jointObjects.contains(source.objectIndex))
        {
            FbxSkeleton* skeleton = FbxSkeleton::Create(state.scene, node->GetName());
            skeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
            skeleton->Size.Set(1.0);
            node->SetNodeAttribute(skeleton);
        }
        else
        {
            FbxNull* nullAttribute = FbxNull::Create(state.scene, node->GetName());
            node->SetNodeAttribute(nullAttribute);
        }
        ApplyLocalMatrix(node, source.bindLocal);
        state.nodes.emplace(source.objectIndex, node);
        state.animatedNodes[source.objectIndex].push_back(node);
    }
    for (const NodeData& source : state.data->nodes)
    {
        FbxNode* node = state.nodes.at(source.objectIndex);
        auto parent = state.nodes.find(source.parentObjectIndex);
        (parent == state.nodes.end() ? state.scene->GetRootNode() : parent->second)->AddChild(node);
        // Keep weighted roots as LimbNode too. Blender treats FBX Root as an
        // armature object and would skip that node's skin clusters/weights.
    }
}

FbxSurfaceMaterial* BuildMaterial(
    SceneState& state, const MeshData& source, std::size_t meshNumber)
{
    std::string name = SafeName(source.name, "material_" + std::to_string(meshNumber));
    FbxSurfacePhong* material = FbxSurfacePhong::Create(state.scene, name.c_str());
    material->Diffuse.Set(FbxDouble3(
        source.materialColor.x, source.materialColor.y, source.materialColor.z));
    material->DiffuseFactor.Set(1.0);
    material->Specular.Set(FbxDouble3(0.0, 0.0, 0.0));
    material->Shininess.Set(0.0);
    const bool includeTextures = (state.data->resources & ResourceTextures) != 0;
    const bool combinedAlpha = includeTextures && source.usesAlpha && source.texture &&
        !source.texture->opacity.empty() && source.materialColor.w < 1.0f;
    // Protocol v3 bakes texture * material alpha into 16-bit PNGs. Applying the
    // scalar again would square it in consumers which multiply both inputs.
    const double alpha = combinedAlpha ? 1.0 :
        std::clamp<double>(source.materialColor.w, 0.0, 1.0);
    material->TransparencyFactor.Set(1.0 - alpha);
    material->TransparentColor.Set(FbxDouble3(1.0 - alpha, 1.0 - alpha, 1.0 - alpha));
    // Some importers use Opacity when TransparencyFactor is exactly 0 or 1.
    FbxProperty opacityProperty = FbxProperty::Create(material, FbxDoubleDT, "Opacity");
    opacityProperty.Set(alpha);
    if ((state.data->resources & ResourceTextures) != 0 && source.texture)
    {
        const TextureData& data = *source.texture;
        const std::string variant = combinedAlpha ?
            "_alpha_" + std::to_string(std::bit_cast<std::uint32_t>(source.materialColor.w)) : "";
        const std::vector<std::uint8_t>& colorBytes =
            !source.usesAlpha && !data.opaqueRgb.empty() ? data.opaqueRgb : data.png;
        FbxFileTexture* texture = CreateTexture(
            state.scene, colorBytes, state.mediaDirectory,
            "texture_" + std::to_string(data.objectIndex) +
                (!source.usesAlpha && !data.opaqueRgb.empty() ? "_opaque" : "_base") + variant,
            "UVSet0");
        if (texture != nullptr) material->Diffuse.ConnectSrcObject(texture);
        if (source.usesAlpha)
        {
            FbxFileTexture* opacity = CreateTexture(
                state.scene, data.opacity, state.mediaDirectory,
                "texture_" + std::to_string(data.objectIndex) + "_opacity" + variant, "UVSet0");
            if (opacity != nullptr) material->TransparentColor.ConnectSrcObject(opacity);
        }
    }
    if ((state.data->resources & ResourceTextures) != 0 && source.effectTexture)
    {
        FbxFileTexture* effect = CreateTexture(
            state.scene, source.effectTexture->png, state.mediaDirectory,
            "texture_" + std::to_string(source.effectTexture->objectIndex) + "_effect",
            "UVSet1");
        if (effect != nullptr)
        {
            material->Emissive.Set(FbxDouble3(1.0, 1.0, 1.0));
            material->EmissiveFactor.Set(1.0);
            material->Emissive.ConnectSrcObject(effect);
        }
    }
    return material;
}

FbxMesh* BuildMeshAttribute(SceneState& state, const MeshData& source)
{
    std::string name = SafeName(source.name, "mesh_" + std::to_string(source.objectIndex));
    FbxMesh* mesh = FbxMesh::Create(state.scene, name.c_str());
    mesh->InitControlPoints(static_cast<int>(source.positions.size()));
    for (std::size_t index = 0; index < source.positions.size(); ++index)
    {
        const Vec3& value = source.positions[index];
        mesh->SetControlPointAt(FbxVector4(value.x, value.y, value.z), static_cast<int>(index));
    }
    AddMeshAttributes(mesh, source);
    if (source.indices.size() % 3 != 0)
        throw std::runtime_error("FBX mesh index count is not divisible by three.");
    for (std::size_t triangle = 0; triangle < source.indices.size(); triangle += 3)
    {
        mesh->BeginPolygon();
        for (int corner = 0; corner < 3; ++corner)
        {
            std::uint32_t index = source.indices[triangle + static_cast<std::size_t>(corner)];
            if (index >= source.positions.size())
                throw std::runtime_error("FBX mesh index is outside the vertex array.");
            mesh->AddPolygon(static_cast<int>(index));
        }
        mesh->EndPolygon();
    }

    if ((state.data->resources & ResourceMaterials) != 0)
    {
        FbxGeometryElementMaterial* element = mesh->CreateElementMaterial();
        element->SetMappingMode(FbxLayerElement::eAllSame);
        element->SetReferenceMode(FbxLayerElement::eIndexToDirect);
        element->GetIndexArray().Add(0);
    }
    return mesh;
}

FbxNode* BuildMeshPlacementNode(
    SceneState& state,
    const PlacementData& placement,
    FbxMesh* mesh,
    FbxSurfaceMaterial* material)
{
    std::string name = SafeName(
        placement.name, "placement_" + std::to_string(placement.sceneObjectIndex));
    FbxNode* node = FbxNode::Create(state.scene, name.c_str());
    const auto metadata = [node](const char* key, std::int32_t value)
    {
        auto property = FbxProperty::Create(node, FbxIntDT, key);
        property.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
        property.Set<FbxInt>(value);
    };
    metadata("SparkplugSceneObjectIndex", placement.sceneObjectIndex);
    metadata("SparkplugMeshObjectIndex", placement.meshObjectIndex);
    metadata("SparkplugMaterialObjectIndex", placement.materialObjectIndex);
    metadata("SparkplugContainerObjectIndex", placement.containerObjectIndex);
    metadata("SparkplugMemberSlot", placement.memberSlot);
    // Reusing one FbxMesh node attribute is native FBX instancing: placement
    // transforms remain per-node while control points/polygons are serialized once.
    node->SetNodeAttribute(mesh);
    ApplyLocalMatrix(node, placement.local);
    if (material != nullptr) node->AddMaterial(material);
    auto parent = state.nodes.find(placement.parentObjectIndex);
    (parent == state.nodes.end() ? state.scene->GetRootNode() : parent->second)->AddChild(node);
    return node;
}

int JointSlot(float value, std::size_t count)
{
    if (!std::isfinite(value)) return -1;
    float rounded = std::round(value);
    if (std::abs(value - rounded) > 0.0001f || rounded < 0 || rounded >= count) return -1;
    return static_cast<int>(rounded);
}

void BindSkin(
    SceneState& state,
    const MeshData& source,
    const Mat4& bindWorld,
    FbxNode* meshNode,
    std::size_t meshNumber)
{
    if ((state.data->resources & ResourceSkeleton) == 0 || source.skinObjectIndex < 0)
        return;
    auto skinFound = state.skins.find(source.skinObjectIndex);
    if (skinFound == state.skins.end())
        throw std::runtime_error("FBX mesh references a missing skin.");
    const SkinData& skinData = *skinFound->second;
    if (source.weights.size() != source.positions.size() ||
        source.joints.size() != source.positions.size())
        throw std::runtime_error("FBX skinned mesh attribute counts differ.");

    std::vector<FbxNode*> links(skinData.joints.size());
    std::unordered_map<std::int32_t, int> occurrence;
    for (std::size_t slot = 0; slot < skinData.joints.size(); ++slot)
    {
        std::int32_t objectIndex = skinData.joints[slot];
        auto original = state.nodes.find(objectIndex);
        if (original == state.nodes.end())
            throw std::runtime_error("FBX skin references a missing joint node.");
        int currentOccurrence = occurrence[objectIndex]++;
        if (currentOccurrence == 0)
        {
            links[slot] = original->second;
        }
        else
        {
            FbxNode* clone = FbxNode::Create(
                state.scene,
                (std::string(original->second->GetName()) + "_palette_" +
                 std::to_string(slot)).c_str());
            FbxSkeleton* attribute = FbxSkeleton::Create(state.scene, clone->GetName());
            attribute->SetSkeletonType(FbxSkeleton::eLimbNode);
            clone->SetNodeAttribute(attribute);
            clone->LclTranslation.Set(original->second->LclTranslation.Get());
            clone->LclRotation.Set(original->second->LclRotation.Get());
            clone->LclScaling.Set(original->second->LclScaling.Get());
            FbxNode* parent = original->second->GetParent();
            (parent == nullptr ? state.scene->GetRootNode() : parent)->AddChild(clone);
            state.animatedNodes[objectIndex].push_back(clone);
            links[slot] = clone;
        }
    }

    FbxSkin* skin = FbxSkin::Create(
        state.scene, SafeName(skinData.name, "skin_" + std::to_string(meshNumber)).c_str());
    FbxAMatrix meshBind = FromRowMatrix(bindWorld);
    auto component = [](const Vec4& value, int index)
    {
        return index == 0 ? value.x : index == 1 ? value.y : index == 2 ? value.z : value.w;
    };
    std::vector<FbxAMatrix> linkBinds(links.size());
    for (std::size_t slot = 0; slot < links.size(); ++slot)
    {
        FbxCluster* cluster = FbxCluster::Create(
            state.scene, (std::string(links[slot]->GetName()) + "_cluster").c_str());
        cluster->SetLink(links[slot]);
        cluster->SetLinkMode(FbxCluster::eNormalize);
        for (std::size_t vertex = 0; vertex < source.positions.size(); ++vertex)
        for (int influence = 0; influence < 4; ++influence)
        {
            float weight = component(source.weights[vertex], influence);
            int joint = JointSlot(component(source.joints[vertex], influence), links.size());
            if (weight > 0 && joint == static_cast<int>(slot))
                cluster->AddControlPointIndex(static_cast<int>(vertex), weight);
        }
        FbxAMatrix inverseBind = FromRowMatrix(skinData.inverseBind[slot]);
        FbxAMatrix linkBind = inverseBind.Inverse() * meshBind;
        linkBinds[slot] = linkBind;
        cluster->SetTransformMatrix(meshBind);
        cluster->SetTransformLinkMatrix(linkBind);
        skin->AddCluster(cluster);
    }
    static_cast<FbxMesh*>(meshNode->GetNodeAttribute())->AddDeformer(skin);

    FbxPose* pose = FbxPose::Create(
        state.scene, ("bind_pose_" + std::to_string(source.objectIndex)).c_str());
    pose->SetIsBindPose(true);
    pose->Add(meshNode, meshBind);
    std::unordered_set<FbxNode*> added;
    for (std::size_t slot = 0; slot < links.size(); ++slot)
    {
        if (added.insert(links[slot]).second) pose->Add(links[slot], linkBinds[slot]);
    }
    // Bind poses must also contain the ancestors of the mesh and its joints.
    // Keep parent transforms explicit instead of relying on importer defaults.
    auto addAncestors = [&](FbxNode* node)
    {
        for (FbxNode* parent = node->GetParent();
             parent != nullptr && parent != state.scene->GetRootNode(); parent = parent->GetParent())
            if (added.insert(parent).second) pose->Add(parent, parent->EvaluateGlobalTransform());
    };
    addAncestors(meshNode);
    for (FbxNode* link : links) addAncestors(link);
    state.scene->AddPose(pose);
}

void SetCurveKeys(FbxAnimCurve* curve, const std::vector<std::pair<float, double>>& keys)
{
    if (curve == nullptr || keys.empty()) return;
    curve->KeyModifyBegin();
    for (const auto& [seconds, value] : keys)
    {
        FbxTime time;
        time.SetSecondDouble(seconds);
        int index = curve->KeyAdd(time);
        curve->KeySetValue(index, static_cast<float>(value));
        curve->KeySetInterpolation(index, FbxAnimCurveDef::eInterpolationLinear);
    }
    curve->KeyModifyEnd();
}

Vec4 NormalizeQuaternion(Vec4 value)
{
    float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
    if (!std::isfinite(length) || length < 1e-8f) return {0, 0, 0, 1};
    return {value.x / length, value.y / length, value.z / length, value.w / length};
}

Vec4 Slerp(Vec4 left, Vec4 right, float amount)
{
    left = NormalizeQuaternion(left);
    right = NormalizeQuaternion(right);
    float dot = left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w;
    if (dot < 0)
    {
        dot = -dot;
        right = {-right.x, -right.y, -right.z, -right.w};
    }
    if (dot > 0.9995f)
        return NormalizeQuaternion({
            left.x + amount * (right.x - left.x), left.y + amount * (right.y - left.y),
            left.z + amount * (right.z - left.z), left.w + amount * (right.w - left.w)});
    float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    float sine = std::sin(theta);
    float a = std::sin((1.0f - amount) * theta) / sine;
    float b = std::sin(amount * theta) / sine;
    return {left.x * a + right.x * b, left.y * a + right.y * b,
            left.z * a + right.z * b, left.w * a + right.w * b};
}

Vec4 EvaluateRotation(const std::vector<RotationKey>& keys, float time)
{
    if (keys.empty()) return {0, 0, 0, 1};
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;
    auto right = std::upper_bound(keys.begin(), keys.end(), time,
        [](float value, const RotationKey& key) { return value < key.time; });
    auto left = right - 1;
    float amount = (time - left->time) / (right->time - left->time);
    return Slerp(left->value, right->value, amount);
}

void AddAnimations(SceneState& state)
{
    if ((state.data->resources & ResourceAnimations) == 0) return;
    std::size_t remainingRotationKeys = 500000;
    state.scene->GetGlobalSettings().SetTimeMode(FbxTime::eFrames30);
    for (const AnimationData& animation : state.data->animations)
    {
        FbxAnimStack* stack = FbxAnimStack::Create(
            state.scene, SafeName(animation.name, "animation").c_str());
        FbxAnimLayer* layer = FbxAnimLayer::Create(state.scene, "Base Layer");
        stack->AddMember(layer);
        FbxTime start;
        start.SetSecondDouble(0);
        FbxTime stop;
        stop.SetSecondDouble(std::max(0.0f, animation.duration));
        FbxTimeSpan timeSpan(start, stop);
        stack->SetLocalTimeSpan(timeSpan);
        for (const TrackData& track : animation.tracks)
        {
            auto targets = state.animatedNodes.find(track.nodeObjectIndex);
            if (targets == state.animatedNodes.end()) continue;
            for (FbxNode* node : targets->second)
            {
                auto vectorCurves = [&](const std::vector<VectorKey>& keys, FbxPropertyT<FbxDouble3>& property)
                {
                    if (keys.empty()) return;
                    std::vector<std::pair<float, double>> x, y, z;
                    for (const VectorKey& key : keys)
                    {
                        x.emplace_back(key.time, key.value.x);
                        y.emplace_back(key.time, key.value.y);
                        z.emplace_back(key.time, key.value.z);
                    }
                    SetCurveKeys(property.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X, true), x);
                    SetCurveKeys(property.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y, true), y);
                    SetCurveKeys(property.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z, true), z);
                };
                vectorCurves(track.positions, node->LclTranslation);
                vectorCurves(track.scales, node->LclScaling);
                if (!track.rotations.empty())
                {
                    float duration = std::max(
                        animation.duration, track.rotations.back().time);
                    double frameCount = std::ceil(static_cast<double>(duration) * 30.0);
                    if (!std::isfinite(frameCount) || frameCount < 0 || frameCount+1 > remainingRotationKeys)
                        throw std::runtime_error("FBX rotation sampling exceeds the 500000-key limit.");
                    int frames = std::max(1, static_cast<int>(frameCount));
                    std::vector<float> times;
                    times.reserve(static_cast<std::size_t>(frames)+1+track.rotations.size());
                    for (int frame = 0; frame <= frames; ++frame)
                        times.push_back(frame == frames ? duration : frame / 30.0f);
                    // The C# SAN baker also emits source boundaries and short
                    // pre-boundary guards. Preserve them during quaternion ->
                    // Euler conversion instead of reducing them back to 30fps.
                    for (const RotationKey& key : track.rotations)
                    {
                        if (!std::isfinite(key.time) || key.time < 0)
                            throw std::runtime_error("Invalid FBX rotation key time.");
                        times.push_back(key.time);
                    }
                    std::sort(times.begin(), times.end());
                    times.erase(std::unique(times.begin(), times.end()), times.end());
                    if (times.size() > remainingRotationKeys)
                        throw std::runtime_error("FBX rotation sampling exceeds the 500000-key limit.");
                    remainingRotationKeys -= times.size();
                    std::vector<std::pair<float, double>> x, y, z;
                    FbxVector4 previous;
                    bool hasPrevious = false;
                    for (float time : times)
                    {
                        Vec4 value = EvaluateRotation(track.rotations, time);
                        FbxAMatrix rotation;
                        rotation.SetQ(FbxQuaternion(value.x, value.y, value.z, value.w));
                        FbxVector4 euler = rotation.GetR();
                        if (hasPrevious)
                        {
                            for (int axis = 0; axis < 3; ++axis)
                            {
                                while (euler[axis] - previous[axis] > 180.0) euler[axis] -= 360.0;
                                while (euler[axis] - previous[axis] < -180.0) euler[axis] += 360.0;
                            }
                        }
                        previous = euler;
                        hasPrevious = true;
                        x.emplace_back(time, euler[0]);
                        y.emplace_back(time, euler[1]);
                        z.emplace_back(time, euler[2]);
                    }
                    SetCurveKeys(node->LclRotation.GetCurve(
                        layer, FBXSDK_CURVENODE_COMPONENT_X, true), x);
                    SetCurveKeys(node->LclRotation.GetCurve(
                        layer, FBXSDK_CURVENODE_COMPONENT_Y, true), y);
                    SetCurveKeys(node->LclRotation.GetCurve(
                        layer, FBXSDK_CURVENODE_COMPONENT_Z, true), z);
                }
            }
        }
    }
}

void ExportScene(const ExportData& data, const fs::path& outputPath, const fs::path& media)
{
    std::unique_ptr<FbxManager, SdkDestroy> manager(FbxManager::Create());
    if (!manager) throw std::runtime_error("FBX SDK manager creation failed.");
    FbxIOSettings* io = FbxIOSettings::Create(manager.get(), IOSROOT);
    manager->SetIOSettings(io);
    FbxScene* scene = FbxScene::Create(manager.get(), "SparkplugScene");
    scene->GetGlobalSettings().SetAxisSystem(FbxAxisSystem::MayaYUp);
    scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::m);
    SceneState state{scene, &data, media};
    BuildLogicalNodes(state);

    struct MeshAsset
    {
        const MeshData* source{};
        FbxMesh* attribute{};
        FbxSurfaceMaterial* material{};
        std::size_t number{};
    };
    struct PlacedMesh
    {
        const MeshAsset* asset{};
        const PlacementData* placement{};
        FbxNode* node{};
    };
    std::unordered_map<std::int32_t, MeshAsset> meshAssets;
    std::unordered_map<std::int32_t, std::pair<const MeshData*, FbxMesh*>> rigidGeometry;
    bool bakeMeshInstances =
        data.sceneMode == SceneModeLevelWithBakedObjects;
    for (std::size_t index = 0; index < data.meshes.size(); ++index)
    {
        const MeshData& source = data.meshes[index];
        FbxMesh* attribute = nullptr;
        if (!bakeMeshInstances && source.skinObjectIndex < 0)
        {
            auto found = rigidGeometry.find(source.objectIndex);
            if (found == rigidGeometry.end())
                found = rigidGeometry.emplace(source.objectIndex, std::make_pair(&source, BuildMeshAttribute(state, source))).first;
            else if (!SameRigidGeometry(*found->second.first, source))
                throw std::runtime_error("FBX variants disagree about the same physical mesh geometry.");
            attribute = found->second.second;
        }
        FbxSurfaceMaterial* material =
            (data.resources & ResourceMaterials) != 0
                ? BuildMaterial(state, source, index)
                : nullptr;
        if (!meshAssets.emplace(
                source.transportIndex,
                MeshAsset{&source, attribute, material, index}).second)
            throw std::runtime_error("Duplicate FBX mesh transport index.");
    }
    std::vector<PlacedMesh> placedMeshes;
    placedMeshes.reserve(data.placements.size());
    for (const PlacementData& placement : data.placements)
    {
        auto found = meshAssets.find(placement.meshTransportIndex);
        if (found == meshAssets.end())
            throw std::runtime_error("FBX placement references a missing mesh.");
        MeshAsset& asset = found->second;
        // Skin deformers own per-placement bind context; do not attach a second
        // Skin to a shared FbxMesh attribute when a Skin resource repeats.
        FbxMesh* placementAttribute = bakeMeshInstances || asset.source->skinObjectIndex >= 0
            ? BuildMeshAttribute(state, *asset.source)
            : asset.attribute;
        FbxNode* node = BuildMeshPlacementNode(
            state, placement, placementAttribute, asset.material);
        placedMeshes.push_back({&asset, &placement, node});
    }
    for (const PlacedMesh& placed : placedMeshes)
    {
        BindSkin(
            state,
            *placed.asset->source,
            placed.placement->world,
            placed.node,
            placed.asset->number);
    }
    AddAnimations(state);

    io->SetBoolProp(EXP_FBX_MATERIAL, (data.resources & ResourceMaterials) != 0);
    io->SetBoolProp(EXP_FBX_TEXTURE, (data.resources & ResourceTextures) != 0);
    io->SetBoolProp(EXP_FBX_EMBEDDED, (data.resources & ResourceTextures) != 0);
    io->SetBoolProp(EXP_FBX_SHAPE, false);
    io->SetBoolProp(EXP_FBX_GOBO, false);
    io->SetBoolProp(EXP_FBX_ANIMATION, (data.resources & ResourceAnimations) != 0);
    io->SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);

    int format = manager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX binary (*.fbx)");
    if (format < 0) format = manager->GetIOPluginRegistry()->GetNativeWriterFormat();
    FbxExporter* exporter = FbxExporter::Create(manager.get(), "");
    std::string outputUtf8 = ToUtf8(outputPath.wstring());
    if (!exporter->Initialize(outputUtf8.c_str(), format, io))
    {
        std::string error = exporter->GetStatus().GetErrorString();
        exporter->Destroy();
        throw std::runtime_error("FBX export initialization failed: " + error);
    }
    if (!exporter->Export(scene))
    {
        std::string error = exporter->GetStatus().GetErrorString();
        exporter->Destroy();
        throw std::runtime_error("FBX export failed: " + error);
    }
    exporter->Destroy();
}
}

int ExportCommandNative(int argc, wchar_t** argv)
{
    if (argc != 4)
        throw std::runtime_error("Usage: SmoFbxBridge export <input.bin> <output.fbx>");
    ExportData data = ReadPayload(argv[2]);
    TemporaryDirectory media;
    ExportScene(data, argv[3], media.Path());
    return 0;
}

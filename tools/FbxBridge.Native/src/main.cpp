#include <fbxsdk.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

int ExportCommandNative(int argc, wchar_t** argv);
int InspectExportCommandNative(int argc, wchar_t** argv);

namespace
{
constexpr std::array<char, 8> ImportMagic{'S', 'M', 'O', 'F', 'B', 'X', 'I', '1'};
constexpr std::uint32_t ProtocolVersion = 1;

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

std::wstring FromUtf8(const std::string& value)
{
    if (value.empty()) return {};
    int size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0);
    if (size <= 0) throw std::runtime_error("FBX SDK returned invalid UTF-8 text.");
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size);
    return result;
}

std::string SafeName(const char* value, const std::string& fallback)
{
    if (value == nullptr || *value == '\0') return fallback;
    return value;
}

struct SdkDestroy
{
    void operator()(FbxManager* value) const noexcept
    {
        if (value != nullptr) value->Destroy();
    }
};

class BinaryWriter
{
public:
    explicit BinaryWriter(const fs::path& path) : stream_(path, std::ios::binary)
    {
        if (!stream_) throw std::runtime_error("Cannot create bridge output payload.");
    }

    void Bytes(const void* data, std::size_t size)
    {
        if (size == 0) return;
        stream_.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!stream_) throw std::runtime_error("Cannot write bridge output payload.");
    }

    template<typename T>
    void Pod(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        Bytes(&value, sizeof(T));
    }

    void String(const std::string& value)
    {
        if (value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::overflow_error("Bridge string is too large.");
        Pod(static_cast<std::uint32_t>(value.size()));
        Bytes(value.data(), value.size());
    }

private:
    std::ofstream stream_;
};

struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Vec4 { float x{}, y{}, z{}, w{}; };
struct Joint4 { std::uint16_t x{}, y{}, z{}, w{}; };
struct Mat4 { std::array<float, 16> value{}; };

struct TextureData
{
    std::string name;
    std::string mime;
    std::vector<std::uint8_t> bytes;
    std::string sourcePath;
};

struct MaterialData
{
    std::string name;
    std::string textureName;
    std::int32_t textureIndex{-1};
};

struct SkeletonData
{
    std::string name;
    std::vector<std::string> names;
    std::vector<std::int32_t> parents;
    std::vector<Mat4> inverseBind;
    std::vector<Mat4> bindWorld;
    std::vector<Mat4> bindLocal;
    std::vector<std::array<std::pair<std::uint16_t, float>, 4>> controlPointWeights;
};

struct MeshData
{
    std::string name;
    std::int32_t materialIndex{-1};
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::vector<std::uint32_t> colors;
    std::vector<std::uint32_t> indices;
    std::optional<SkeletonData> skeleton;
    std::vector<Joint4> joints;
    std::vector<Vec4> weights;
};

struct ImportedData
{
    std::vector<TextureData> textures;
    std::vector<MaterialData> materials;
    std::vector<MeshData> meshes;
};

Mat4 ToRowMatrix(const FbxAMatrix& matrix)
{
    Mat4 result;
    for (int row = 0; row < 4; ++row)
    for (int column = 0; column < 4; ++column)
        result.value[static_cast<std::size_t>(row * 4 + column)] =
            static_cast<float>(matrix.Get(row, column));
    return result;
}

FbxAMatrix GeometryTransform(FbxNode* node)
{
    FbxAMatrix result;
    result.SetT(node->GetGeometricTranslation(FbxNode::eSourcePivot));
    result.SetR(node->GetGeometricRotation(FbxNode::eSourcePivot));
    result.SetS(node->GetGeometricScaling(FbxNode::eSourcePivot));
    return result;
}

Vec3 TransformPoint(const FbxAMatrix& matrix, const FbxVector4& value)
{
    FbxVector4 transformed = matrix.MultT(FbxVector4(value[0], value[1], value[2], 1.0));
    return {static_cast<float>(transformed[0]), static_cast<float>(transformed[1]),
            static_cast<float>(transformed[2])};
}

Vec3 TransformNormal(const FbxAMatrix& matrix, const FbxVector4& value)
{
    FbxAMatrix normalMatrix = matrix.Inverse().Transpose();
    FbxVector4 transformed = normalMatrix.MultT(
        FbxVector4(value[0], value[1], value[2], 0.0));
    double length = std::sqrt(
        transformed[0] * transformed[0] + transformed[1] * transformed[1] +
        transformed[2] * transformed[2]);
    if (length > 1e-20)
        transformed /= length;
    return {static_cast<float>(transformed[0]), static_cast<float>(transformed[1]),
            static_cast<float>(transformed[2])};
}

std::uint8_t ColorByte(double value)
{
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
}

std::uint32_t ToArgb(const FbxColor& color)
{
    return (static_cast<std::uint32_t>(ColorByte(color.mAlpha)) << 24) |
           (static_cast<std::uint32_t>(ColorByte(color.mRed)) << 16) |
           (static_cast<std::uint32_t>(ColorByte(color.mGreen)) << 8) |
           static_cast<std::uint32_t>(ColorByte(color.mBlue));
}

template<typename TElement>
int LayerDirectIndex(
    const TElement* element, int controlPoint, int polygonVertex, int polygon)
{
    if (element == nullptr) return -1;
    int mapped = -1;
    switch (element->GetMappingMode())
    {
    case FbxLayerElement::eByControlPoint: mapped = controlPoint; break;
    case FbxLayerElement::eByPolygonVertex: mapped = polygonVertex; break;
    case FbxLayerElement::eByPolygon: mapped = polygon; break;
    case FbxLayerElement::eAllSame: mapped = 0; break;
    default: return -1;
    }
    if (mapped < 0) return -1;
    if (element->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
    {
        if (mapped >= element->GetIndexArray().GetCount()) return -1;
        mapped = element->GetIndexArray().GetAt(mapped);
    }
    return mapped >= 0 && mapped < element->GetDirectArray().GetCount() ? mapped : -1;
}

bool HasSkin(FbxMesh* mesh)
{
    for (int index = 0; index < mesh->GetDeformerCount(FbxDeformer::eSkin); ++index)
    {
        auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(index, FbxDeformer::eSkin));
        if (skin != nullptr && skin->GetClusterCount() > 0) return true;
    }
    return false;
}

std::optional<SkeletonData> ReadSkeleton(FbxNode* meshNode, FbxMesh* mesh)
{
    std::vector<FbxCluster*> clusters;
    std::vector<FbxNode*> links;
    std::unordered_map<FbxNode*, std::uint16_t> slotByLink;
    for (int skinIndex = 0; skinIndex < mesh->GetDeformerCount(FbxDeformer::eSkin); ++skinIndex)
    {
        auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(skinIndex, FbxDeformer::eSkin));
        if (skin == nullptr) continue;
        for (int clusterIndex = 0; clusterIndex < skin->GetClusterCount(); ++clusterIndex)
        {
            FbxCluster* cluster = skin->GetCluster(clusterIndex);
            FbxNode* link = cluster == nullptr ? nullptr : cluster->GetLink();
            if (link == nullptr || slotByLink.contains(link)) continue;
            if (links.size() >= std::numeric_limits<std::uint16_t>::max())
                throw std::runtime_error("FBX skin contains more than 65535 joints.");
            slotByLink.emplace(link, static_cast<std::uint16_t>(links.size()));
            links.push_back(link);
            clusters.push_back(cluster);
        }
    }
    if (links.empty()) return std::nullopt;

    SkeletonData result;
    result.name = SafeName(meshNode->GetName(), "skin") + "_skin";
    result.names.reserve(links.size());
    result.parents.reserve(links.size());
    result.inverseBind.reserve(links.size());
    result.bindWorld.reserve(links.size());
    result.bindLocal.reserve(links.size());

    std::vector<FbxAMatrix> bindWorld;
    bindWorld.reserve(links.size());
    FbxAMatrix geometry = GeometryTransform(meshNode);
    for (std::size_t index = 0; index < links.size(); ++index)
    {
        FbxCluster* cluster = clusters[index];
        FbxAMatrix meshBind;
        FbxAMatrix linkBind;
        cluster->GetTransformMatrix(meshBind);
        cluster->GetTransformLinkMatrix(linkBind);
        meshBind *= geometry;
        FbxAMatrix inverseBind = meshBind * linkBind.Inverse();
        FbxAMatrix world = inverseBind.Inverse();
        bindWorld.push_back(world);
        result.names.push_back(SafeName(
            links[index]->GetName(), "joint_" + std::to_string(index)));
        result.inverseBind.push_back(ToRowMatrix(inverseBind));
        result.bindWorld.push_back(ToRowMatrix(world));

        FbxNode* ancestor = links[index]->GetParent();
        std::int32_t parent = -1;
        while (ancestor != nullptr)
        {
            auto found = slotByLink.find(ancestor);
            if (found != slotByLink.end())
            {
                parent = static_cast<std::int32_t>(found->second);
                break;
            }
            ancestor = ancestor->GetParent();
        }
        result.parents.push_back(parent);
    }
    for (std::size_t index = 0; index < bindWorld.size(); ++index)
    {
        std::int32_t parent = result.parents[index];
        FbxAMatrix local = parent >= 0
            ? bindWorld[index] * bindWorld[static_cast<std::size_t>(parent)].Inverse()
            : bindWorld[index];
        result.bindLocal.push_back(ToRowMatrix(local));
    }

    const int controlPointCount = mesh->GetControlPointsCount();
    std::vector<std::vector<std::pair<std::uint16_t, float>>> influences(
        static_cast<std::size_t>(std::max(0, controlPointCount)));
    for (std::size_t slot = 0; slot < clusters.size(); ++slot)
    {
        FbxCluster* cluster = clusters[slot];
        const int* indices = cluster->GetControlPointIndices();
        const double* weights = cluster->GetControlPointWeights();
        const int count = cluster->GetControlPointIndicesCount();
        for (int item = 0; item < count; ++item)
        {
            int controlPoint = indices[item];
            double weight = weights[item];
            if (controlPoint < 0 || controlPoint >= controlPointCount ||
                !std::isfinite(weight) || weight <= 0.0)
                continue;
            influences[static_cast<std::size_t>(controlPoint)].emplace_back(
                static_cast<std::uint16_t>(slot), static_cast<float>(weight));
        }
    }

    result.controlPointWeights.resize(influences.size());
    for (std::size_t controlPoint = 0; controlPoint < influences.size(); ++controlPoint)
    {
        auto& values = influences[controlPoint];
        std::sort(values.begin(), values.end(), [](const auto& left, const auto& right)
        {
            return left.second > right.second;
        });
        if (values.size() > 4) values.resize(4);
        float sum = 0.0f;
        for (const auto& value : values) sum += value.second;
        auto& destination = result.controlPointWeights[controlPoint];
        for (std::size_t index = 0; index < values.size(); ++index)
            destination[index] = {values[index].first, values[index].second / sum};
    }
    return result;
}

std::string MimeType(const fs::path& path, const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() >= 8 &&
        std::equal(bytes.begin(), bytes.begin() + 8,
            std::array<std::uint8_t, 8>{137, 80, 78, 71, 13, 10, 26, 10}.begin()))
        return "image/png";
    if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff)
        return "image/jpeg";
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    if (extension == L".tga") return "image/x-tga";
    if (extension == L".bmp") return "image/bmp";
    return "application/octet-stream";
}

std::vector<std::uint8_t> ReadAllBytes(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    std::streamsize size = input.tellg();
    if (size < 0 || size > std::numeric_limits<std::int32_t>::max()) return {};
    input.seekg(0);
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    if (size > 0 && !input.read(
        reinterpret_cast<char*>(result.data()), size)) return {};
    return result;
}

std::optional<fs::path> ResolveTexturePath(
    FbxFileTexture* texture, const fs::path& inputDirectory, const fs::path& extracted)
{
    std::vector<fs::path> candidates;
    auto append = [&](const char* raw)
    {
        if (raw == nullptr || *raw == '\0') return;
        try
        {
            fs::path path(FromUtf8(raw));
            candidates.push_back(path);
            if (path.is_relative()) candidates.push_back(inputDirectory / path);
            candidates.push_back(inputDirectory / path.filename());
            candidates.push_back(extracted / path.filename());
        }
        catch (...) { }
    };
    append(texture->GetFileName());
    append(texture->GetRelativeFileName());
    for (const fs::path& candidate : candidates)
    {
        std::error_code error;
        if (fs::is_regular_file(candidate, error)) return fs::absolute(candidate, error);
    }
    std::error_code error;
    if (fs::exists(extracted, error))
    {
        for (fs::recursive_directory_iterator iterator(extracted, error), end;
             !error && iterator != end; iterator.increment(error))
        {
            if (!iterator->is_regular_file(error)) continue;
            for (const fs::path& candidate : candidates)
            {
                if (iterator->path().filename() == candidate.filename())
                    return iterator->path();
            }
        }
    }
    return std::nullopt;
}

FbxFileTexture* FindFileTexture(FbxSurfaceMaterial* material)
{
    const std::array<const char*, 5> properties{
        FbxSurfaceMaterial::sDiffuse, "BaseColor", "base_color", "DiffuseColor", "Diffuse"};
    for (const char* name : properties)
    {
        FbxProperty property = material->FindProperty(name);
        if (!property.IsValid()) continue;
        int count = property.GetSrcObjectCount<FbxFileTexture>();
        if (count > 0) return property.GetSrcObject<FbxFileTexture>(0);
        int layeredCount = property.GetSrcObjectCount<FbxLayeredTexture>();
        for (int index = 0; index < layeredCount; ++index)
        {
            FbxLayeredTexture* layered = property.GetSrcObject<FbxLayeredTexture>(index);
            if (layered != nullptr && layered->GetSrcObjectCount<FbxFileTexture>() > 0)
                return layered->GetSrcObject<FbxFileTexture>(0);
        }
    }
    return nullptr;
}

void ReadMaterialsAndTextures(
    FbxScene* scene, const fs::path& inputPath, const fs::path& extracted,
    ImportedData& output, std::unordered_map<FbxSurfaceMaterial*, int>& materialIndices)
{
    std::map<fs::path, int> textureByPath;
    const int materialCount = scene->GetMaterialCount();
    output.materials.reserve(static_cast<std::size_t>(std::max(0, materialCount)));
    for (int index = 0; index < materialCount; ++index)
    {
        FbxSurfaceMaterial* source = scene->GetMaterial(index);
        MaterialData material;
        material.name = SafeName(source->GetName(), "material_" + std::to_string(index));
        FbxFileTexture* texture = FindFileTexture(source);
        if (texture != nullptr)
        {
            const char* relative = texture->GetRelativeFileName();
            const char* absolute = texture->GetFileName();
            const char* logical = relative != nullptr && *relative != '\0' ? relative : absolute;
            if (logical != nullptr) material.textureName = ToUtf8(fs::path(FromUtf8(logical)).filename().wstring());
            std::optional<fs::path> path = ResolveTexturePath(
                texture, inputPath.parent_path(), extracted);
            if (path)
            {
                std::error_code error;
                fs::path key = fs::weakly_canonical(*path, error);
                if (error) key = *path;
                auto found = textureByPath.find(key);
                if (found != textureByPath.end())
                {
                    material.textureIndex = found->second;
                }
                else
                {
                    TextureData data;
                    data.name = material.textureName.empty()
                        ? ToUtf8(path->filename().wstring()) : material.textureName;
                    data.bytes = ReadAllBytes(*path);
                    if (!data.bytes.empty())
                    {
                        data.mime = MimeType(*path, data.bytes);
                        data.sourcePath = logical == nullptr ? data.name : logical;
                        material.textureIndex = static_cast<int>(output.textures.size());
                        textureByPath.emplace(key, material.textureIndex);
                        output.textures.push_back(std::move(data));
                    }
                }
            }
        }
        materialIndices.emplace(source, index);
        output.materials.push_back(std::move(material));
    }
}

int PolygonMaterial(
    FbxNode* node, FbxMesh* mesh, int polygon,
    const std::unordered_map<FbxSurfaceMaterial*, int>& materialIndices)
{
    int localIndex = 0;
    if (FbxGeometryElementMaterial* element = mesh->GetElementMaterial(0))
    {
        if (element->GetMappingMode() == FbxLayerElement::eByPolygon &&
            polygon < element->GetIndexArray().GetCount())
            localIndex = element->GetIndexArray().GetAt(polygon);
        else if (element->GetMappingMode() == FbxLayerElement::eAllSame &&
                 element->GetIndexArray().GetCount() > 0)
            localIndex = element->GetIndexArray().GetAt(0);
    }
    if (localIndex < 0 || localIndex >= node->GetMaterialCount()) return -1;
    auto found = materialIndices.find(node->GetMaterial(localIndex));
    return found == materialIndices.end() ? -1 : found->second;
}

void ReadMeshNode(
    FbxNode* node, bool geometryOnly,
    const std::unordered_map<FbxSurfaceMaterial*, int>& materialIndices,
    ImportedData& output)
{
    FbxMesh* mesh = node->GetMesh();
    if (mesh == nullptr || mesh->GetPolygonCount() == 0) return;
    std::optional<SkeletonData> skeleton = geometryOnly ? std::nullopt : ReadSkeleton(node, mesh);
    FbxAMatrix transform = node->EvaluateGlobalTransform() * GeometryTransform(node);
    FbxGeometryElementNormal* normalElement = mesh->GetElementNormal(0);
    FbxGeometryElementUV* uvElement = mesh->GetElementUV(0);
    FbxGeometryElementVertexColor* colorElement = mesh->GetElementVertexColor(0);
    FbxVector4* controlPoints = mesh->GetControlPoints();
    const int controlPointCount = mesh->GetControlPointsCount();
    std::map<int, MeshData> groups;
    int polygonVertex = 0;
    for (int polygon = 0; polygon < mesh->GetPolygonCount(); ++polygon)
    {
        int polygonSize = mesh->GetPolygonSize(polygon);
        if (polygonSize != 3)
            throw std::runtime_error("FBX triangulation left a non-triangle polygon.");
        int material = PolygonMaterial(node, mesh, polygon, materialIndices);
        MeshData& destination = groups[material];
        destination.materialIndex = material;
        for (int corner = 0; corner < 3; ++corner, ++polygonVertex)
        {
            int controlPoint = mesh->GetPolygonVertex(polygon, corner);
            if (controlPoint < 0 || controlPoint >= controlPointCount)
                throw std::runtime_error("FBX polygon references an invalid control point.");
            FbxVector4 position = controlPoints[controlPoint];
            destination.positions.push_back(skeleton
                ? Vec3{static_cast<float>(position[0]), static_cast<float>(position[1]),
                       static_cast<float>(position[2])}
                : TransformPoint(transform, position));

            int normalIndex = LayerDirectIndex(
                normalElement, controlPoint, polygonVertex, polygon);
            if (normalElement != nullptr)
            {
                FbxVector4 normal = normalIndex >= 0
                    ? normalElement->GetDirectArray().GetAt(normalIndex)
                    : FbxVector4(0, 1, 0, 0);
                destination.normals.push_back(skeleton
                    ? Vec3{static_cast<float>(normal[0]), static_cast<float>(normal[1]),
                           static_cast<float>(normal[2])}
                    : TransformNormal(transform, normal));
            }

            int uvIndex = LayerDirectIndex(uvElement, controlPoint, polygonVertex, polygon);
            if (uvElement != nullptr)
            {
                FbxVector2 uv = uvIndex >= 0
                    ? uvElement->GetDirectArray().GetAt(uvIndex) : FbxVector2(0, 0);
                destination.uvs.push_back(
                    {static_cast<float>(uv[0]), 1.0f - static_cast<float>(uv[1])});
            }

            int colorIndex = LayerDirectIndex(
                colorElement, controlPoint, polygonVertex, polygon);
            if (colorElement != nullptr)
            {
                FbxColor color = colorIndex >= 0
                    ? colorElement->GetDirectArray().GetAt(colorIndex) : FbxColor(1, 1, 1, 1);
                destination.colors.push_back(ToArgb(color));
            }

            destination.indices.push_back(
                static_cast<std::uint32_t>(destination.indices.size()));
            if (skeleton)
            {
                const auto& influences = skeleton->controlPointWeights[
                    static_cast<std::size_t>(controlPoint)];
                destination.joints.push_back(
                    {influences[0].first, influences[1].first,
                     influences[2].first, influences[3].first});
                destination.weights.push_back(
                    {influences[0].second, influences[1].second,
                     influences[2].second, influences[3].second});
            }
        }
    }

    std::string baseName = SafeName(node->GetName(), SafeName(mesh->GetName(), "mesh"));
    int primitive = 0;
    for (auto& [material, destination] : groups)
    {
        destination.name = primitive == 0
            ? baseName : baseName + "_" + std::to_string(primitive);
        if (skeleton) destination.skeleton = skeleton;
        output.meshes.push_back(std::move(destination));
        ++primitive;
    }
}

void CollectMeshNodes(FbxNode* node, std::vector<FbxNode*>& output)
{
    if (node == nullptr) return;
    if (node->GetMesh() != nullptr) output.push_back(node);
    for (int index = 0; index < node->GetChildCount(); ++index)
        CollectMeshNodes(node->GetChild(index), output);
}

ImportedData ImportFbx(
    const fs::path& inputPath, bool allMeshes, bool geometryOnly, const fs::path& extraction)
{
    std::unique_ptr<FbxManager, SdkDestroy> manager(FbxManager::Create());
    if (!manager) throw std::runtime_error("FBX SDK manager creation failed.");
    FbxIOSettings* io = FbxIOSettings::Create(manager.get(), IOSROOT);
    manager->SetIOSettings(io);
    io->SetBoolProp(IMP_FBX_MATERIAL, true);
    io->SetBoolProp(IMP_FBX_TEXTURE, true);
    io->SetBoolProp(IMP_FBX_LINK, true);
    io->SetBoolProp(IMP_FBX_SHAPE, false);
    io->SetBoolProp(IMP_FBX_GOBO, false);
    io->SetBoolProp(IMP_FBX_ANIMATION, false);
    io->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
    io->SetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true);

    FbxScene* scene = FbxScene::Create(manager.get(), "ImportedScene");
    FbxImporter* importer = FbxImporter::Create(manager.get(), "");
    std::string inputUtf8 = ToUtf8(inputPath.wstring());
    std::string extractionUtf8 = ToUtf8(extraction.wstring());
    importer->SetEmbeddingExtractionFolder(extractionUtf8.c_str());
    if (!importer->Initialize(inputUtf8.c_str(), -1, io))
    {
        std::string error = importer->GetStatus().GetErrorString();
        importer->Destroy();
        throw std::runtime_error("FBX import initialization failed: " + error);
    }
    if (!importer->Import(scene))
    {
        std::string error = importer->GetStatus().GetErrorString();
        importer->Destroy();
        throw std::runtime_error("FBX import failed: " + error);
    }
    importer->Destroy();

    FbxAxisSystem::MayaYUp.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);
    FbxGeometryConverter converter(manager.get());
    if (!converter.Triangulate(scene, true, false))
        throw std::runtime_error("FBX SDK could not triangulate the scene.");

    ImportedData result;
    std::unordered_map<FbxSurfaceMaterial*, int> materialIndices;
    ReadMaterialsAndTextures(scene, inputPath, extraction, result, materialIndices);
    std::vector<FbxNode*> nodes;
    CollectMeshNodes(scene->GetRootNode(), nodes);
    bool containsSkinnedMesh = std::any_of(nodes.begin(), nodes.end(), [](FbxNode* node)
    {
        return HasSkin(node->GetMesh());
    });
    for (FbxNode* node : nodes)
    {
        if (!geometryOnly && !allMeshes && containsSkinnedMesh && !HasSkin(node->GetMesh()))
            continue;
        ReadMeshNode(node, geometryOnly, materialIndices, result);
    }
    if (result.meshes.empty()) throw std::runtime_error("FBX contains no usable mesh objects.");
    return result;
}

template<typename T>
void WriteArray(BinaryWriter& writer, const std::vector<T>& values)
{
    writer.Pod(static_cast<std::uint32_t>(values.size()));
    writer.Bytes(values.data(), values.size() * sizeof(T));
}

void WriteImportPayload(const ImportedData& scene, const fs::path& outputPath)
{
    BinaryWriter writer(outputPath);
    writer.Bytes(ImportMagic.data(), ImportMagic.size());
    writer.Pod(ProtocolVersion);
    writer.Pod(static_cast<std::uint32_t>(scene.textures.size()));
    for (const TextureData& texture : scene.textures)
    {
        writer.String(texture.name);
        writer.String(texture.mime);
        writer.Pod<std::int32_t>(0);
        writer.Pod<std::int32_t>(0);
        WriteArray(writer, texture.bytes);
        writer.String(texture.sourcePath);
    }
    writer.Pod(static_cast<std::uint32_t>(scene.materials.size()));
    for (const MaterialData& material : scene.materials)
    {
        writer.String(material.name);
        writer.String(material.textureName);
        writer.Pod(material.textureIndex);
    }
    writer.Pod(static_cast<std::uint32_t>(scene.meshes.size()));
    for (const MeshData& mesh : scene.meshes)
    {
        writer.String(mesh.name);
        writer.Pod(mesh.materialIndex);
        WriteArray(writer, mesh.positions);
        WriteArray(writer, mesh.normals);
        WriteArray(writer, mesh.uvs);
        WriteArray(writer, mesh.colors);
        WriteArray(writer, mesh.indices);
        writer.Pod<std::uint8_t>(mesh.skeleton.has_value() ? 1 : 0);
        if (!mesh.skeleton) continue;
        const SkeletonData& skeleton = *mesh.skeleton;
        writer.String(skeleton.name);
        writer.Pod(static_cast<std::uint32_t>(skeleton.names.size()));
        for (std::size_t joint = 0; joint < skeleton.names.size(); ++joint)
        {
            writer.String(skeleton.names[joint]);
            writer.Pod(skeleton.parents[joint]);
            writer.Pod(skeleton.inverseBind[joint]);
            writer.Pod(skeleton.bindWorld[joint]);
            writer.Pod(skeleton.bindLocal[joint]);
        }
        WriteArray(writer, mesh.joints);
        WriteArray(writer, mesh.weights);
    }
}

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        fs::path base = fs::temp_directory_path();
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            std::wstring name = L"smo-fbx-native-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt);
            path_ = base / name;
            std::error_code error;
            if (fs::create_directory(path_, error)) return;
        }
        throw std::runtime_error("Cannot create a temporary FBX extraction directory.");
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

int ImportCommand(int argc, wchar_t** argv)
{
    if (argc < 4)
        throw std::runtime_error(
            "Usage: SmoFbxBridge import <input.fbx> <output.bin> [--all-meshes] [--geometry-only]");
    bool allMeshes = false;
    bool geometryOnly = false;
    for (int index = 4; index < argc; ++index)
    {
        std::wstring option = argv[index];
        if (option == L"--all-meshes") allMeshes = true;
        else if (option == L"--geometry-only") geometryOnly = true;
        else throw std::runtime_error("Unknown FBX import option: " + ToUtf8(option));
    }
    TemporaryDirectory temporary;
    ImportedData scene = ImportFbx(argv[2], allMeshes, geometryOnly, temporary.Path());
    WriteImportPayload(scene, argv[3]);
    return 0;
}
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc >= 2 && std::wstring(argv[1]) == L"import")
            return ImportCommand(argc, argv);
        if (argc >= 2 && std::wstring(argv[1]) == L"export")
            return ExportCommandNative(argc, argv);
        if (argc >= 2 && std::wstring(argv[1]) == L"inspect-export")
            return InspectExportCommandNative(argc, argv);
        if (argc >= 2 && std::wstring(argv[1]) == L"--version")
        {
            std::cout << "SmoFbxBridge protocol " << ProtocolVersion
                      << ", Autodesk FBX SDK " << FBXSDK_VERSION_STRING << '\n';
            return 0;
        }
        throw std::runtime_error("Expected command 'import', 'export', 'inspect-export' or '--version'.");
    }
    catch (const std::exception& exception)
    {
        std::cerr << "SmoFbxBridge: " << exception.what() << '\n';
        return 1;
    }
}

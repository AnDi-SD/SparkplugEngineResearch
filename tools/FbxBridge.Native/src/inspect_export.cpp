#include <fbxsdk.h>
#include <Windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
std::string Utf8(const wchar_t* value) {
    const int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,nullptr,0,nullptr,nullptr);
    if(count<=0)throw std::runtime_error("Invalid inspection path encoding");
    std::string result(static_cast<std::size_t>(count),'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value,-1,result.data(),count,nullptr,nullptr);
    result.pop_back();return result;
}
void String(std::ostream& out,const char* value) {
    out<<'"';
    for(const auto* p=reinterpret_cast<const unsigned char*>(value);*p;++p) {
        if(*p=='"'||*p=='\\')out<<'\\'<<static_cast<char>(*p);
        else if(*p<32)out<<"\\u00"<<"0123456789abcdef"[*p>>4]<<"0123456789abcdef"[*p&15];
        else out<<static_cast<char>(*p);
    }
    out<<'"';
}
struct DestroyManager {void operator()(FbxManager* value)const{if(value)value->Destroy();}};
}

// SDK readback for export integration checks; it does not recreate game logic.
int InspectExportCommandNative(int argc,wchar_t** argv) {
    if(argc!=4)throw std::runtime_error("Usage: SmoFbxBridge inspect-export <input.fbx> <report.json>");
    if(std::filesystem::file_size(argv[2])>256ull*1024*1024)throw std::runtime_error("Inspection input exceeds 256 MiB");
    std::unique_ptr<FbxManager,DestroyManager> manager(FbxManager::Create());
    if(!manager)throw std::runtime_error("Cannot create FBX inspection manager");
    auto* settings=FbxIOSettings::Create(manager.get(),IOSROOT);manager->SetIOSettings(settings);
    auto* importer=FbxImporter::Create(manager.get(),"");
    const auto path=Utf8(argv[2]);
    if(!importer->Initialize(path.c_str(),-1,settings))throw std::runtime_error(importer->GetStatus().GetErrorString());
    auto* scene=FbxScene::Create(manager.get(),"Inspection");
    if(!importer->Import(scene))throw std::runtime_error(importer->GetStatus().GetErrorString());
    importer->Destroy();
    std::ofstream out(std::filesystem::path(argv[3]),std::ios::binary);
    if(!out)throw std::runtime_error("Cannot write FBX inspection report");
    out<<std::setprecision(17)<<"{\"status\":\"passed\",\"meshes\":[";
    bool first=true;
    std::function<void(FbxNode*)> visit=[&](FbxNode* node) {
        if(auto* mesh=node->GetMesh()) {
            if(!first)out<<',';first=false;
            out<<"{\"name\":";String(out,node->GetName());
            const auto metadata=[&](const char* key,const char* jsonKey) {
                out<<",\""<<jsonKey<<"\":";auto value=node->FindProperty(key);
                if(value.IsValid())out<<value.Get<FbxInt>();else out<<"null";
            };
            metadata("SparkplugSceneObjectIndex","renderable");metadata("SparkplugMeshObjectIndex","mesh");
            metadata("SparkplugMaterialObjectIndex","material");metadata("SparkplugContainerObjectIndex","container");
            metadata("SparkplugMemberSlot","slot");
            out<<",\"attribute_id\":"<<mesh->GetUniqueID()<<",\"vertices\":"<<mesh->GetControlPointsCount()
                <<",\"polygons\":"<<mesh->GetPolygonCount()<<",\"skin_deformers\":"<<mesh->GetDeformerCount(FbxDeformer::eSkin);
            const auto world=node->EvaluateGlobalTransform();out<<",\"world\":[";
            for(int row=0;row<4;++row)for(int column=0;column<4;++column) {
                if(row||column)out<<',';out<<world.Get(row,column);
            }
            out<<"],\"materials\":[";
            for(int index=0;index<node->GetMaterialCount();++index) {
                if(index)out<<',';auto* material=node->GetMaterial(index);out<<"{\"name\":";String(out,material->GetName());
                if(auto* lambert=FbxCast<FbxSurfaceLambert>(material)) {
                    const auto color=lambert->Diffuse.Get();out<<",\"diffuse\":["<<color[0]<<','<<color[1]<<','<<color[2]<<']';
                }
                out<<'}';
            }
            out<<"]}";
        }
        for(int i=0;i<node->GetChildCount();++i)visit(node->GetChild(i));
    };
    visit(scene->GetRootNode());out<<"]}\n";
    if(!out)throw std::runtime_error("Failed to finish FBX inspection report");
    return 0;
}

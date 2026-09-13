// Own Vulkan compute harness. Executes the frozen renderer SPIR-V directly;
// it does not load Remix, bridge, a game process or any game assets.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <string>
#define main CpuSkinStrideMain
#include "test_skin_strides.cpp"
#undef main

static_assert(sizeof(SkinningArgs)==16448 && offsetof(SkinningArgs,dstPositionOffset)==16384);
static_assert(offsetof(SkinningArgs,numVertices)==16432);
static void Vk(VkResult result,const char* label){if(result!=VK_SUCCESS)throw std::runtime_error(std::string(label)+": "+std::to_string(result));}
struct Buffer {
  VkDevice device{};VkBuffer buffer{};VkDeviceMemory memory{};void* mapped{};size_t size{};
  Buffer()=default;Buffer(const Buffer&)=delete;Buffer& operator=(const Buffer&)=delete;
  ~Buffer(){if(mapped)vkUnmapMemory(device,memory);if(buffer)vkDestroyBuffer(device,buffer,nullptr);if(memory)vkFreeMemory(device,memory,nullptr);}
};
struct Compute {
  VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};
  VkDescriptorSetLayout setLayout{};VkPipelineLayout pipelineLayout{};VkPipeline pipeline{};
  VkDescriptorPool descriptorPool{};VkDescriptorSet descriptorSet{};VkCommandPool commandPool{};
  VkCommandBuffer command{};VkFence fence{};VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceMemoryProperties memoryProperties{};
  unsigned queueFamily=0;
  explicit Compute(const char* shader){try{
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Sparkplug skinning compute evidence";app.apiVersion=VK_API_VERSION_1_3;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};create.pApplicationInfo=&app;
    Vk(vkCreateInstance(&create,nullptr,&instance),"instance");
    uint32_t count=0;Vk(vkEnumeratePhysicalDevices(instance,&count,nullptr),"device count");
    if(!count||count>16)throw std::runtime_error("physical device bound");
    std::vector<VkPhysicalDevice> devices(count);Vk(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"devices");
    for(auto candidate:devices){VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(candidate,&p);
      if(p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){physical=candidate;properties=p;break;}}
    if(!physical)throw std::runtime_error("requires a discrete GPU; no CPU fallback");
    if(properties.apiVersion<VK_API_VERSION_1_3 || properties.limits.maxUniformBufferRange<sizeof(SkinningArgs))throw std::runtime_error("GPU API/uniform range");
    vkGetPhysicalDeviceMemoryProperties(physical,&memoryProperties);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
    bool found=false;for(unsigned i=0;i<count;++i)if(families[i].queueFlags&VK_QUEUE_COMPUTE_BIT){queueFamily=i;found=true;break;}
    if(!found)throw std::runtime_error("no compute queue");
    float priority=1;VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queueInfo.queueFamilyIndex=queueFamily;queueInfo.queueCount=1;queueInfo.pQueuePriorities=&priority;
    VkPhysicalDeviceFeatures features{};vkGetPhysicalDeviceFeatures(physical,&features);VkPhysicalDeviceFeatures enabled{};enabled.robustBufferAccess=features.robustBufferAccess;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};deviceInfo.queueCreateInfoCount=1;deviceInfo.pQueueCreateInfos=&queueInfo;deviceInfo.pEnabledFeatures=&enabled;
    Vk(vkCreateDevice(physical,&deviceInfo,nullptr,&device),"device");vkGetDeviceQueue(device,queueFamily,0,&queue);
    std::array<VkDescriptorSetLayoutBinding,7> bindings{};
    for(unsigned i=0;i<7;++i){bindings[i].binding=i;bindings[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};layout.bindingCount=7;layout.pBindings=bindings.data();Vk(vkCreateDescriptorSetLayout(device,&layout,nullptr,&setLayout),"set layout");
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layoutInfo.setLayoutCount=1;layoutInfo.pSetLayouts=&setLayout;Vk(vkCreatePipelineLayout(device,&layoutInfo,nullptr,&pipelineLayout),"pipeline layout");
    std::ifstream file(shader,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("shader open");const auto size=static_cast<std::streamoff>(file.tellg());
    if(size<20||size>65536||size%4)throw std::runtime_error("shader size bound");std::vector<uint32_t> code(size_t(size)/4);file.seekg(0);file.read(reinterpret_cast<char*>(code.data()),size);if(!file||code[0]!=0x07230203)throw std::runtime_error("shader bytes");
    VkShaderModuleCreateInfo shaderInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shaderInfo.codeSize=size_t(size);shaderInfo.pCode=code.data();VkShaderModule module{};Vk(vkCreateShaderModule(device,&shaderInfo,nullptr,&module),"shader module");
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipelineInfo.layout=pipelineLayout;pipelineInfo.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pipelineInfo.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipelineInfo.stage.module=module;pipelineInfo.stage.pName="main";
    const auto pipelineResult=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&pipeline);vkDestroyShaderModule(device,module,nullptr);Vk(pipelineResult,"compute pipeline");
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=2;pool.pPoolSizes=sizes;Vk(vkCreateDescriptorPool(device,&pool,nullptr,&descriptorPool),"descriptor pool");
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocate.descriptorPool=descriptorPool;allocate.descriptorSetCount=1;allocate.pSetLayouts=&setLayout;Vk(vkAllocateDescriptorSets(device,&allocate,&descriptorSet),"descriptor set");
    VkCommandPoolCreateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};commandInfo.queueFamilyIndex=queueFamily;commandInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;Vk(vkCreateCommandPool(device,&commandInfo,nullptr,&commandPool),"command pool");
    VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};commands.commandPool=commandPool;commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;commands.commandBufferCount=1;Vk(vkAllocateCommandBuffers(device,&commands,&command),"command buffer");
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};Vk(vkCreateFence(device,&fenceInfo,nullptr,&fence),"fence");
    std::fprintf(stderr,"GPU %s vendor=%04x device=%04x API=%u driver=%u queue=%u robust=%u\n",properties.deviceName,properties.vendorID,properties.deviceID,properties.apiVersion,properties.driverVersion,queueFamily,enabled.robustBufferAccess);
  }catch(...){Destroy();throw;}}
  ~Compute(){Destroy();}
  void Destroy(){if(device){vkDeviceWaitIdle(device);if(fence)vkDestroyFence(device,fence,nullptr);if(commandPool)vkDestroyCommandPool(device,commandPool,nullptr);if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);if(setLayout)vkDestroyDescriptorSetLayout(device,setLayout,nullptr);vkDestroyDevice(device,nullptr);}if(instance)vkDestroyInstance(instance,nullptr);}
  void Allocate(Buffer& result,const void* input,size_t size,unsigned binding){
    result.device=device;result.size=size;VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};create.size=size;create.usage=binding==0?VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT:VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    Vk(vkCreateBuffer(device,&create,nullptr,&result.buffer),"buffer");VkMemoryRequirements required{};vkGetBufferMemoryRequirements(device,result.buffer,&required);
    unsigned memoryType=memoryProperties.memoryTypeCount;
    for(unsigned i=0;i<memoryProperties.memoryTypeCount;++i)if((required.memoryTypeBits&(1u<<i)) && (memoryProperties.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){memoryType=i;break;}
    if(memoryType==memoryProperties.memoryTypeCount||required.size>1024*1024)throw std::runtime_error("host memory/buffer bound");
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=required.size;allocate.memoryTypeIndex=memoryType;Vk(vkAllocateMemory(device,&allocate,nullptr,&result.memory),"memory");
    Vk(vkBindBufferMemory(device,result.buffer,result.memory,0),"bind memory");Vk(vkMapMemory(device,result.memory,0,size,0,&result.mapped),"map memory");memcpy(result.mapped,input,size);
  }
  void Execute(std::array<Buffer,7>& buffers,unsigned count){
    std::array<VkDescriptorBufferInfo,7> info{};std::array<VkWriteDescriptorSet,7> writes{};
    for(unsigned i=0;i<7;++i){info[i]={buffers[i].buffer,0,buffers[i].size};writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=descriptorSet;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&info[i];}
    vkUpdateDescriptorSets(device,7,writes.data(),0,nullptr);Vk(vkResetCommandPool(device,commandPool,0),"reset command pool");Vk(vkResetFences(device,1,&fence),"reset fence");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;Vk(vkBeginCommandBuffer(command,&begin),"begin");
    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;host.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_UNIFORM_READ_BIT;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&host,0,nullptr,0,nullptr);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout,0,1,&descriptorSet,0,nullptr);vkCmdDispatch(command,(count+127)/128,1,1);
    VkMemoryBarrier read{VK_STRUCTURE_TYPE_MEMORY_BARRIER};read.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;read.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&read,0,nullptr,0,nullptr);
    Vk(vkEndCommandBuffer(command),"end");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;Vk(vkQueueSubmit(queue,1,&submit,fence),"submit");
    const auto completion=vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ull);
    // Buffers must stay alive while submitted work remains. The outer owned
    // process watchdog bounds a stalled driver during this failure cleanup.
    if(completion!=VK_SUCCESS)vkDeviceWaitIdle(device);Vk(completion,"five-second GPU fence");
  }
};

int main(int argc,char** argv){try{
  if(argc!=2)throw std::runtime_error("usage: test_gpu_skinning frozen-gpu_skinning.spv");
  unsigned dispatches=0,verticesChecked=0,stockWrong=0,indexOnlyWrong=0;float maxError=0,maxOriginalError=0;
  {Compute gpu(argv[1]);
    std::printf("{\"event\":\"device\",\"vendor\":%u,\"device\":%u,\"apiVersion\":%u,\"driverVersion\":%u,\"validationLayer\":false}\n",gpu.properties.vendorID,gpu.properties.deviceID,gpu.properties.apiVersion,gpu.properties.driverVersion);std::fflush(stdout);
    for(unsigned n:{3u,325u})for(unsigned b:{1u,2u,3u,4u,5u,8u}){
      Run run(b,n);
      for(unsigned v=0;v<n;++v){run.normals[v*3]=float(v%3)*.25f;run.normals[v*3+1]=.5f;}
      for(unsigned bone=0;bone<16;++bone){dxvk::Matrix4 matrix;memcpy(&matrix,&run.args.bones[bone],sizeof(matrix));matrix[0][0]=1+bone*.125f;matrix[1][1]=.75f+bone*.03125f;matrix[2][2]=.5f+bone*.0625f;matrix[1][0]=bone*.015625f;memcpy(&run.args.bones[bone],&matrix,sizeof(matrix));}
      run.args.dstPositionOffset=16;run.args.dstPositionStride=20;run.args.dstNormalOffset=8;run.args.dstNormalStride=16;
      const unsigned padded=((n+127)/128)*128;const float sentinel=-1234567.25f;
      std::vector<float> referencePosition(padded*5+16,sentinel),referenceNormal(padded*4+16,sentinel);
      auto cpu=[&](unsigned weightStride,unsigned indexStride,std::vector<float>& p,std::vector<float>& normal){run.args.blendWeightStride=weightStride;run.args.blendIndicesStride=indexStride;for(unsigned v=0;v<n;++v)dxvk::skinning(v,p.data(),normal.data(),run.positions.data(),run.weights.data(),reinterpret_cast<const uint8_t*>(run.packed.data()),run.normals.data(),run.args);};
      cpu(FixedWeightStride(run.source),FixedIndicesStride(run.source),referencePosition,referenceNormal);
      for(unsigned variant=0;variant<3;++variant){
        const unsigned ws=variant==0?StockWeightStride(run.source):FixedWeightStride(run.source),is=variant==2?FixedIndicesStride(run.source):StockIndicesStride(run.source);
        std::vector<float> expectedPosition(padded*5+16,sentinel),expectedNormal(padded*4+16,sentinel);cpu(ws,is,expectedPosition,expectedNormal);
        std::vector<float> initialPosition(expectedPosition.size(),sentinel),initialNormal(expectedNormal.size(),sentinel);std::array<Buffer,7> buffers;
        gpu.Allocate(buffers[0],&run.args,sizeof(run.args),0);gpu.Allocate(buffers[1],initialPosition.data(),initialPosition.size()*4,1);gpu.Allocate(buffers[2],run.positions.data(),run.positions.size()*4,2);
        gpu.Allocate(buffers[3],run.weights.data(),run.weights.size()*4,3);gpu.Allocate(buffers[4],run.packed.data(),run.packed.size()*4,4);gpu.Allocate(buffers[5],initialNormal.data(),initialNormal.size()*4,5);gpu.Allocate(buffers[6],run.normals.data(),run.normals.size()*4,6);
        gpu.Execute(buffers,n);unsigned mismatches=0;
        for(unsigned channel:{1u,5u}){const auto& expected=channel==1?expectedPosition:expectedNormal;const auto* actual=static_cast<const float*>(buffers[channel].mapped);
          for(size_t i=0;i<expected.size();++i){if(expected[i]==sentinel)Check(actual[i]==sentinel,"GPU preserves output offsets, padding and excess workgroup lanes");else{Check(std::isfinite(actual[i]),"finite GPU output");const float error=std::fabs(actual[i]-expected[i]);maxError=std::max(maxError,error);Check(error<0.0001f,"GPU matches actual shared CPU skinning header");}}}
        const auto* actual=static_cast<const float*>(buffers[1].mapped);const auto* normal=static_cast<const float*>(buffers[5].mapped);
        for(unsigned v=0;v<n;++v){bool different=false;for(unsigned c=0;c<3;++c)different|=std::fabs(actual[4+v*5+c]-referencePosition[4+v*5+c])>.0001f;if(different)++mismatches;
          if(variant==2&&b<=4){const auto original=Original(run,v);for(unsigned c=0;c<3;++c){const float error=std::max(std::fabs(actual[4+v*5+c]-original.position[c]),std::fabs(normal[2+v*4+c]-original.normal[c]));maxOriginalError=std::max(maxOriginalError,error);Check(error<.0001f,"GPU fixed B1-B4 agrees with recovered original Fixed expressions");}}}
        if(variant==0){Check((b==1)==(mismatches==0),"GPU stock control detects multiweight defect");stockWrong+=mismatches;}
        if(variant==1){Check((b<=4)==(mismatches==0),"GPU weight-only control detects packed index stride defect");indexOnlyWrong+=mismatches;}
        if(variant==2)Check(mismatches==0,"GPU v2 strides match fixed reference");
        ++dispatches;verticesChecked+=n;std::printf("{\"event\":\"case\",\"vertices\":%u,\"bones\":%u,\"variant\":%u,\"weightStride\":%u,\"indexStride\":%u,\"wrongVsFixed\":%u}\n",n,b,variant,ws,is,mismatches);std::fflush(stdout);
      }
    }
  }
  std::printf("{\"status\":\"PASS\",\"gpu\":true,\"dispatches\":%u,\"verticesChecked\":%u,\"checks\":%u,\"stockWrongVertices\":%u,\"weightOnlyWrongVertices\":%u,\"maxCpuGpuError\":%.9g,\"maxOriginalError\":%.9g,\"fullRenderer\":false,\"resourcesDestroyed\":true}\n",dispatches,verticesChecked,checks,stockWrong,indexOnlyWrong,maxError,maxOriginalError);return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}}

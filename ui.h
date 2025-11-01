#pragma once
#include <imgui.h>
#include <imnodes.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>

#include <shader/ShaderReflection.h>
#include <shader/ShaderCompiler.h>
#include "TextEditor.h"

#include "Engine.h"

enum class PinType {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage,
    InputAttachment,
    PushConstant,
    VertexInput,
    FragmentOutput,
    ShaderStageIn,
    ShaderStageOut
};

enum class NodeType {
    Undefined,
    ImageResource,
    BufferResource,
    RenderTarget,
    ShaderGraph
};

const char* PinTypeToString(PinType type) {
    switch(type) {
        case PinType::UniformBuffer: return "UBO";
        case PinType::StorageBuffer: return "SSBO";
        case PinType::CombinedImageSampler: return "Sampler";
        case PinType::StorageImage: return "Storage Image";
        case PinType::InputAttachment: return "Input Attachment";
        case PinType::PushConstant: return "Push Constants";
        case PinType::VertexInput: return "Vertex Input";
        case PinType::FragmentOutput: return "Fragment Output";
        case PinType::ShaderStageIn: return "Shader Stage In";
        case PinType::ShaderStageOut: return "Shader Stage Out";
        default: return "Unknown";
    }
}

VkDescriptorType PinTypeToDescriptorType(PinType type) {
    switch(type) {
        case PinType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case PinType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case PinType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case PinType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case PinType::InputAttachment: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

struct Pin {
    int id;
    std::string name;
    bool isInput;
    PinType type;

    uint32_t set = 0;
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;

    size_t size = 0;

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent = {0, 0, 0};

    uint32_t location = 0;
};

struct Link {
    int id;
    int startPinId;
    int endPinId;
};

class GraphNode {
public:
    NodeType nodeType = NodeType::Undefined;
    int id;
    ImVec2 position;
    std::string name;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;

    GraphNode(int nodeId, const std::string& nodeName, NodeType type)
        : id(nodeId), name(nodeName), nodeType(type) {}

    virtual ~GraphNode() = default;
    virtual void Draw() = 0;
    virtual void DrawProperties() {}
    virtual const char* GetTypeName() const = 0;
};

// Forward declarations
class ShaderGraphNode;
class RenderTargetNode;

// NEW: Pipeline builder structure
struct PipelineBuilder {
    ShaderGraphNode* vertexShader = nullptr;
    ShaderGraphNode* fragmentShader = nullptr;
    std::vector<ShaderGraphNode*> shaderChain;
    RenderTargetNode* renderTarget = nullptr;

    // Collected resources from all connected nodes
    std::unordered_map<int, std::pair<uint32_t, uint32_t>> descriptorBindings; // pinId -> (set, binding)
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<VkFormat> colorAttachmentFormats;

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkViewport viewport{};
    VkRect2D scissor{};

    bool IsValid() const {
        return vertexShader && fragmentShader && renderTarget;
    }
};

class ImageResourceNode : public GraphNode {
private:
    VkDevice device;
    ResourceManager* resourceManager;
public:
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;

    std::shared_ptr<Image> image;
    std::shared_ptr<Sampler> meowSampler;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet meowTargetSet;

    ImageResourceNode(int nodeId, ResourceManager* resourceManager, VkDescriptorPool descriptorPool, VkDevice device)
        : GraphNode(nodeId, "Image", NodeType::ImageResource), resourceManager(resourceManager),
          format(VK_FORMAT_R8G8B8A8_SRGB),
          extent{1024, 1024, 1},
          mipLevels(1),
          arrayLayers(1),
          descriptorPool(descriptorPool),
          device(device)
    {
        meowSampler = resourceManager->createSampler({}, "ImageSampler"+std::to_string(id));

        Pin output;
        output.id = nodeId * 1000;
        output.name = "Image";
        output.isInput = false;
        output.type = PinType::CombinedImageSampler;
        output.format = format;
        output.extent = extent;
        outputs.push_back(output);

        image = resourceManager->createImage({
              .imageType = VK_IMAGE_TYPE_2D,
              .format = format,
              .extent = extent,
              .mipLevels = mipLevels,
              .arrayLayers = arrayLayers,
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .tiling = VK_IMAGE_TILING_OPTIMAL,
              .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
              //.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        });

        image->createImageView({.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = image->getFormat(), .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}});

        VkDescriptorSetLayout imguiLayout = ImGui_ImplVulkan_GetDescriptorSetLayout();
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &imguiLayout;
        vkAllocateDescriptorSets(device, &allocInfo, &meowTargetSet);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = image->getImageView();
        imageInfo.sampler = meowSampler->getSampler();

        VkWriteDescriptorSet writeMeow{};
        writeMeow.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeMeow.descriptorCount = 1;
        writeMeow.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeMeow.dstSet = meowTargetSet;
        writeMeow.dstBinding = 0;
        writeMeow.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &writeMeow, 0, nullptr);
    }

    ~ImageResourceNode() {
        vkFreeDescriptorSets(device, descriptorPool, 1, &meowTargetSet);
        resourceManager->destroySampler("ImageSampler"+std::to_string(id));
    }

    const char* GetTypeName() const override { return "Image Resource"; }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("Image");
        ImGui::SameLine();
        ImGui::Text("%dx%d", extent.width, extent.height);
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginOutputAttribute(outputs[0].id);
        float scale = 200.0f;
        glm::vec2 size = glm::normalize(glm::vec2(extent.width, extent.height))*scale;
        ImGui::Image(meowTargetSet, ImVec2(size.x, size.y));
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Image Resource Properties");
        ImGui::Separator();

        const char* formats[] = {
            "R8G8B8A8_SRGB", "R8G8B8A8_UNORM", "R16G16B16A16_SFLOAT",
            "R32G32B32A32_SFLOAT", "B8G8R8A8_SRGB", "D32_SFLOAT"
        };
        const VkFormat formatMap[] = {
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_D32_SFLOAT
        };

        int formatIdx = 0;
        for (int i = 0; i < IM_ARRAYSIZE(formatMap); i++) {
            if (formatMap[i] == format) { formatIdx = i; break; }
        }

        bool update = false;

        if (ImGui::Combo("Format", &formatIdx, formats, IM_ARRAYSIZE(formats))) {
            format = formatMap[formatIdx];
            outputs[0].format = format;
            update = true;
        }

        int w = extent.width, h = extent.height;
        if (ImGui::InputInt("Width", &w)) {
            extent.width = std::max(1, w);
            outputs[0].extent = extent;
            update = true;
        }
        if (ImGui::InputInt("Height", &h)) {
            extent.height = std::max(1, h);
            outputs[0].extent = extent;
            update = true;
        }

        int mips = mipLevels;
        if (ImGui::InputInt("Mip Levels", &mips)) {
            mipLevels = std::max(1, mips);
            update = true;
        }

        int layers = arrayLayers;
        if (ImGui::InputInt("Array Layers", &layers)) {
            arrayLayers = std::max(1, layers);
            update = true;
        }

        if (update) {
            vkFreeDescriptorSets(device, descriptorPool, 1, &meowTargetSet);

            image = resourceManager->createImage({
                  .imageType = VK_IMAGE_TYPE_2D,
                  .format = format,
                  .extent = extent,
                  .mipLevels = mipLevels,
                  .arrayLayers = arrayLayers,
                  .samples = VK_SAMPLE_COUNT_1_BIT,
                  .tiling = VK_IMAGE_TILING_OPTIMAL,
                  .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                  .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            });

            image->createImageView({.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = image->getFormat(), .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}});

            VkDescriptorSetLayout imguiLayout = ImGui_ImplVulkan_GetDescriptorSetLayout();
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &imguiLayout;
            vkAllocateDescriptorSets(device, &allocInfo, &meowTargetSet);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = image->getImageView();
            imageInfo.sampler = meowSampler->getSampler();

            VkWriteDescriptorSet writeMeow{};
            writeMeow.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeMeow.descriptorCount = 1;
            writeMeow.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeMeow.dstSet = meowTargetSet;
            writeMeow.dstBinding = 0;
            writeMeow.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, 1, &writeMeow, 0, nullptr);
        }
    }
};

class BufferResourceNode : public GraphNode {
private:
    ResourceManager* resourceManager;
public:
    size_t size;
    bool isUniform;

    std::shared_ptr<Buffer> buffer;

    BufferResourceNode(int nodeId, ResourceManager* resourceManager, bool uniform = true)
        : GraphNode(nodeId, uniform ? "Uniform Buffer" : "Storage Buffer", NodeType::BufferResource),
          size(256),
          isUniform(uniform),
          resourceManager(resourceManager) {
        Pin output;
        output.id = nodeId * 1000;
        output.name = "Buffer";
        output.isInput = false;
        output.type = uniform ? PinType::UniformBuffer : PinType::StorageBuffer;
        output.size = size;
        outputs.push_back(output);

        buffer = resourceManager->createBuffer({.size = size, .usage = static_cast<VkBufferUsageFlags>((isUniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) | VK_BUFFER_USAGE_TRANSFER_DST_BIT)});
    }

    const char* GetTypeName() const override { return isUniform ? "Uniform Buffer" : "Storage Buffer"; }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(isUniform ? "UBO" : "SSBO");
        ImNodes::EndNodeTitleBar();

        ImGui::Text("%zu bytes", size);

        ImNodes::BeginOutputAttribute(outputs[0].id);
        ImGui::Indent(60);
        ImGui::Text("→");
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("%s Properties", GetTypeName());
        ImGui::Separator();

        bool update = false;

        int sizeKB = size / 1024;
        if (ImGui::InputInt("Size (KB)", &sizeKB)) {
            size = std::max(1, sizeKB) * 1024;
            outputs[0].size = size;
            update = true;
        }

        if (update) {
            buffer = resourceManager->createBuffer({.size = size, .usage = static_cast<VkBufferUsageFlags>((isUniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) | VK_BUFFER_USAGE_TRANSFER_DST_BIT)});
        }

        ImGui::Text("Size in bytes: %zu", size);
    }
};

// FIXME: REMOVE URGENTLY
VkDescriptorSet debugTexture = VK_NULL_HANDLE;

// NEW: Render Target Node - represents the final output
class RenderTargetNode : public GraphNode {
private:
    VkDevice device;
    ResourceManager* resourceManager;
    VkDescriptorPool descriptorPool;

public:
    VkFormat format;
    VkExtent3D extent;

    std::shared_ptr<Image> targetImage;
    std::shared_ptr<Sampler> sampler;
    VkDescriptorSet displaySet;

    // FIXME: WHAT THE FUCK?????????????????????
    // FIXME: THIS IS FUCKING CURSED!!!
    std::unique_ptr<RenderPass>* renderPassReference;
    std::unique_ptr<Framebuffer> framebuffer;

    RenderTargetNode(int nodeId, ResourceManager* resourceManager, VkDescriptorPool descriptorPool, VkDevice device, std::unique_ptr<RenderPass>* renderPass)
        : GraphNode(nodeId, "Render Target", NodeType::RenderTarget),
          resourceManager(resourceManager),
          descriptorPool(descriptorPool),
          device(device),
          format(VK_FORMAT_R8G8B8A8_SRGB),
          extent{1024, 1024, 1},
          renderPassReference(renderPass)
    {
        // Input pin for fragment shader output
        Pin input;
        input.id = nodeId * 1000;
        input.name = "Color";
        input.isInput = true;
        input.type = PinType::FragmentOutput;
        input.location = 0;
        inputs.push_back(input);

        CreateRenderTarget();
    }

    ~RenderTargetNode() {
        if (displaySet != VK_NULL_HANDLE) {
            //vkFreeDescriptorSets(device, descriptorPool, 1, &displaySet);
        }
        resourceManager->destroySampler("RenderTargetSampler" + std::to_string(id));
    }

    void CreateRenderTarget() {
        sampler = resourceManager->createSampler({}, "RenderTargetSampler" + std::to_string(id));

        targetImage = resourceManager->createImage({
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            //.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        });

        targetImage->createImageView({
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        });

        // Create descriptor set for ImGui display
        /*
        VkDescriptorSetLayout imguiLayout = ImGui_ImplVulkan_GetDescriptorSetLayout();
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &imguiLayout;
        vkAllocateDescriptorSets(device, &allocInfo, &displaySet);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = targetImage->getImageView();
        imageInfo.sampler = sampler->getSampler();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.dstSet = displaySet;
        write.dstBinding = 0;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        */

        displaySet = ImGui_ImplVulkan_AddTexture(sampler->getSampler(), targetImage->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        framebuffer = std::make_unique<Framebuffer>(device, **renderPassReference);
        framebuffer->create({targetImage->getImageView()}, extent.width, extent.height);
    }

    const char* GetTypeName() const override { return "Render Target"; }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("RT Render Target");
        ImNodes::EndNodeTitleBar();

        ImNodes::BeginInputAttribute(inputs[0].id);
        ImGui::Text("← Color Output");
        ImNodes::EndInputAttribute();

        //ImGui::Separator();
        ImGui::Text("Output: %dx%d", extent.width, extent.height);

        float scale = 200.0f;
        glm::vec2 size = glm::normalize(glm::vec2(extent.width, extent.height)) * scale;
        ImGui::Image(displaySet, ImVec2(size.x, size.y));
        //ImGui::Image(debugTexture, ImVec2(size.x, size.y));

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Render Target Properties");
        ImGui::Separator();

        int w = extent.width, h = extent.height;
        bool needsUpdate = false;

        if (ImGui::InputInt("Width", &w)) {
            extent.width = std::max(1, w);
            needsUpdate = true;
        }
        if (ImGui::InputInt("Height", &h)) {
            extent.height = std::max(1, h);
            needsUpdate = true;
        }

        if (needsUpdate) {
            //vkFreeDescriptorSets(device, descriptorPool, 1, &displaySet);
            ImGui_ImplVulkan_RemoveTexture(displaySet);
            CreateRenderTarget();
        }
    }
};

class ShaderGraphNode : public GraphNode {
public:
    std::string shaderPath;
    std::string sourceCode;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirvCode;
    std::unique_ptr<ShaderReflection> reflection;
    bool loaded = false;
    std::string compileError;

    // NEW: Stage connection pins
    int stageOutputPinId = -1;
    int stageInputPinId = -1;

    ShaderGraphNode(int nodeId, VkShaderStageFlagBits shaderStage)
        : GraphNode(nodeId, "Shader", NodeType::ShaderGraph), stage(shaderStage) {
        UpdateName();

        // Create stage connection pins
        int basePinId = nodeId * 1000 + 10000; // Offset to avoid conflicts

        // Output pin for connecting to next stage
        if (stage == VK_SHADER_STAGE_VERTEX_BIT || stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
            stageOutputPinId = basePinId++;
            Pin stageOut;
            stageOut.id = stageOutputPinId;
            stageOut.name = "Next Stage";
            stageOut.isInput = false;
            stageOut.type = PinType::ShaderStageOut;
            stageOut.stages = stage;
            outputs.push_back(stageOut);
        }

        // Input pin for receiving from previous stage
        if (stage == VK_SHADER_STAGE_FRAGMENT_BIT || stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
            stageInputPinId = basePinId++;
            Pin stageIn;
            stageIn.id = stageInputPinId;
            stageIn.name = "Prev Stage";
            stageIn.isInput = true;
            stageIn.type = PinType::ShaderStageIn;
            stageIn.stages = stage;
            inputs.push_back(stageIn);
        }
    }

    const char* GetTypeName() const override { return "Shader"; }

    void UpdateName() {
        switch(stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: name = "Vertex Shader"; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: name = "Fragment Shader"; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: name = "Compute Shader"; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: name = "Geometry Shader"; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: name = "Tess Control"; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: name = "Tess Eval"; break;
            default: name = "Shader"; break;
        }
    }

    bool LoadSourceCode(const std::string& path) {
        shaderPath = path;
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader source: " << path << std::endl;
            return false;
        }

        sourceCode = std::string((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
        file.close();
        return true;
    }

    bool LoadShader(const std::string& path) {
        shaderPath = path;

        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader: " << path << std::endl;
            return false;
        }

        size_t fileSize = file.tellg();
        spirvCode.resize(fileSize / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize);
        file.close();

        std::string spirvString(reinterpret_cast<const char*>(spirvCode.data()),
                               spirvCode.size() * sizeof(uint32_t));
        reflection = std::make_unique<ShaderReflection>(spirvString);

        PopulateFromReflection();

        loaded = true;
        compileError.clear();
        return true;
    }

    bool CompileShader() {
        shaderc_shader_kind kind;
        switch(stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: kind = shaderc_vertex_shader; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_fragment_shader; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: kind = shaderc_compute_shader; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: kind = shaderc_geometry_shader; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: kind = shaderc_tess_control_shader; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: kind = shaderc_tess_evaluation_shader; break;
            default:
                compileError = "Unknown shader stage";
                return false;
        }

        std::string error;
        spirvCode = compileGLSL(sourceCode, kind, &error);

        if (spirvCode.empty()) {
            compileError = error;
            loaded = false;
            return false;
        }

        std::string spirvString(reinterpret_cast<const char*>(spirvCode.data()),
                               spirvCode.size() * sizeof(uint32_t));
        reflection = std::make_unique<ShaderReflection>(spirvString);

        PopulateFromReflection();

        loaded = true;
        compileError.clear();
        return true;
    }

    void PopulateFromReflection() {
        if (!reflection) return;

        // Clear ALL pins except stage connection pins
        inputs.erase(std::remove_if(inputs.begin(), inputs.end(),
            [](const Pin& p) {
                return p.type != PinType::ShaderStageIn;
            }), inputs.end());

        outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
            [](const Pin& p) {
                return p.type != PinType::ShaderStageOut;
            }), outputs.end());

        // Reset pin ID counter to avoid ID conflicts after recompilation
        int pinId = id * 1000;

        // Skip past stage connection pin IDs if they exist
        if (stageInputPinId >= 0) pinId = std::max(pinId, stageInputPinId + 1);
        if (stageOutputPinId >= 0) pinId = std::max(pinId, stageOutputPinId + 1);

        // Add descriptor set bindings as input pins
        for (const auto& setLayout : reflection->getDescriptorSetLayouts()) {
            size_t nameidx = 0;
            for (const auto& binding : setLayout.bindings) {
                Pin input;
                input.id = pinId++;
                input.name = setLayout.names[nameidx++];
                input.isInput = true;
                input.set = setLayout.set;
                input.binding = binding.binding;
                input.stages = binding.stageFlags;

                switch(binding.descriptorType) {
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                        input.type = PinType::UniformBuffer;
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                        input.type = PinType::StorageBuffer;
                        break;
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                        input.type = PinType::CombinedImageSampler;
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        input.type = PinType::StorageImage;
                        break;
                    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                        input.type = PinType::InputAttachment;
                        break;
                    default:
                        continue;
                }

                inputs.push_back(input);
            }
        }

        // Add push constants
        for (const auto& pcRange : reflection->getPushConstantRanges()) {
            Pin input;
            input.id = pinId++;
            input.name = "PushConstants";
            input.isInput = true;
            input.type = PinType::PushConstant;
            input.size = pcRange.size;
            input.stages = pcRange.stageFlags;
            inputs.push_back(input);
        }

        // Add vertex inputs
        if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
            for (const auto& inputVar : reflection->getInputVariables()) {
                Pin input;
                input.id = pinId++;
                input.name = inputVar.name;
                input.isInput = true;
                input.type = PinType::VertexInput;
                input.location = inputVar.location;
                input.format = inputVar.format;
                inputs.push_back(input);
            }
        }

        // Add fragment outputs
        if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            for (const auto& outputVar : reflection->getOutputVariables()) {
                Pin output;
                output.id = pinId++;
                output.name = outputVar.name;
                output.isInput = false;
                output.type = PinType::FragmentOutput;
                output.location = outputVar.location;
                output.format = outputVar.format;
                outputs.push_back(output);
            }
        }
    }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        const char* icon = stage == VK_SHADER_STAGE_VERTEX_BIT ? "V" :
                          stage == VK_SHADER_STAGE_FRAGMENT_BIT ? "F" :
                          stage == VK_SHADER_STAGE_COMPUTE_BIT ? "C" : "G";
        ImGui::Text("%s %s", icon, name.c_str());
        ImNodes::EndNodeTitleBar();

        if (!shaderPath.empty()) {
            size_t lastSlash = shaderPath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ?
                shaderPath.substr(lastSlash + 1) : shaderPath;
            ImGui::TextDisabled("%s", filename.c_str());
        }

        if (!loaded) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "- Not compiled");
        } else if (!compileError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "X Compile error");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "+ Ready");
        }

        bool hascontent = false;

        // Draw stage input pin first
        if (stageInputPinId >= 0) {
            hascontent = true;
            ImNodes::BeginInputAttribute(stageInputPinId);
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "← Previous Stage");
            ImNodes::EndInputAttribute();
            //ImGui::Separator();
        }

        // Group inputs by type
        std::vector<Pin*> descriptorInputs;
        std::vector<Pin*> pushConstantInputs;
        std::vector<Pin*> vertexInputs;

        for (auto& input : inputs) {
            if (input.type == PinType::ShaderStageIn) continue; // Already drawn
            if (input.type == PinType::PushConstant)
                pushConstantInputs.push_back(&input);
            else if (input.type == PinType::VertexInput)
                vertexInputs.push_back(&input);
            else
                descriptorInputs.push_back(&input);
        }

        // Draw descriptor inputs
        if (!descriptorInputs.empty()) {
            hascontent = true;
            ImGui::TextDisabled("Resources:");
            for (auto* input : descriptorInputs) {
                ImNodes::BeginInputAttribute(input->id);
                ImGui::Text("← [%d:%d] %s", input->set, input->binding, input->name.c_str());
                ImNodes::EndInputAttribute();
            }
        }

        // Draw push constants
        if (!pushConstantInputs.empty()) {
            hascontent = true;
            if (!descriptorInputs.empty()) ImGui::Spacing();
            ImGui::TextDisabled("Push Constants:");
            for (auto* input : pushConstantInputs) {
                ImGui::Text(" %zu bytes", input->size);
            }
        }

        // Draw vertex inputs
        if (!vertexInputs.empty()) {
            hascontent = true;
            if (!descriptorInputs.empty() || !pushConstantInputs.empty()) ImGui::Spacing();
            ImGui::TextDisabled("Vertex Inputs:");
            for (auto* input : vertexInputs) {
                ImGui::Text(" [%d] %s", input->location, input->name.c_str());
            }
        }

        // Draw outputs
        bool hasFragmentOutputs = false;
        for (auto& output : outputs) {
            if (output.type == PinType::FragmentOutput) {
                if (!hasFragmentOutputs) {
                    hascontent = true;
                    if (!descriptorInputs.empty() || !pushConstantInputs.empty() || !vertexInputs.empty())
                        ImGui::Separator();
                    ImGui::TextDisabled("Outputs:");
                    hasFragmentOutputs = true;
                }
                ImNodes::BeginOutputAttribute(output.id);
                ImGui::Text("[%d] %s →", output.location, output.name.c_str());
                ImNodes::EndOutputAttribute();
            }
        }

        // Draw stage output pin last
        if (stageOutputPinId >= 0) {
            hascontent = true;
            if (!descriptorInputs.empty() || !pushConstantInputs.empty() || !vertexInputs.empty() || hasFragmentOutputs)
                ImGui::Separator();
            ImNodes::BeginOutputAttribute(stageOutputPinId);
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Next Stage →");
            ImNodes::EndOutputAttribute();
        }

        if (!hascontent) {
            ImGui::Text("- No reflection data");
        }

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Shader Properties");
        ImGui::Separator();

        const char* stages[] = {"Vertex", "Fragment", "Compute", "Geometry", "Tess Control", "Tess Eval"};
        const VkShaderStageFlagBits stageMap[] = {
            VK_SHADER_STAGE_VERTEX_BIT,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_GEOMETRY_BIT,
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
        };

        int currentStage = 0;
        for (int i = 0; i < IM_ARRAYSIZE(stageMap); i++) {
            if (stageMap[i] == stage) { currentStage = i; break; }
        }

        if (ImGui::Combo("Stage", &currentStage, stages, IM_ARRAYSIZE(stages))) {
            stage = stageMap[currentStage];
            UpdateName();
        }

        if (!sourceCode.empty()) {
            if (ImGui::Button("^-^ Compile Shader", ImVec2(-1, 0))) {
                if (CompileShader()) {
                    ImGui::OpenPopup("Compile Success");
                } else {
                    ImGui::OpenPopup("Compile Error");
                }
            }

            if (ImGui::BeginPopup("Compile Success")) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "+ Compilation successful!");
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("Compile Error")) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "X Compilation failed:");
                ImGui::Separator();
                ImGui::TextWrapped("%s", compileError.c_str());
                ImGui::EndPopup();
            }
        }

        if (!compileError.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Compile Error:");
            ImGui::TextWrapped("%s", compileError.c_str());
        }

        if (reflection) {
            ImGui::Separator();
            ImGui::Text("Reflection Info:");
            ImGui::BulletText("Descriptor Sets: %zu", reflection->getDescriptorSetLayouts().size());
            ImGui::BulletText("Push Constants: %zu", reflection->getPushConstantRanges().size());
            ImGui::BulletText("Inputs: %zu", reflection->getInputVariables().size());
            ImGui::BulletText("Outputs: %zu", reflection->getOutputVariables().size());
        }
    }
};

// Main node editor
class VulkanNodeEditor {
private:
    std::unordered_map<int, std::unique_ptr<GraphNode>> nodes;
    std::vector<Link> links;
    int nextNodeId = 1;
    int nextLinkId = 1;
    int selectedNodeId = -1;

    ImNodesEditorContext* editorContext = nullptr;

    VkDevice device;
    MemoryAllocator memoryAllocator;
    ResourceManager resourceManager;
    VkDescriptorPool descriptorPool;

    TextEditor* textEditor = nullptr;
    int editingNodeId = -1;

    // NEW: Pipeline building state
    std::vector<PipelineBuilder> detectedPipelines;

    DescriptorLayoutCache descriptorLayoutCache;
    PipelineLayoutCache pipelineLayoutCache;

    // FIXME: Temporary
    std::unique_ptr<RenderPass> renderPass;
    VkQueue graphicsQueue;

    CommandPool commandPool;
    std::vector<CommandBuffer> commandBuffers;

    // FIXME: Debug texture
    Texture texture;
    VkDescriptorSet meowTargetSet;
    StagingBufferManager stagingBufferManager;

public:
    VulkanNodeEditor(VulkanInstance& instance, Device& device)
        : memoryAllocator(instance, device), resourceManager(device, memoryAllocator), descriptorLayoutCache(device), pipelineLayoutCache(device, descriptorLayoutCache), device(device), commandPool(device, device.getGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT), graphicsQueue(device.getGraphicsQueue()), stagingBufferManager(device, 64 * 1024 * 1024) {
        editorContext = ImNodes::EditorContextCreate();
        ImNodes::EditorContextSet(editorContext);
        ImNodes::StyleColorsDark();
        ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(30, 30, 40, 255));
        ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered, IM_COL32(40, 40, 50, 255));
        ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected, IM_COL32(50, 50, 70, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(140, 40, 40, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(160, 60, 60, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(180, 80, 80, 255));
        ImNodes::PushColorStyle(ImNodesCol_Link, IM_COL32(200, 200, 200, 255));
        ImNodes::PushColorStyle(ImNodesCol_LinkHovered, IM_COL32(255, 255, 255, 255));
        ImNodes::PushColorStyle(ImNodesCol_LinkSelected, IM_COL32(255, 200, 100, 255));
        ImNodes::PushColorStyle(ImNodesCol_Pin, IM_COL32(100, 100, 150, 255));
        ImNodes::PushColorStyle(ImNodesCol_PinHovered, IM_COL32(150, 150, 200, 255));

        std::vector<std::pair<VkDescriptorType, float>> poolSizes = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}
        };

        size_t count = 1024;
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve(poolSizes.size());
        for (auto sz: poolSizes) {
            sizes.push_back({sz.first, static_cast<uint32_t>(sz.second * count)});
        }
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = count;
        pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
        pool_info.pPoolSizes = sizes.data();

        vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);

        // FIXME: WHATTT??!?!?!??!
        commandBuffers = commandPool.allocateCommandBuffers(1);

        // FIXME: Temporary
        texture = Texture::loadImage("/home/honeywrap/Documents/kitten/assets/vokselia_spawn/vokselia_spawn.png");
        texture.pushTexture(resourceManager, stagingBufferManager);

        meowTargetSet = ImGui_ImplVulkan_AddTexture(texture.sampler->getSampler(), texture.image->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        debugTexture = meowTargetSet;

        renderPass = std::make_unique<RenderPass>(device);
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentReference;

        VkSubpassDependency subpassDependency{};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        renderPass->create({colorAttachment}, {subpass}, {subpassDependency});
    }

    ~VulkanNodeEditor() {
        ImNodes::EditorContextFree(editorContext);
        links.clear();
        nodes.clear();
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }

    void AddImageNode() {
        auto node = std::make_unique<ImageResourceNode>(nextNodeId++, &resourceManager, descriptorPool, device);
        nodes[node->id] = std::move(node);
    }

    void AddBufferNode(bool uniform = true) {
        auto node = std::make_unique<BufferResourceNode>(nextNodeId++, &resourceManager, uniform);
        nodes[node->id] = std::move(node);
    }

    void AddShaderNode(VkShaderStageFlagBits stage = VK_SHADER_STAGE_FRAGMENT_BIT) {
        auto node = std::make_unique<ShaderGraphNode>(nextNodeId++, stage);
        nodes[node->id] = std::move(node);
    }

    void AddRenderTargetNode() {
        auto node = std::make_unique<RenderTargetNode>(nextNodeId++, &resourceManager, descriptorPool, device, &renderPass);
        nodes[node->id] = std::move(node);
    }

    const std::unordered_map<int, std::unique_ptr<GraphNode>>& GetNodes() const { return nodes; }
    const std::vector<Link>& GetLinks() const { return links; }

    Pin* FindPin(int pinId) const {
        for (auto& [nodeId, node] : nodes) {
            for (auto& pin : node->inputs) {
                if (pin.id == pinId) return const_cast<Pin*>(&pin);
            }
            for (auto& pin : node->outputs) {
                if (pin.id == pinId) return const_cast<Pin*>(&pin);
            }
        }
        return nullptr;
    }

    const Link* FindLinkToPin(int pinId) const {
        for (const auto& link : links) {
            if (link.endPinId == pinId) return &link;
        }
        return nullptr;
    }

    GraphNode* FindNodeByPin(int pinId) const {
        for (auto& [nodeId, node] : nodes) {
            for (auto& pin : node->inputs) {
                if (pin.id == pinId) return node.get();
            }
            for (auto& pin : node->outputs) {
                if (pin.id == pinId) return node.get();
            }
        }
        return nullptr;
    }

    // NEW: Analyze graph and detect valid pipelines
    void AnalyzePipelines() {
        std::cout << "Analyzing!!" << std::endl;
        detectedPipelines.clear();

        // Find all render target nodes
        std::vector<RenderTargetNode*> renderTargets;
        for (auto& [id, node] : nodes) {
            if (node->nodeType == NodeType::RenderTarget) {
                std::cout << "Render target found " << node->name << std::endl;
                renderTargets.push_back(dynamic_cast<RenderTargetNode*>(node.get()));
            }
        }

        // For each render target, trace back to find the pipeline
        for (auto* rt : renderTargets) {
            PipelineBuilder builder;
            builder.renderTarget = rt;

            // Find fragment shader connected to render target
            for (const auto& link : links) {
                Pin* startPin = FindPin(link.startPinId);
                Pin* endPin = FindPin(link.endPinId);
                if (!startPin || !endPin) continue;
                if (endPin->type == PinType::FragmentOutput) {
                    GraphNode* node = FindNodeByPin(link.endPinId);
                    if (node && node->id == rt->id) {
                        GraphNode* faggot = FindNodeByPin(link.startPinId);
                        if (faggot->nodeType == NodeType::ShaderGraph) {
                            auto* fragShader = dynamic_cast<ShaderGraphNode*>(faggot);
                            if (fragShader->stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
                                std::cout << "Fragment shader found" << std::endl;
                                builder.fragmentShader = fragShader;
                                break;
                            }
                        }
                    }
                }
            }

            if (!builder.fragmentShader) {
                std::cout << "Fragment shader not found" << std::endl;
                continue;
            }

            // Find vertex shader connected to fragment shader
            if (builder.fragmentShader->stageInputPinId >= 0) {
                for (const auto& link : links) {
                    if (link.endPinId == builder.fragmentShader->stageInputPinId) {
                        Pin* startPin = FindPin(link.startPinId);
                        if (startPin && startPin->type == PinType::ShaderStageOut) {
                            GraphNode* node = FindNodeByPin(link.startPinId);
                            if (node->nodeType == NodeType::ShaderGraph) {
                                auto* vertShader = dynamic_cast<ShaderGraphNode*>(node);
                                if (vertShader->stage == VK_SHADER_STAGE_VERTEX_BIT) {
                                    builder.vertexShader = vertShader;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if (builder.IsValid()) {
                // Collect all resources connected to shaders
                std::vector<ShaderGraphNode*> shaders = {builder.vertexShader, builder.fragmentShader};
                for (auto* shader : shaders) {
                    for (const auto& input : shader->inputs) {
                        if (input.type != PinType::ShaderStageIn &&
                            input.type != PinType::PushConstant &&
                            input.type != PinType::VertexInput) {
                            // Find what's connected to this input
                            for (const auto& link : links) {
                                if (link.endPinId == input.id) {
                                    builder.descriptorBindings[input.id] = {input.set, input.binding};
                                    break;
                                }
                            }
                        }
                    }
                }

                detectedPipelines.push_back(builder);
            } else {
                std::cout << "Invalid pipeline!!" << std::endl;
            }
        }
    }

    // NEW: Build Vulkan pipeline from detected shader chain
    bool BuildPipeline(PipelineBuilder& builder) {
        if (!builder.IsValid()) {
            std::cerr << "Invalid pipeline builder" << std::endl;
            return false;
        }

        if (!builder.vertexShader->loaded || !builder.fragmentShader->loaded) {
            std::cerr << "Shaders not compiled" << std::endl;
            return false;
        }

        std::cout << "Building pipeline:" << std::endl;
        std::cout << "  Vertex Shader: " << builder.vertexShader->name << std::endl;
        std::cout << "  Fragment Shader: " << builder.fragmentShader->name << std::endl;
        std::cout << "  Render Target: " << builder.renderTarget->extent.width << "x"
                  << builder.renderTarget->extent.height << std::endl;
        std::cout << "  Descriptor Bindings: " << builder.descriptorBindings.size() << std::endl;


        auto vertCode = builder.vertexShader->spirvCode;
        auto fragCode = builder.fragmentShader->spirvCode;
        ShaderModule vertModule(device, vertCode), fragModule(device, fragCode);
        ShaderReflection vertexShader(vertCode), fragmentShader(fragCode);
        VkPipelineLayout pipelineLayout = pipelineLayoutCache.createPipelineLayout(vertexShader+fragmentShader);

        builder.viewport.width = static_cast<float>(builder.renderTarget->extent.width);
        builder.viewport.height = static_cast<float>(builder.renderTarget->extent.height);
        builder.viewport.minDepth = 0.0f;
        builder.viewport.maxDepth = 1.0f;

        builder.scissor.extent = {static_cast<uint32_t>(builder.renderTarget->extent.width), static_cast<uint32_t>(builder.renderTarget->extent.height)};

        // TODO: Ability to run arbitrary vector buffers through pipelines
        // TODO: Figure out how to customize render passes!!
        builder.pipeline = GraphicsPipelineBuilder()
        .setShaders(vertModule, fragModule)
        .setViewportState(builder.viewport, builder.scissor)
        .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 1.0f)
        .setColorBlendState({alphaBlend})
        .setDepthStencilState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS)
        .setLayout(pipelineLayout)
        .setRenderPass(*renderPass.get(), 0)
        .setDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
        .build(device);

        // 1. Creating VkShaderModules from spirvCode
        // 2. Setting up descriptor set layouts from collected bindings
        // 3. Creating pipeline layout
        // 4. Setting up vertex input state
        // 5. Creating render pass for the render target
        // 6. Creating graphics pipeline
        // 7. Allocating and updating descriptor sets

        return true;
    }

    void SetTextEditor(TextEditor* editor) {
        textEditor = editor;
    }

    void EditShaderNode(int nodeId) {
        if (!nodes.count(nodeId)) return;

        auto* shaderNode = dynamic_cast<ShaderGraphNode*>(nodes[nodeId].get());
        if (!shaderNode) return;

        if (textEditor) {
            if (editingNodeId >= 0 && nodes.count(editingNodeId)) {
                auto* prevNode = dynamic_cast<ShaderGraphNode*>(nodes[editingNodeId].get());
                if (prevNode) {
                    prevNode->sourceCode = textEditor->GetText();
                }
            }

            textEditor->SetText(shaderNode->sourceCode);
            auto lang = TextEditor::LanguageDefinition::GLSL();
            textEditor->SetLanguageDefinition(lang);
            editingNodeId = nodeId;
        }
    }

    void SaveCurrentShaderEdit() {
        if (editingNodeId >= 0 && nodes.count(editingNodeId) && textEditor) {
            auto* shaderNode = dynamic_cast<ShaderGraphNode*>(nodes[editingNodeId].get());
            if (shaderNode) {
                shaderNode->sourceCode = textEditor->GetText();
            }
        }
    }

    void Draw(CommandBuffer& commandBuffer) {
        //auto& commandBuffer = commandBuffers[0];

        std::vector<RenderTargetNode*> renderTargets;
        for (auto& [id, node] : nodes) {
            if (node->nodeType == NodeType::RenderTarget) {
                //std::cout << "Render target found " << node->name << std::endl;
                renderTargets.push_back(dynamic_cast<RenderTargetNode*>(node.get()));
            }
        }

        for (auto renderTarget: renderTargets) {
            ResourceBarrier::transitionImageLayout(commandBuffer, renderTarget->targetImage->getImage(), renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        for (auto& pipeline: detectedPipelines) {
            if (pipeline.pipeline != VK_NULL_HANDLE) {
                ResourceBarrier::transitionImageLayout(commandBuffer, pipeline.renderTarget->targetImage->getImage(), pipeline.renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                renderPass->begin(commandBuffer, *pipeline.renderTarget->framebuffer, {.extent = {static_cast<uint32_t>(pipeline.renderTarget->extent.width), static_cast<uint32_t>(pipeline.renderTarget->extent.height)}}, {{.color = {0.0f, 0.5f, 1.0f, 1.0f}}, {.depthStencil = {1.0f, 0}}});

                vkCmdSetViewport(commandBuffer, 0, 1, &pipeline.viewport);
                vkCmdSetScissor(commandBuffer, 0, 1, &pipeline.scissor);

                //glm::vec4 data = {viewportSize.x, viewportSize.y, time, deltaTime};
                commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
                //commandBuffer.pushConstants(pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(data), &data);
                commandBuffer.draw(3);

                renderPass->end(commandBuffer);

                ResourceBarrier::transitionImageLayout(commandBuffer, pipeline.renderTarget->targetImage->getImage(), pipeline.renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        ImGui::Begin("Shader Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Add")) {
                if (ImGui::MenuItem("RT Render Target")) AddRenderTargetNode();
                ImGui::Separator();
                if (ImGui::MenuItem("V Vertex Shader")) AddShaderNode(VK_SHADER_STAGE_VERTEX_BIT);
                if (ImGui::MenuItem("F Fragment Shader")) AddShaderNode(VK_SHADER_STAGE_FRAGMENT_BIT);
                if (ImGui::MenuItem("C Compute Shader")) AddShaderNode(VK_SHADER_STAGE_COMPUTE_BIT);
                if (ImGui::MenuItem("G Geometry Shader")) AddShaderNode(VK_SHADER_STAGE_GEOMETRY_BIT);
                ImGui::Separator();
                if (ImGui::MenuItem("I Image Resource")) AddImageNode();
                if (ImGui::MenuItem("UB Uniform Buffer")) AddBufferNode(true);
                if (ImGui::MenuItem("SB Storage Buffer")) AddBufferNode(false);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Pipeline")) {
                if (ImGui::MenuItem("OwO Analyze Graph")) {
                    AnalyzePipelines();
                }
                ImGui::Separator();
                ImGui::TextDisabled("Detected Pipelines: %zu", detectedPipelines.size());
                for (size_t i = 0; i < detectedPipelines.size(); i++) {
                    if (ImGui::MenuItem(("Build Pipeline " + std::to_string(i)).c_str())) {
                        BuildPipeline(detectedPipelines[i]);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImNodes::EditorContextSet(editorContext);
        ImNodes::BeginNodeEditor();

        for (auto& [nodeId, node] : nodes) {
            node->Draw();
        }

        for (const auto& link : links) {
            ImNodes::Link(link.id, link.startPinId, link.endPinId);
        }

        ImNodes::EndNodeEditor();

        // Handle new links
        int startPin, endPin;
        if (ImNodes::IsLinkCreated(&startPin, &endPin)) {
            Pin* start = FindPin(startPin);
            Pin* end = FindPin(endPin);

            bool canConnect = false;

            if (start && end && !start->isInput && end->isInput) {
                // Allow stage connections
                if (start->type == PinType::ShaderStageOut && end->type == PinType::ShaderStageIn) {
                    canConnect = true;
                }
                // Allow fragment output to render target
                else if (start->type == PinType::FragmentOutput && end->type == PinType::FragmentOutput) {
                    canConnect = true;
                }
                // Allow resource connections
                else if (start->type == end->type) {
                    canConnect = true;
                }
            }

            if (canConnect) {
                Link newLink;
                newLink.id = nextLinkId++;
                newLink.startPinId = startPin;
                newLink.endPinId = endPin;
                links.push_back(newLink);
            }
        }

        int linkId;
        if (ImNodes::IsLinkDestroyed(&linkId)) {
            links.erase(std::remove_if(links.begin(), links.end(),
                [linkId](const Link& link) { return link.id == linkId; }),
                links.end());
        }

        const int numSelected = ImNodes::NumSelectedNodes();
        if (numSelected > 0) {
            std::vector<int> selectedNodes(numSelected);
            ImNodes::GetSelectedNodes(selectedNodes.data());
            selectedNodeId = selectedNodes[0];
        } else {
            selectedNodeId = -1;
        }

        // Detect double-click on shader nodes
        int clickedNodeId = -1;
        if (ImNodes::IsNodeHovered(&clickedNodeId) && ImGui::IsMouseDoubleClicked(0)) {
            if (nodes.count(clickedNodeId)) {
                auto* shaderNode = dynamic_cast<ShaderGraphNode*>(nodes[clickedNodeId].get());
                if (shaderNode) {
                    EditShaderNode(clickedNodeId);
                }
            }
        }

        // Handle node deletion
        if (numSelected > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            std::vector<int> selectedNodes(numSelected);
            ImNodes::GetSelectedNodes(selectedNodes.data());

            for (int nodeId : selectedNodes) {
                if (nodeId == editingNodeId) {
                    editingNodeId = -1;
                    if (textEditor) {
                        textEditor->SetText("");
                    }
                }

                links.erase(std::remove_if(links.begin(), links.end(),
                    [this, nodeId](const Link& link) {
                        Pin* start = FindPin(link.startPinId);
                        Pin* end = FindPin(link.endPinId);
                        auto startNode = std::find_if(nodes.begin(), nodes.end(),
                            [start](const auto& pair) {
                                for (auto& p : pair.second->outputs)
                                    if (&p == start) return true;
                                return false;
                            });
                        auto endNode = std::find_if(nodes.begin(), nodes.end(),
                            [end](const auto& pair) {
                                for (auto& p : pair.second->inputs)
                                    if (&p == end) return true;
                                return false;
                            });
                        return (startNode != nodes.end() && startNode->first == nodeId) ||
                               (endNode != nodes.end() && endNode->first == nodeId);
                    }), links.end());

                nodes.erase(nodeId);
            }

            ImNodes::ClearNodeSelection();
            selectedNodeId = -1;
        }

        ImGui::End();

        // Properties panel
        ImGui::Begin("Node Properties");
        if (selectedNodeId >= 0 && nodes.count(selectedNodeId)) {
            nodes[selectedNodeId]->DrawProperties();
        } else {
            ImGui::TextDisabled("No node selected");
            ImGui::Separator();
            //ImGui::BulletText("Select a node to edit properties");
            //ImGui::BulletText("Double-click shader nodes to edit source");
            //ImGui::BulletText("Connect shaders: Vertex → Fragment → Render Target");
            //ImGui::BulletText("Use 'Pipeline → Analyze Graph' to detect pipelines");
        }
        ImGui::End();

        // Pipeline info panel
        ImGui::Begin("Pipeline Info");
        if (detectedPipelines.empty()) {
            ImGui::TextDisabled("No pipelines detected");
            ImGui::Separator();
            ImGui::TextWrapped("Build a complete pipeline:");
            ImGui::BulletText("Add a Vertex Shader");
            ImGui::BulletText("Add a Fragment Shader");
            ImGui::BulletText("Add a Render Target");
            ImGui::BulletText("Connect: Vertex → Fragment → Render Target");
            ImGui::BulletText("Use 'Pipeline → Analyze Graph'");
        } else {
            ImGui::Text("O_O Detected %zu pipeline(s)", detectedPipelines.size());
            ImGui::Separator();

            for (size_t i = 0; i < detectedPipelines.size(); i++) {
                    auto& pipeline = detectedPipelines[i];
                if (ImGui::CollapsingHeader(("Pipeline " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Indent();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "+ Complete Pipeline");
                    ImGui::BulletText("Vertex: %s", pipeline.vertexShader->name.c_str());
                    ImGui::BulletText("Fragment: %s", pipeline.fragmentShader->name.c_str());
                    ImGui::BulletText("Target: %dx%d",
                        pipeline.renderTarget->extent.width,
                        pipeline.renderTarget->extent.height);
                    ImGui::BulletText("Resources: %zu", pipeline.descriptorBindings.size());

                    bool shadersReady = pipeline.vertexShader->loaded && pipeline.fragmentShader->loaded;

                    if (!shadersReady) {
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "- Compile shaders first!");
                    }

                    ImGui::Spacing();
                    if (ImGui::Button(("UwU Build Pipeline " + std::to_string(i)).c_str(), ImVec2(-1, 0))) {
                        if (shadersReady) {
                            BuildPipeline(pipeline);
                        }
                    }

                    if (!shadersReady) {
                        ImGui::SetItemTooltip("Compile all shaders before building the pipeline");
                    }

                    ImGui::Unindent();
                }
            }
        }
        ImGui::End();
    }
};
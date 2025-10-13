// VulkanNodeEditor.h
#pragma once
#include <imgui.h>
#include <imnodes.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>

// Your engine includes
#include <shader/ShaderReflection.h>

// Pin types based on Vulkan descriptor types
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

// Pin represents a shader binding or resource output
struct Pin {
    int id;
    std::string name;
    bool isInput;
    PinType type;

    // For descriptor bindings
    uint32_t set = 0;
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;

    // For buffers
    size_t size = 0;

    // For images
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent = {0, 0, 0};

    // For vertex inputs/fragment outputs
    uint32_t location = 0;
};

// Link between two pins
struct Link {
    int id;
    int startPinId;
    int endPinId;
};

// Base node class
class GraphNode {
public:
    int id;
    ImVec2 position;
    std::string name;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;

    GraphNode(int nodeId, const std::string& nodeName)
        : id(nodeId), name(nodeName) {}

    virtual ~GraphNode() = default;
    virtual void Draw() = 0;
    virtual void DrawProperties() {}
    virtual const char* GetTypeName() const = 0;
};

// Image resource node
class ImageResourceNode : public GraphNode {
private:
    ResourceManager* resourceManager;
public:
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;

    std::shared_ptr<Image> image;
    std::shared_ptr<Sampler> meowSampler;

    VkDescriptorSet meowTargetSet;

    ImageResourceNode(int nodeId, ResourceManager* resourceManager)
        : GraphNode(nodeId, "Image"), resourceManager(resourceManager),
          format(VK_FORMAT_R8G8B8A8_SRGB),
          extent{1024, 1024, 1},
          mipLevels(1),
          arrayLayers(1) {
        auto meowSampler = resourceManager->createSampler({}, "ImageSampler"+std::to_string(nodeId));

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
              .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        image->createImageView({.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = image->getFormat(), .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}});

        meowTargetSet = ImGui_ImplVulkan_AddTexture(meowSampler->getSampler(), image->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    ~ImageResourceNode() {
        ImGui_ImplVulkan_RemoveTexture(meowTargetSet);
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
            image = resourceManager->createImage({
                  .imageType = VK_IMAGE_TYPE_2D,
                  .format = format,
                  .extent = extent,
                  .mipLevels = mipLevels,
                  .arrayLayers = arrayLayers,
                  .samples = VK_SAMPLE_COUNT_1_BIT,
                  .tiling = VK_IMAGE_TILING_OPTIMAL,
                  .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
                  .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });

            image->createImageView({.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = image->getFormat(), .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}});

            ImGui_ImplVulkan_RemoveTexture(meowTargetSet);
            meowTargetSet = ImGui_ImplVulkan_AddTexture(meowSampler->getSampler(), image->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }
};

// Buffer resource node
class BufferResourceNode : public GraphNode {
public:
    size_t size;
    bool isUniform;

    BufferResourceNode(int nodeId, bool uniform = true)
        : GraphNode(nodeId, uniform ? "Uniform Buffer" : "Storage Buffer"),
          size(256),
          isUniform(uniform) {

        Pin output;
        output.id = nodeId * 1000;
        output.name = "Buffer";
        output.isInput = false;
        output.type = uniform ? PinType::UniformBuffer : PinType::StorageBuffer;
        output.size = size;
        outputs.push_back(output);
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

        int sizeKB = size / 1024;
        if (ImGui::InputInt("Size (KB)", &sizeKB)) {
            size = std::max(1, sizeKB) * 1024;
            outputs[0].size = size;
        }

        ImGui::Text("Size in bytes: %zu", size);
    }
};

// Shader node with reflection support
class ShaderGraphNode : public GraphNode {
public:
    std::string shaderPath;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirvCode;
    std::unique_ptr<ShaderReflection> reflection;
    bool loaded = false;

    ShaderGraphNode(int nodeId, VkShaderStageFlagBits shaderStage)
        : GraphNode(nodeId, "Shader"), stage(shaderStage) {
        UpdateName();
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

    bool LoadShader(const std::string& path) {
        shaderPath = path;

        // Read SPIR-V file
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

        // Create reflection
        std::string spirvString(reinterpret_cast<const char*>(spirvCode.data()),
                               spirvCode.size() * sizeof(uint32_t));
        reflection = std::make_unique<ShaderReflection>(spirvString);

        // Populate pins from reflection
        PopulateFromReflection();

        loaded = true;
        return true;
    }

    void PopulateFromReflection() {
        if (!reflection) return;

        inputs.clear();
        outputs.clear();

        int pinId = id * 1000;

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

                // Map descriptor type to pin type
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

        // Add push constants (displayed but not connectable)
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

        // Add vertex inputs (for vertex shaders)
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

        // Add fragment outputs (for fragment shaders)
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
        //if (!loaded) return;
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        const char* icon = stage == VK_SHADER_STAGE_VERTEX_BIT ? "V" :
                          stage == VK_SHADER_STAGE_FRAGMENT_BIT ? "F" :
                          stage == VK_SHADER_STAGE_COMPUTE_BIT ? "C" : "U";
        ImGui::Text("%s %s", icon, name.c_str());
        ImNodes::EndNodeTitleBar();

        if (!shaderPath.empty()) {
            // Get just the filename
            size_t lastSlash = shaderPath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ?
                shaderPath.substr(lastSlash + 1) : shaderPath;
            ImGui::TextDisabled("%s", filename.c_str());
        }

        if (!loaded) {
            ImGui::Text("undefined");
        }

        // Group inputs by type
        std::vector<Pin*> descriptorInputs;
        std::vector<Pin*> pushConstantInputs;
        std::vector<Pin*> vertexInputs;

        for (auto& input : inputs) {
            if (input.type == PinType::PushConstant)
                pushConstantInputs.push_back(&input);
            else if (input.type == PinType::VertexInput)
                vertexInputs.push_back(&input);
            else
                descriptorInputs.push_back(&input);
        }

        // Draw descriptor inputs
        if (!descriptorInputs.empty()) {
            ImGui::TextDisabled("Descriptors:");
            for (auto* input : descriptorInputs) {
                ImNodes::BeginInputAttribute(input->id);
                ImGui::Text("← [%d:%d] %s", input->set, input->binding, input->name.c_str());
                ImNodes::EndInputAttribute();
            }
        }

        // Draw push constants (not connectable)
        if (!pushConstantInputs.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Push Constants:");
            for (auto* input : pushConstantInputs) {
                ImGui::Text("  %zu bytes", input->size);
            }
        }

        // Draw vertex inputs (not connectable in graph)
        if (!vertexInputs.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Vertex Inputs:");
            for (auto* input : vertexInputs) {
                ImGui::Text("  [%d] %s", input->location, input->name.c_str());
            }
        }

        // Draw fragment outputs
        if (!outputs.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Outputs:");
            for (auto& output : outputs) {
                ImNodes::BeginOutputAttribute(output.id);
                ImGui::Text("[%d] %s →", output.location, output.name.c_str());
                ImNodes::EndOutputAttribute();
            }
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

        ImGui::InputText("Path", &shaderPath);

        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            if (LoadShader(shaderPath)) {
                ImGui::Text("✓ Loaded");
            } else {
                ImGui::Text("✗ Failed");
            }
        }

        if (reflection) {
            ImGui::Separator();
            ImGui::Text("Reflection Info:");
            ImGui::Text("  Descriptor Sets: %zu", reflection->getDescriptorSetLayouts().size());
            ImGui::Text("  Push Constants: %zu", reflection->getPushConstantRanges().size());
            ImGui::Text("  Inputs: %zu", reflection->getInputVariables().size());
            ImGui::Text("  Outputs: %zu", reflection->getOutputVariables().size());
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

    MemoryAllocator memoryAllocator;
    ResourceManager resourceManager;

public:
    VulkanNodeEditor(VulkanInstance& instance, Device& device) : memoryAllocator(instance, device), resourceManager(device, memoryAllocator) {
        editorContext = ImNodes::EditorContextCreate();
        ImNodes::EditorContextSet(editorContext);
        ImNodes::StyleColorsDark();

        // Vulkan-themed colors
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
    }

    ~VulkanNodeEditor() {
        ImNodes::EditorContextFree(editorContext);
    }

    void AddImageNode() {
        auto node = std::make_unique<ImageResourceNode>(nextNodeId++, &resourceManager);
        nodes[node->id] = std::move(node);
    }

    void AddBufferNode(bool uniform = true) {
        auto node = std::make_unique<BufferResourceNode>(nextNodeId++, uniform);
        nodes[node->id] = std::move(node);
    }

    void AddShaderNode(VkShaderStageFlagBits stage = VK_SHADER_STAGE_FRAGMENT_BIT) {
        auto node = std::make_unique<ShaderGraphNode>(nextNodeId++, stage);
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

    void Draw() {
        ImGui::Begin("Shader Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Add")) {
                if (ImGui::MenuItem("Image Resource")) AddImageNode();
                if (ImGui::MenuItem("Uniform Buffer")) AddBufferNode(true);
                if (ImGui::MenuItem("Storage Buffer")) AddBufferNode(false);
                ImGui::Separator();
                if (ImGui::MenuItem("Vertex Shader")) AddShaderNode(VK_SHADER_STAGE_VERTEX_BIT);
                if (ImGui::MenuItem("Fragment Shader")) AddShaderNode(VK_SHADER_STAGE_FRAGMENT_BIT);
                if (ImGui::MenuItem("Compute Shader")) AddShaderNode(VK_SHADER_STAGE_COMPUTE_BIT);
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

            // Validate: output -> input, matching types
            if (start && end && !start->isInput && end->isInput) {
                if (start->type == end->type) {
                    Link newLink;
                    newLink.id = nextLinkId++;
                    newLink.startPinId = startPin;
                    newLink.endPinId = endPin;
                    links.push_back(newLink);
                }
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

        if (numSelected > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            std::vector<int> selectedNodes(numSelected);
            ImNodes::GetSelectedNodes(selectedNodes.data());

            for (int nodeId : selectedNodes) {
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
            ImGui::TextWrapped("Select a node to edit its properties");
        }
        ImGui::End();
    }
};
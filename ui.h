// VulkanNodeEditor.h
#pragma once
#include <imgui.h>
#include <imnodes.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

// Forward declarations for your engine classes
class Device;
class ResourceManager;
class ShaderReflection;
class DescriptorLayoutCache;
class PipelineLayoutCache;

// Pin types based on Vulkan descriptor types
enum class PinType {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage,
    InputAttachment,
    PushConstant
};

// Pin represents a shader binding or resource output
struct Pin {
    int id;
    std::string name;
    bool isInput;
    PinType type;
    uint32_t binding;
    VkShaderStageFlags stages;

    // For buffers
    size_t size = 0;

    // For images
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent = {0, 0, 0};
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
};

// Image resource node - outputs an image that can be bound to samplers
class ImageResourceNode : public GraphNode {
public:
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkImageUsageFlags usage;

    ImageResourceNode(int nodeId)
        : GraphNode(nodeId, "Image Resource"),
          format(VK_FORMAT_R8G8B8A8_SRGB),
          extent{1024, 1024, 1},
          mipLevels(1),
          arrayLayers(1),
          usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {

        // Output pin
        Pin output;
        output.id = nodeId * 1000;
        output.name = "Image";
        output.isInput = false;
        output.type = PinType::CombinedImageSampler;
        output.format = format;
        output.extent = extent;
        outputs.push_back(output);
    }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndNodeTitleBar();

        ImGui::Text("Format: %d", format);
        ImGui::Text("Size: %dx%d", extent.width, extent.height);

        ImNodes::BeginOutputAttribute(outputs[0].id);
        ImGui::Text("→ %s", outputs[0].name.c_str());
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Image Resource Properties");
        ImGui::Separator();

        const char* formats[] = {
            "R8G8B8A8_SRGB", "R8G8B8A8_UNORM", "R16G16B16A16_SFLOAT",
            "R32G32B32A32_SFLOAT", "B8G8R8A8_SRGB"
        };
        int formatIdx = 0;
        if (ImGui::Combo("Format", &formatIdx, formats, IM_ARRAYSIZE(formats))) {
            const VkFormat formatMap[] = {
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_FORMAT_R8G8B8A8_UNORM,
                VK_FORMAT_R16G16B16A16_SFLOAT,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_FORMAT_B8G8R8A8_SRGB
            };
            format = formatMap[formatIdx];
            outputs[0].format = format;
        }

        int w = extent.width, h = extent.height;
        if (ImGui::InputInt("Width", &w)) extent.width = std::max(1, w);
        if (ImGui::InputInt("Height", &h)) extent.height = std::max(1, h);

        int mips = mipLevels;
        if (ImGui::InputInt("Mip Levels", &mips)) mipLevels = std::max(1, mips);
    }
};

// Buffer resource node
class BufferResourceNode : public GraphNode {
public:
    size_t size;
    VkBufferUsageFlags usage;
    bool isUniform;

    BufferResourceNode(int nodeId, bool uniform = true)
        : GraphNode(nodeId, uniform ? "Uniform Buffer" : "Storage Buffer"),
          size(256),
          usage(uniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
          isUniform(uniform) {

        Pin output;
        output.id = nodeId * 1000;
        output.name = "Buffer";
        output.isInput = false;
        output.type = uniform ? PinType::UniformBuffer : PinType::StorageBuffer;
        output.size = size;
        outputs.push_back(output);
    }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndNodeTitleBar();

        ImGui::Text("Size: %zu bytes", size);

        ImNodes::BeginOutputAttribute(outputs[0].id);
        ImGui::Text("→ %s", outputs[0].name.c_str());
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Buffer Properties");
        ImGui::Separator();

        int sizeKB = size / 1024;
        if (ImGui::InputInt("Size (KB)", &sizeKB)) {
            size = std::max(1, sizeKB) * 1024;
            outputs[0].size = size;
        }
    }
};

// Shader node - uses your ShaderReflection to populate inputs
class ShaderGraphNode : public GraphNode {
public:
    std::string vertexPath;
    std::string fragmentPath;
    std::string computePath;

    ShaderGraphNode(int nodeId)
        : GraphNode(nodeId, "Shader") {}

    // Populate inputs from shader reflection
    void SetupFromReflection(const ShaderReflection& reflection) {
        inputs.clear();

        // Get bindings from reflection
        // This assumes your ShaderReflection has methods to get descriptor bindings
        // You'll need to adapt this to match your actual ShaderReflection API

        int pinId = id * 1000;

        // Example: iterate through uniform buffers
        // for (auto& binding : reflection.getUniformBuffers()) {
        //     Pin input;
        //     input.id = pinId++;
        //     input.name = binding.name;
        //     input.isInput = true;
        //     input.type = PinType::UniformBuffer;
        //     input.binding = binding.binding;
        //     input.stages = binding.stages;
        //     inputs.push_back(input);
        // }

        // Placeholder pins for demonstration
        Pin uboInput;
        uboInput.id = pinId++;
        uboInput.name = "UBO";
        uboInput.isInput = true;
        uboInput.type = PinType::UniformBuffer;
        uboInput.binding = 0;
        inputs.push_back(uboInput);

        Pin samplerInput;
        samplerInput.id = pinId++;
        samplerInput.name = "Sampler";
        samplerInput.isInput = true;
        samplerInput.type = PinType::CombinedImageSampler;
        samplerInput.binding = 1;
        inputs.push_back(samplerInput);
    }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndNodeTitleBar();

        // Draw input pins
        for (auto& input : inputs) {
            ImNodes::BeginInputAttribute(input.id);
            const char* typeStr = input.type == PinType::UniformBuffer ? "UBO" :
                                 input.type == PinType::CombinedImageSampler ? "Tex" : "Buf";
            ImGui::Text("%s ← [%d]", input.name.c_str(), input.binding);
            ImNodes::EndInputAttribute();
        }

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Shader Properties");
        ImGui::Separator();

        static char vertPath[256] = "";
        static char fragPath[256] = "";

        ImGui::InputText("Vertex", vertPath, sizeof(vertPath));
        ImGui::InputText("Fragment", fragPath, sizeof(fragPath));

        if (ImGui::Button("Load Shader")) {
            vertexPath = vertPath;
            fragmentPath = fragPath;
            // TODO: Load and reflect shader
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

public:
    VulkanNodeEditor() {
        editorContext = ImNodes::EditorContextCreate();
        ImNodes::EditorContextSet(editorContext);
        ImNodes::StyleColorsDark();

        // Customize colors for Vulkan aesthetic
        ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(40, 40, 50, 255));
        ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundHovered, IM_COL32(50, 50, 60, 255));
        ImNodes::PushColorStyle(ImNodesCol_NodeBackgroundSelected, IM_COL32(60, 60, 80, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(100, 40, 120, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(120, 60, 140, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(140, 80, 160, 255));
    }

    ~VulkanNodeEditor() {
        ImNodes::EditorContextFree(editorContext);
    }

    void AddImageNode() {
        auto node = std::make_unique<ImageResourceNode>(nextNodeId++);
        nodes[node->id] = std::move(node);
    }

    void AddBufferNode(bool uniform = true) {
        auto node = std::make_unique<BufferResourceNode>(nextNodeId++, uniform);
        nodes[node->id] = std::move(node);
    }

    void AddShaderNode() {
        auto node = std::make_unique<ShaderGraphNode>(nextNodeId++);
        nodes[node->id] = std::move(node);
    }

    void Draw() {
        // Main window with node editor
        ImGui::Begin("Shader Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Add Node")) {
                if (ImGui::MenuItem("Image Resource")) AddImageNode();
                if (ImGui::MenuItem("Uniform Buffer")) AddBufferNode(true);
                if (ImGui::MenuItem("Storage Buffer")) AddBufferNode(false);
                if (ImGui::MenuItem("Shader")) AddShaderNode();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Graph")) SaveGraph();
                if (ImGui::MenuItem("Load Graph")) LoadGraph();
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImNodes::EditorContextSet(editorContext);
        ImNodes::BeginNodeEditor();

        // Draw all nodes
        for (auto& [nodeId, node] : nodes) {
            node->Draw();
        }

        // Draw all links
        for (const auto& link : links) {
            ImNodes::Link(link.id, link.startPinId, link.endPinId);
        }

        ImNodes::EndNodeEditor();

        // Handle new links
        int startPin, endPin;
        if (ImNodes::IsLinkCreated(&startPin, &endPin)) {
            // Validate connection types match
            Pin* start = FindPin(startPin);
            Pin* end = FindPin(endPin);

            if (start && end && start->type == end->type) {
                Link newLink;
                newLink.id = nextLinkId++;
                newLink.startPinId = startPin;
                newLink.endPinId = endPin;
                links.push_back(newLink);
            }
        }

        // Handle link deletion
        int linkId;
        if (ImNodes::IsLinkDestroyed(&linkId)) {
            auto it = std::find_if(links.begin(), links.end(),
                [linkId](const Link& link) { return link.id == linkId; });
            if (it != links.end()) {
                links.erase(it);
            }
        }

        // Handle node selection
        const int numSelected = ImNodes::NumSelectedNodes();
        if (numSelected > 0) {
            std::vector<int> selectedNodes(numSelected);
            ImNodes::GetSelectedNodes(selectedNodes.data());
            selectedNodeId = selectedNodes[0];
        } else {
            selectedNodeId = -1;
        }

        // Handle node deletion
        if (numSelected > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            std::vector<int> selectedNodes(numSelected);
            ImNodes::GetSelectedNodes(selectedNodes.data());

            for (int nodeId : selectedNodes) {
                // Remove associated links
                links.erase(
                    std::remove_if(links.begin(), links.end(),
                        [this, nodeId](const Link& link) {
                            Pin* start = FindPin(link.startPinId);
                            Pin* end = FindPin(link.endPinId);
                            return (start && FindNodeByPin(start) == nodeId) ||
                                   (end && FindNodeByPin(end) == nodeId);
                        }),
                    links.end());

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
        }
        ImGui::End();
    }

    Pin* FindPin(int pinId) {
        for (auto& [nodeId, node] : nodes) {
            for (auto& pin : node->inputs) {
                if (pin.id == pinId) return &pin;
            }
            for (auto& pin : node->outputs) {
                if (pin.id == pinId) return &pin;
            }
        }
        return nullptr;
    }

    int FindNodeByPin(Pin* pin) {
        for (auto& [nodeId, node] : nodes) {
            for (auto& p : node->inputs) {
                if (&p == pin) return nodeId;
            }
            for (auto& p : node->outputs) {
                if (&p == pin) return nodeId;
            }
        }
        return -1;
    }

    void SaveGraph() {
        // TODO: Serialize graph to JSON
        std::cout << "Saving graph..." << std::endl;
    }

    void LoadGraph() {
        // TODO: Deserialize graph from JSON
        std::cout << "Loading graph..." << std::endl;
    }
};
#pragma once
#include <alphyslab/node.h>
#include <alphyslab/ResourceNode.h>
#include <alphyslab/ShaderNode.h>
#include <alphyslab/FramebufferNode.h>

struct PipelineBuilder {
    ShaderNode* vertexShader = nullptr;
    ShaderNode* fragmentShader = nullptr;
    std::vector<ShaderNode*> shaderChain;
    FramebufferNode* renderTarget = nullptr;

    // Collected resources from all connected nodes
    std::unordered_map<int, std::pair<uint32_t, uint32_t>> descriptorBindings; // pinId -> (set, binding)
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<VkFormat> colorAttachmentFormats;

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkViewport viewport{};
    VkRect2D scissor{};

    bool IsValid() const;
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

    // Pipeline building state
    std::vector<PipelineBuilder> detectedPipelines;

    DescriptorLayoutCache descriptorLayoutCache;
    PipelineLayoutCache pipelineLayoutCache;

    // FIXME: Temporary
    std::unique_ptr<RenderPass> renderPass;
    VkQueue graphicsQueue;

    CommandPool commandPool;
    std::vector<CommandBuffer> commandBuffers;

    VkDescriptorSet focused_image = VK_NULL_HANDLE;

public:
    VulkanNodeEditor(VulkanInstance& instance, Device& device);
    ~VulkanNodeEditor();

    void AddImageNode();
    void AddBufferNode(bool uniform = true);
    void AddShaderNode(VkShaderStageFlagBits stage = VK_SHADER_STAGE_FRAGMENT_BIT);
    void AddRenderTargetNode();

    const std::unordered_map<int, std::unique_ptr<GraphNode>>& GetNodes() const { return nodes; }
    const std::vector<Link>& GetLinks() const { return links; }

    Pin* FindPin(int pinId) const;
    const Link* FindLinkToPin(int pinId) const;
    GraphNode* FindNodeByPin(int pinId) const;

    void AnalyzePipelines();
    bool BuildPipeline(PipelineBuilder& builder);

    void SetTextEditor(TextEditor* editor);
    void EditShaderNode(int nodeId);
    void SaveCurrentShaderEdit();

    void Draw(CommandBuffer& commandBuffer);
    VkDescriptorSet getFocusedImage();
};
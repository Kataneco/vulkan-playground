#pragma once
#include <alphyslab/node.h>
#include <alphyslab/ShaderNode.h>
#include <alphyslab/ResourceNode.h>
#include <alphyslab/FramebufferNode.h>

class PipelineNode : public GraphNode {
public:
    // Pipeline configuration
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    VkViewport viewport{};
    VkRect2D scissor{};

    // Pipeline state
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
    float lineWidth = 1.0f;
    bool depthTestEnable = false;
    bool depthWriteEnable = false;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

    // Connected resources (tracked via pins)
    struct ConnectedShader {
        ShaderNode* node = nullptr;
        int pinId = -1;
    };

    ConnectedShader vertexShader;
    ConnectedShader fragmentShader;
    ConnectedShader geometryShader;

    // Pin IDs for easy reference
    int vertexShaderInputPin = -1;
    int fragmentShaderInputPin = -1;
    int geometryShaderInputPin = -1;
    int renderTargetOutputPin = -1;

    // Descriptor bindings collected from shaders
    std::unordered_map<int, std::pair<uint32_t, uint32_t>> descriptorBindings; // pinId -> (set, binding)
    std::vector<int> descriptorInputPins;

    bool isBuilt = false;
    std::string buildError;

    VkDevice device;
    DescriptorLayoutCache* layoutCache = nullptr;
    PipelineLayoutCache* pipelineCache = nullptr;
    RenderPass* renderPass = nullptr;

public:
    PipelineNode(int nodeId, VkDevice dev, DescriptorLayoutCache* descLayoutCache,
                 PipelineLayoutCache* pipeLayoutCache, RenderPass* rp);

    ~PipelineNode() override;

    void Draw() override;
    void DrawProperties() override;

    const char* GetTypeName() const override { return "Pipeline"; }

    bool BuildPipeline();
    void DestroyPipeline();

    void UpdateDescriptorPins(const std::vector<ShaderNode*>& shaders);

    bool HasValidShaders() const;
    std::vector<ShaderNode*> GetConnectedShaders() const;

private:
    void CreatePins();
    void UpdatePipelineState();
};
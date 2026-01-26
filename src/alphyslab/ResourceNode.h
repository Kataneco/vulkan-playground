#pragma once
#include "node.h"

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

    ImageResourceNode(int nodeId, ResourceManager* resourceManager, VkDescriptorPool descriptorPool, VkDevice device);
    ~ImageResourceNode() override;

    const char* GetTypeName() const override { return "Image Resource"; }

    void Draw() override;
    void DrawProperties() override;
};

class BufferResourceNode : public GraphNode {
private:
    ResourceManager* resourceManager;
public:
    size_t size;
    bool isUniform;

    std::shared_ptr<Buffer> buffer;

    BufferResourceNode(int nodeId, ResourceManager* resourceManager, bool uniform = true);
    // this class is suicidal and self destructive, no need to write a custom destructor

    const char* GetTypeName() const override { return isUniform ? "Uniform Buffer" : "Storage Buffer"; }

    void Draw() override;
    void DrawProperties() override;
};

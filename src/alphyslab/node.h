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

// TODO FIXME ...
#include "/home/honeywrap/Documents/kitten/Engine.h"

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
    ShaderStageOut,
    Pipeline
};

enum class NodeType {
    Undefined,
    ImageResource,
    BufferResource,
    RenderTarget,
    ShaderGraph
};

inline const char* PinTypeToString(PinType type) {
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

inline VkDescriptorType PinTypeToDescriptorType(PinType type) {
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

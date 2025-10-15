#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imnodes.h>
#include <misc/cpp/imgui_stdlib.h>
#include <stb_image.h>

#include <shader/ShaderReflection.h>
#include <resource/MemoryAllocator.h>
#include <resource/ResourceManager.h>
#include <resource/StagingBufferManager.h>
#include <scene/Mesh.h>
#include <sync/Fence.h>
#include <sync/Semaphore.h>

#include "app/ShaderWorkspace.h"

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
    Pipeline,
    Mesh,
    Semaphore,
    Fence,
    PipelineBarrier
};

inline const char* PinTypeToString(PinType type) {
    switch (type) {
        case PinType::UniformBuffer: return "UBO";
        case PinType::StorageBuffer: return "SSBO";
        case PinType::CombinedImageSampler: return "Sampler";
        case PinType::StorageImage: return "Storage Image";
        case PinType::InputAttachment: return "Input Attachment";
        case PinType::PushConstant: return "Push Constants";
        case PinType::VertexInput: return "Vertex Input";
        case PinType::FragmentOutput: return "Fragment Output";
        case PinType::ShaderStageIn: return "Stage In";
        case PinType::ShaderStageOut: return "Stage Out";
        case PinType::Pipeline: return "Pipeline";
        case PinType::Mesh: return "Mesh";
        case PinType::Semaphore: return "Semaphore";
        case PinType::Fence: return "Fence";
        case PinType::PipelineBarrier: return "Barrier";
        default: return "Unknown";
    }
}

inline VkDescriptorType PinTypeToDescriptorType(PinType type) {
    switch (type) {
        case PinType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case PinType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case PinType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case PinType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case PinType::InputAttachment: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case PinType::Pipeline:
        case PinType::Mesh:
        case PinType::Semaphore:
        case PinType::Fence:
        case PinType::PipelineBarrier:
        default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

struct Pin {
    int id = -1;
    int nodeId = -1;
    std::string name;
    bool isInput = false;
    PinType type = PinType::UniformBuffer;

    uint32_t set = 0;
    uint32_t binding = 0;
    VkShaderStageFlags stages = 0;
    size_t size = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent{0, 0, 0};
    uint32_t location = 0;
};

struct Link {
    int id = -1;
    int startPinId = -1;
    int endPinId = -1;
};

class GraphNode {
public:
    int id;
    ImVec2 position{0.0f, 0.0f};
    std::string name;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;

    GraphNode(int nodeId, std::string nodeName)
        : id(nodeId)
        , name(std::move(nodeName)) {}
    virtual ~GraphNode() = default;

    virtual void Draw() = 0;
    virtual void DrawProperties() {}
    virtual const char* GetTypeName() const = 0;
};

class ImageResourceNode : public GraphNode {
public:
    ImageResourceNode(int nodeId,
                      ResourceManager* resourceManager,
                      StagingBufferManager* stagingManager,
                      VkDescriptorPool descriptorPool,
                      VkDevice device)
        : GraphNode(nodeId, "Image")
        , device(device)
        , resourceManager(resourceManager)
        , stagingManager(stagingManager)
        , descriptorPool(descriptorPool) {
        nextPinId = id * 1000;
        samplerName = "ImageSampler" + std::to_string(id);
        imageName = "ImageNode" + std::to_string(id);
        sampler = resourceManager->createSampler({
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 16.0f
        }, samplerName);

        Pin colorInput{};
        colorInput.id = nextPinId++;
        colorInput.nodeId = id;
        colorInput.name = "Color Attachment";
        colorInput.isInput = true;
        colorInput.type = PinType::FragmentOutput;
        inputs.push_back(colorInput);

        Pin output{};
        output.id = nextPinId++;
        output.nodeId = id;
        output.name = "Image";
        output.isInput = false;
        output.type = PinType::CombinedImageSampler;
        outputs.push_back(output);

        updatePinMetadata();
        rebuildImage();
    }

    ~ImageResourceNode() override {
        releaseDescriptorSet();
        resourceManager->destroySampler(samplerName);
    }

    const char* GetTypeName() const override { return "Image Resource"; }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("Image");
        ImGui::SameLine();
        ImGui::Text("%ux%u", extent.width, extent.height);
        ImNodes::EndNodeTitleBar();

        if (!inputs.empty()) {
            ImNodes::BeginInputAttribute(inputs[0].id);
            ImGui::Text("Color Target ←");
            ImNodes::EndInputAttribute();
        }

        if (descriptorSet != VK_NULL_HANDLE) {
            ImNodes::BeginOutputAttribute(outputs[0].id);
            float scale = 200.0f;
            glm::vec2 sizeVec = glm::normalize(glm::vec2(static_cast<float>(extent.width), static_cast<float>(extent.height)));
            if (!std::isfinite(sizeVec.x) || !std::isfinite(sizeVec.y)) {
                sizeVec = {1.0f, 1.0f};
            }
            ImGui::Image(descriptorSet, ImVec2(sizeVec.x * scale, sizeVec.y * scale));
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Image Resource Properties");
        ImGui::Separator();

        const char* formatLabels[] = {
            "R8G8B8A8_SRGB", "R8G8B8A8_UNORM", "R16G16B16A16_SFLOAT",
            "R32G32B32A32_SFLOAT", "B8G8R8A8_SRGB", "D32_SFLOAT"
        };
        const VkFormat formatValues[] = {
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_D32_SFLOAT
        };
        int currentFormat = 0;
        for (int i = 0; i < IM_ARRAYSIZE(formatValues); ++i) {
            if (formatValues[i] == format) {
                currentFormat = i;
                break;
            }
        }
        if (ImGui::Combo("Format", &currentFormat, formatLabels, IM_ARRAYSIZE(formatLabels))) {
            format = formatValues[currentFormat];
            updatePinMetadata();
            rebuildImage();
        }

        int width = static_cast<int>(extent.width);
        int height = static_cast<int>(extent.height);
        if (ImGui::InputInt("Width", &width)) {
            extent.width = static_cast<uint32_t>(std::max(1, width));
            rebuildImage();
        }
        if (ImGui::InputInt("Height", &height)) {
            extent.height = static_cast<uint32_t>(std::max(1, height));
            rebuildImage();
        }

        int mipInput = static_cast<int>(mipLevels);
        if (ImGui::InputInt("Mip Levels", &mipInput)) {
            mipLevels = static_cast<uint32_t>(std::max(1, mipInput));
            rebuildImage();
        }

        int layerInput = static_cast<int>(arrayLayers);
        if (ImGui::InputInt("Array Layers", &layerInput)) {
            arrayLayers = static_cast<uint32_t>(std::max(1, layerInput));
            rebuildImage();
        }

        const char* sampleLabels[] = {"1x", "2x", "4x"};
        const VkSampleCountFlagBits sampleValues[] = {
            VK_SAMPLE_COUNT_1_BIT,
            VK_SAMPLE_COUNT_2_BIT,
            VK_SAMPLE_COUNT_4_BIT
        };
        int sampleIndex = 0;
        for (int i = 0; i < IM_ARRAYSIZE(sampleValues); ++i) {
            if (sampleValues[i] == samples) {
                sampleIndex = i;
                break;
            }
        }
        if (ImGui::Combo("Samples", &sampleIndex, sampleLabels, IM_ARRAYSIZE(sampleLabels))) {
            samples = sampleValues[sampleIndex];
            rebuildImage();
        }

        const char* tilingLabels[] = {"Optimal", "Linear"};
        VkImageTiling tilingValues[] = {VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TILING_LINEAR};
        int tilingIndex = tiling == VK_IMAGE_TILING_LINEAR ? 1 : 0;
        if (ImGui::Combo("Tiling", &tilingIndex, tilingLabels, IM_ARRAYSIZE(tilingLabels))) {
            tiling = tilingValues[tilingIndex];
            rebuildImage();
        }

        struct UsageEntry { const char* label; VkImageUsageFlags flag; };
        static const UsageEntry usageOptions[] = {
            {"Sampled", VK_IMAGE_USAGE_SAMPLED_BIT},
            {"Storage", VK_IMAGE_USAGE_STORAGE_BIT},
            {"Color Attachment", VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
            {"Depth Attachment", VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
            {"Transfer Src", VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
            {"Transfer Dst", VK_IMAGE_USAGE_TRANSFER_DST_BIT}
        };
        bool usageChanged = false;
        for (const auto& option : usageOptions) {
            bool enabled = (usageFlags & option.flag) != 0;
            if (ImGui::Checkbox(option.label, &enabled)) {
                usageChanged = true;
                if (enabled) {
                    usageFlags |= option.flag;
                } else {
                    usageFlags &= ~option.flag;
                }
            }
        }
        if (usageChanged) {
            rebuildImage();
        }

        if (pathInput.empty() && !imagePath.empty()) {
            pathInput = imagePath.string();
        }
        ImGui::InputText("Texture Path", &pathInput);
        if (ImGui::Button("Load Texture")) {
            if (pathInput.empty()) {
                statusMessage = "Provide a texture path.";
            } else if (loadTextureFromFile(pathInput)) {
                statusMessage = "Loaded " + pathInput;
            } else {
                statusMessage = "Failed to load texture.";
            }
        }
        if (!statusMessage.empty()) {
            ImGui::TextWrapped("%s", statusMessage.c_str());
        }
    }

    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }
    VkExtent2D getExtent2D() const { return VkExtent2D{extent.width, extent.height}; }
    const std::filesystem::path& getImagePath() const { return imagePath; }

private:
    VkDevice device;
    ResourceManager* resourceManager;
    StagingBufferManager* stagingManager;

    int nextPinId = 0;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    VkExtent3D extent{1024, 1024, 1};
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageLayout shaderLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::filesystem::path imagePath;
    std::string pathInput;
    std::string statusMessage;

    std::shared_ptr<Image> image;
    std::shared_ptr<Sampler> sampler;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    std::string samplerName;
    std::string imageName;

    void updatePinMetadata() {
        if (!outputs.empty()) {
            outputs[0].nodeId = id;
            outputs[0].format = format;
            outputs[0].extent = extent;
        }
        if (!inputs.empty()) {
            inputs[0].nodeId = id;
            inputs[0].format = format;
            inputs[0].extent = extent;
        }
    }

    void rebuildImage(const void* pixelData = nullptr, size_t dataSize = 0) {
        releaseDescriptorSet();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = extent;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.samples = samples;
        imageInfo.tiling = tiling;
        VkImageUsageFlags actualUsage = usageFlags | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (pixelData && dataSize > 0) {
            actualUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        imageInfo.usage = actualUsage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        image = resourceManager->createImage(imageInfo, {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE}, imageName);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image->getImage();
        viewInfo.viewType = viewType;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectMask();
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = arrayLayers;
        image->createImageView(viewInfo);

        allocateDescriptorSet();

        if (pixelData && dataSize > 0 && stagingManager) {
            std::vector<uint8_t> copy(dataSize);
            std::memcpy(copy.data(), pixelData, dataSize);
            stagingManager->stageImageData(copy.data(), image->getImage(), dataSize,
                                           {aspectMask(), 0, 0, arrayLayers},
                                           {0, 0, 0}, extent);
            stagingManager->flush();
        }
    }

    void releaseDescriptorSet() {
        if (descriptorSet != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
            descriptorSet = VK_NULL_HANDLE;
        }
    }

    void allocateDescriptorSet() {
        VkDescriptorSetLayout layout = ImGui_ImplVulkan_GetDescriptorSetLayout();
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;
        vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = shaderLayout;
        imageInfo.imageView = image ? image->getImageView() : VK_NULL_HANDLE;
        imageInfo.sampler = sampler ? sampler->getSampler() : VK_NULL_HANDLE;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    VkImageAspectFlags aspectMask() const {
        switch (format) {
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    bool loadTextureFromFile(const std::filesystem::path& path) {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) {
            return false;
        }

        std::vector<uint8_t> data(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        std::memcpy(data.data(), pixels, data.size());
        stbi_image_free(pixels);

        extent.width = static_cast<uint32_t>(std::max(1, width));
        extent.height = static_cast<uint32_t>(std::max(1, height));
        extent.depth = 1;
        mipLevels = 1;
        arrayLayers = 1;
        format = VK_FORMAT_R8G8B8A8_SRGB;
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imagePath = path;
        updatePinMetadata();
        rebuildImage(data.data(), data.size());
        return true;
    }
};

class BufferResourceNode : public GraphNode {
public:
    BufferResourceNode(int nodeId, ResourceManager* resourceManager, bool uniform)
        : GraphNode(nodeId, uniform ? "Uniform Buffer" : "Storage Buffer")
        , resourceManager(resourceManager)
        , usageFlags(uniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        , size(256)
        , hostVisible(true) {
        usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        Pin output{};
        output.id = id * 1000;
        output.nodeId = id;
        output.name = "Buffer";
        output.isInput = false;
        output.type = uniform ? PinType::UniformBuffer : PinType::StorageBuffer;
        output.size = size;
        outputs.push_back(output);

        rebuildBuffer();
    }

    const char* GetTypeName() const override {
        if ((usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) && !(usageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
            return "Uniform Buffer";
        }
        if ((usageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) && !(usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) {
            return "Storage Buffer";
        }
        return "Buffer";
    }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(GetTypeName());
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

        bool changed = false;
        int sizeKB = static_cast<int>(size / 1024);
        if (ImGui::InputInt("Size (KB)", &sizeKB)) {
            size = static_cast<size_t>(std::max(1, sizeKB)) * 1024;
            changed = true;
        }

        bool uniformUsage = (usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0;
        bool storageUsage = (usageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0;
        if (ImGui::Checkbox("Uniform Binding", &uniformUsage)) {
            changed = true;
        }
        if (ImGui::Checkbox("Storage Binding", &storageUsage)) {
            changed = true;
        }
        if (!uniformUsage && !storageUsage) {
            uniformUsage = true;
            changed = true;
        }

        struct Toggle { const char* label; VkBufferUsageFlags flag; };
        static const Toggle toggles[] = {
            {"Vertex Input", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
            {"Index Input", VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
            {"Transfer Src", VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
            {"Transfer Dst", VK_BUFFER_USAGE_TRANSFER_DST_BIT},
            {"Indirect", VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT}
        };
        for (const auto& toggle : toggles) {
            bool enabled = (usageFlags & toggle.flag) != 0;
            if (ImGui::Checkbox(toggle.label, &enabled)) {
                changed = true;
                if (enabled) {
                    usageFlags |= toggle.flag;
                } else {
                    usageFlags &= ~toggle.flag;
                }
            }
        }

        bool hostToggle = hostVisible;
        if (ImGui::Checkbox("Host Visible", &hostToggle)) {
            hostVisible = hostToggle;
            allocationInfo.usage = hostVisible ? VMA_MEMORY_USAGE_AUTO : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocationInfo.flags = hostVisible ? (VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
                                               : 0;
            changed = true;
        }

        if (uniformUsage) {
            usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        } else {
            usageFlags &= ~VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if (storageUsage) {
            usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        } else {
            usageFlags &= ~VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if (!(usageFlags & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))) {
            usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        outputs[0].type = (usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) && !(usageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                               ? PinType::UniformBuffer
                               : PinType::StorageBuffer;

        if (changed) {
            rebuildBuffer();
        }

        ImGui::Text("Size in bytes: %zu", size);
    }

private:
    ResourceManager* resourceManager;
    VmaAllocationCreateInfo allocationInfo{};
    VkBufferUsageFlags usageFlags = 0;
    size_t size = 0;
    bool hostVisible = true;

    std::shared_ptr<Buffer> buffer;

    void rebuildBuffer() {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer = resourceManager->createBuffer(bufferInfo, allocationInfo);
        if (!outputs.empty()) {
            outputs[0].size = size;
            outputs[0].nodeId = id;
        }
    }
};

class MeshResourceNode : public GraphNode {
public:
    MeshResourceNode(int nodeId, ResourceManager* resourceManager, StagingBufferManager* stagingManager)
        : GraphNode(nodeId, "Mesh")
        , resourceManager(resourceManager)
        , stagingManager(stagingManager) {
        name = "Mesh " + std::to_string(id);
        nextPinId = id * 1000;

        Pin vertex{};
        vertex.id = nextPinId++;
        vertex.nodeId = id;
        vertex.name = "Vertex Buffer";
        vertex.isInput = false;
        vertex.type = PinType::VertexInput;
        outputs.push_back(vertex);

        Pin meshPin{};
        meshPin.id = nextPinId++;
        meshPin.nodeId = id;
        meshPin.name = "Mesh";
        meshPin.isInput = false;
        meshPin.type = PinType::Mesh;
        outputs.push_back(meshPin);
    }

    const char* GetTypeName() const override { return "Mesh"; }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(displayName().c_str());
        ImNodes::EndNodeTitleBar();

        if (loaded) {
            ImGui::Text("%zu vertices", vertexCount);
            ImGui::Text("%zu indices", indexCount);
        } else {
            ImGui::TextDisabled("No mesh loaded");
        }

        for (auto& output : outputs) {
            ImNodes::BeginOutputAttribute(output.id);
            ImGui::Indent(60);
            ImGui::Text("→ %s", PinTypeToString(output.type));
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Mesh Properties");
        ImGui::Separator();

        ImGui::InputText("Node Name", &name);

        if (pathBuffer.empty() && !meshPath.empty()) {
            pathBuffer = meshPath.string();
        }

        if (ImGui::InputText("Mesh Path", &pathBuffer)) {
            pathBufferDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Mesh")) {
            loadFromPath(std::filesystem::path(pathBuffer));
        }

        if (!statusMessage.empty()) {
            ImGui::TextWrapped("%s", statusMessage.c_str());
        }

        if (loaded) {
            ImGui::Separator();
            ImGui::Text("Loaded: %s", meshPath.string().c_str());
            ImGui::Text("Vertices: %zu", vertexCount);
            ImGui::Text("Indices: %zu", indexCount);
            ImGui::Text("Vertex Buffer Size: %zu bytes", vertexCount * sizeof(Vertex));
            ImGui::Text("Index Buffer Size: %zu bytes", indexCount * sizeof(uint32_t));
        }
    }

    const Mesh* getMesh() const { return loaded ? &mesh : nullptr; }
    std::string displayName() const {
        if (!meshPath.empty()) {
            return meshPath.filename().string();
        }
        return name.empty() ? std::string{"Mesh"} : name;
    }

    std::filesystem::path getPath() const { return meshPath; }

private:
    ResourceManager* resourceManager;
    StagingBufferManager* stagingManager;
    Mesh mesh{};
    bool loaded = false;
    size_t vertexCount = 0;
    size_t indexCount = 0;
    std::filesystem::path meshPath;
    std::string pathBuffer;
    bool pathBufferDirty = false;
    std::string statusMessage;
    int nextPinId = 0;

    bool loadFromPath(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            statusMessage = "Mesh file does not exist.";
            return false;
        }

        try {
            Mesh loadedMesh = Mesh::loadObj(path.string());
            loadedMesh.pushMesh(*resourceManager, *stagingManager);
            mesh = std::move(loadedMesh);
            meshPath = path;
            vertexCount = mesh.vertices.size();
            indexCount = mesh.indices.size();
            loaded = true;
            statusMessage = "Loaded mesh successfully.";
            pathBuffer = meshPath.string();
            pathBufferDirty = false;
            if (!meshPath.filename().string().empty()) {
                name = meshPath.filename().string();
            }
            return true;
        } catch (const std::exception& e) {
            statusMessage = std::string{"Failed to load mesh: "} + e.what();
            loaded = false;
            vertexCount = 0;
            indexCount = 0;
            return false;
        }
    }
};

class SemaphoreGraphNode : public GraphNode {
public:
    SemaphoreGraphNode(int nodeId, VkDevice device)
        : GraphNode(nodeId, "Semaphore")
        , device(device) {
        name = "Semaphore " + std::to_string(id);
        Pin output{};
        output.id = id * 1000;
        output.nodeId = id;
        output.name = "Signal";
        output.isInput = false;
        output.type = PinType::Semaphore;
        outputs.push_back(output);

        recreateHandle();
    }

    const char* GetTypeName() const override { return "Semaphore"; }

    void Draw() override {
        ImNodes::BeginNode(id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndNodeTitleBar();

        ImGui::Text("Binary semaphore");

        ImNodes::BeginOutputAttribute(outputs[0].id);
        ImGui::Indent(60);
        ImGui::Text("→");
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Semaphore Properties");
        ImGui::Separator();
        ImGui::InputText("Name", &name);
        ImGui::TextDisabled("Timeline semaphores are not yet supported.");
        if (ImGui::Button("Recreate")) {
            recreateHandle();
        }
        if (!statusMessage.empty()) {
            ImGui::TextWrapped("%s", statusMessage.c_str());
        }
    }

    VkSemaphore getHandle() const { return handle ? static_cast<VkSemaphore>(*handle) : VK_NULL_HANDLE; }
    std::string summary() const { return "Binary semaphore"; }

private:
    VkDevice device;
    std::unique_ptr<Semaphore> handle;
    std::string statusMessage;

    void recreateHandle() {
        handle = std::make_unique<Semaphore>(device);
        statusMessage = "Created binary semaphore.";
    }
};

class FenceGraphNode : public GraphNode {
public:
    FenceGraphNode(int nodeId, VkDevice device)
        : GraphNode(nodeId, "Fence")
        , device(device) {
        name = "Fence " + std::to_string(id);
        Pin output{};
        output.id = id * 1000;
        output.nodeId = id;
        output.name = "Fence";
        output.isInput = false;
        output.type = PinType::Fence;
        outputs.push_back(output);

        recreateHandle();
    }

    const char* GetTypeName() const override { return "Fence"; }

    void Draw() override {
        ImNodes::BeginNode(id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndNodeTitleBar();

        ImGui::Text(signaled ? "Start Signaled" : "Start Unsignaled");

        ImNodes::BeginOutputAttribute(outputs[0].id);
        ImGui::Indent(60);
        ImGui::Text("→");
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Fence Properties");
        ImGui::Separator();
        ImGui::InputText("Name", &name);
        if (ImGui::Checkbox("Create Signaled", &signaled)) {
            recreateHandle();
        }
        if (!statusMessage.empty()) {
            ImGui::TextWrapped("%s", statusMessage.c_str());
        }
    }

    VkFence getHandle() const { return fence ? static_cast<VkFence>(*fence) : VK_NULL_HANDLE; }
    std::string summary() const { return signaled ? "Signaled fence" : "Fence"; }

private:
    VkDevice device;
    std::unique_ptr<Fence> fence;
    bool signaled = false;
    std::string statusMessage;

    void recreateHandle() {
        fence = std::make_unique<Fence>(device, signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0);
        statusMessage = signaled ? "Fence created in signaled state." : "Fence created.";
    }
};

class PipelineBarrierNode : public GraphNode {
public:
    struct Settings {
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkAccessFlags srcAccess = 0;
        VkAccessFlags dstAccess = 0;
        bool imageBarrier = false;
        VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };

    PipelineBarrierNode(int nodeId)
        : GraphNode(nodeId, "Pipeline Barrier") {
        name = "Barrier " + std::to_string(id);
        Pin output{};
        output.id = id * 1000;
        output.nodeId = id;
        output.name = "Barrier";
        output.isInput = false;
        output.type = PinType::PipelineBarrier;
        outputs.push_back(output);
    }

    const char* GetTypeName() const override { return "Pipeline Barrier"; }

    void Draw() override {
        ImNodes::BeginNode(id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(name.c_str());
        ImNodes::EndNodeTitleBar();

        ImGui::Text("%s → %s", stageLabel(settings.srcStage).c_str(), stageLabel(settings.dstStage).c_str());
        ImGui::Text("Access: %s / %s", accessLabel(settings.srcAccess).c_str(), accessLabel(settings.dstAccess).c_str());
        if (settings.imageBarrier) {
            ImGui::Text("Layout: %s → %s", layoutLabel(settings.oldLayout).c_str(), layoutLabel(settings.newLayout).c_str());
        }

        ImNodes::BeginOutputAttribute(outputs[0].id);
        ImGui::Indent(60);
        ImGui::Text("→");
        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Pipeline Barrier");
        ImGui::Separator();
        ImGui::InputText("Name", &name);

        drawStageCombo("Src Stage", settings.srcStage);
        drawStageCombo("Dst Stage", settings.dstStage);
        drawAccessCombo("Src Access", settings.srcAccess);
        drawAccessCombo("Dst Access", settings.dstAccess);

        if (ImGui::Checkbox("Image Barrier", &settings.imageBarrier)) {
            if (!settings.imageBarrier) {
                settings.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                settings.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
        }
        if (settings.imageBarrier) {
            drawLayoutCombo("Old Layout", settings.oldLayout);
            drawLayoutCombo("New Layout", settings.newLayout);
        }
    }

    const Settings& getSettings() const { return settings; }
    std::string summary() const {
        std::string summary = stageLabel(settings.srcStage) + " → " + stageLabel(settings.dstStage);
        if (settings.imageBarrier) {
            summary += " (" + layoutLabel(settings.oldLayout) + " → " + layoutLabel(settings.newLayout) + ")";
        }
        return summary;
    }

private:
    Settings settings{};

    struct StageOption {
        const char* label;
        VkPipelineStageFlags flag;
    };

    struct AccessOption {
        const char* label;
        VkAccessFlags flag;
    };

    struct LayoutOption {
        const char* label;
        VkImageLayout layout;
    };

    static const std::array<StageOption, 8>& stageOptions() {
        static const std::array<StageOption, 8> options{{
            StageOption{"Top of Pipe", VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
            StageOption{"Vertex Input", VK_PIPELINE_STAGE_VERTEX_INPUT_BIT},
            StageOption{"Vertex Shader", VK_PIPELINE_STAGE_VERTEX_SHADER_BIT},
            StageOption{"Fragment Shader", VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
            StageOption{"Compute Shader", VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
            StageOption{"Transfer", VK_PIPELINE_STAGE_TRANSFER_BIT},
            StageOption{"Color Attachment Output", VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            StageOption{"Bottom of Pipe", VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT}
        }};
        return options;
    }

    static const std::array<AccessOption, 8>& accessOptions() {
        static const std::array<AccessOption, 8> options{{
            AccessOption{"None", 0},
            AccessOption{"Vertex Attribute Read", VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT},
            AccessOption{"Index Read", VK_ACCESS_INDEX_READ_BIT},
            AccessOption{"Uniform Read", VK_ACCESS_UNIFORM_READ_BIT},
            AccessOption{"Shader Read", VK_ACCESS_SHADER_READ_BIT},
            AccessOption{"Shader Write", VK_ACCESS_SHADER_WRITE_BIT},
            AccessOption{"Transfer Read", VK_ACCESS_TRANSFER_READ_BIT},
            AccessOption{"Transfer Write", VK_ACCESS_TRANSFER_WRITE_BIT}
        }};
        return options;
    }

    static const std::array<LayoutOption, 7>& layoutOptions() {
        static const std::array<LayoutOption, 7> options{{
            LayoutOption{"Undefined", VK_IMAGE_LAYOUT_UNDEFINED},
            LayoutOption{"General", VK_IMAGE_LAYOUT_GENERAL},
            LayoutOption{"Color Attachment", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
            LayoutOption{"Depth-Stencil", VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
            LayoutOption{"Shader Read", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            LayoutOption{"Transfer Src", VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL},
            LayoutOption{"Transfer Dst", VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL}
        }};
        return options;
    }

    void drawStageCombo(const char* label, VkPipelineStageFlags& value) {
        auto& options = stageOptions();
        int currentIndex = 0;
        for (size_t i = 0; i < options.size(); ++i) {
            if (options[i].flag == value) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }
        if (ImGui::BeginCombo(label, options[currentIndex].label)) {
            for (size_t i = 0; i < options.size(); ++i) {
                bool selected = (currentIndex == static_cast<int>(i));
                if (ImGui::Selectable(options[i].label, selected)) {
                    value = options[i].flag;
                    currentIndex = static_cast<int>(i);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void drawAccessCombo(const char* label, VkAccessFlags& value) {
        auto& options = accessOptions();
        int currentIndex = 0;
        for (size_t i = 0; i < options.size(); ++i) {
            if (options[i].flag == value) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }
        if (ImGui::BeginCombo(label, options[currentIndex].label)) {
            for (size_t i = 0; i < options.size(); ++i) {
                bool selected = (currentIndex == static_cast<int>(i));
                if (ImGui::Selectable(options[i].label, selected)) {
                    value = options[i].flag;
                    currentIndex = static_cast<int>(i);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void drawLayoutCombo(const char* label, VkImageLayout& value) {
        auto& options = layoutOptions();
        int currentIndex = 0;
        for (size_t i = 0; i < options.size(); ++i) {
            if (options[i].layout == value) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }
        if (ImGui::BeginCombo(label, options[currentIndex].label)) {
            for (size_t i = 0; i < options.size(); ++i) {
                bool selected = (currentIndex == static_cast<int>(i));
                if (ImGui::Selectable(options[i].label, selected)) {
                    value = options[i].layout;
                    currentIndex = static_cast<int>(i);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    std::string stageLabel(VkPipelineStageFlags flag) const {
        auto& options = stageOptions();
        for (const auto& option : options) {
            if (option.flag == flag) {
                return option.label;
            }
        }
        return "Custom";
    }

    std::string accessLabel(VkAccessFlags flag) const {
        auto& options = accessOptions();
        for (const auto& option : options) {
            if (option.flag == flag) {
                return option.label;
            }
        }
        return "Custom";
    }

    std::string layoutLabel(VkImageLayout layout) const {
        auto& options = layoutOptions();
        for (const auto& option : options) {
            if (option.layout == layout) {
                return option.label;
            }
        }
        return "Custom";
    }
};

class PipelineGroupNode : public GraphNode {
public:
    PipelineGroupNode(int nodeId)
        : GraphNode(nodeId, "Pipeline Group")
        , groupName("Group " + std::to_string(nodeId)) {
        int pinBase = id * 1000;

        pipelineInputId = pinBase++;
        Pin pipelineInput{};
        pipelineInput.id = pipelineInputId;
        pipelineInput.nodeId = id;
        pipelineInput.name = "Pipeline";
        pipelineInput.isInput = true;
        pipelineInput.type = PinType::Pipeline;
        inputs.push_back(pipelineInput);

        meshInputId = pinBase++;
        Pin meshInput{};
        meshInput.id = meshInputId;
        meshInput.nodeId = id;
        meshInput.name = "Meshes";
        meshInput.isInput = true;
        meshInput.type = PinType::Mesh;
        inputs.push_back(meshInput);

        semaphoreInputId = pinBase++;
        Pin semaphoreInput{};
        semaphoreInput.id = semaphoreInputId;
        semaphoreInput.nodeId = id;
        semaphoreInput.name = "Semaphores";
        semaphoreInput.isInput = true;
        semaphoreInput.type = PinType::Semaphore;
        inputs.push_back(semaphoreInput);

        fenceInputId = pinBase++;
        Pin fenceInput{};
        fenceInput.id = fenceInputId;
        fenceInput.nodeId = id;
        fenceInput.name = "Fences";
        fenceInput.isInput = true;
        fenceInput.type = PinType::Fence;
        inputs.push_back(fenceInput);

        barrierInputId = pinBase++;
        Pin barrierInput{};
        barrierInput.id = barrierInputId;
        barrierInput.nodeId = id;
        barrierInput.name = "Barriers";
        barrierInput.isInput = true;
        barrierInput.type = PinType::PipelineBarrier;
        inputs.push_back(barrierInput);
    }

    const char* GetTypeName() const override { return "Pipeline Group"; }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(groupName.c_str());
        ImNodes::EndNodeTitleBar();

        ImGui::Text("Pipeline: %s", pipelineId >= 0 ? pipelineName.c_str() : "Unbound");
        ImGui::Text("Meshes: %zu", meshNames.size());
        ImGui::Text("Semaphores: %zu", semaphoreNames.size());
        ImGui::Text("Fences: %zu", fenceNames.size());
        ImGui::Text("Barriers: %zu", barrierSummaries.size());

        for (auto& pin : inputs) {
            ImNodes::BeginInputAttribute(pin.id);
            ImGui::Text("← %s", pin.name.c_str());
            ImNodes::EndInputAttribute();
        }

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Pipeline Group");
        ImGui::Separator();
        ImGui::InputText("Group Name", &groupName);

        if (pipelineId >= 0) {
            ImGui::Text("Pipeline: %s", pipelineName.c_str());
            if (!stageNames.empty()) {
                ImGui::Text("Stages:");
                for (const auto& stage : stageNames) {
                    ImGui::BulletText("%s", stage.c_str());
                }
            }
        } else {
            ImGui::TextDisabled("No pipeline connected.");
        }

        if (!meshNames.empty()) {
            ImGui::Separator();
            ImGui::Text("Meshes (%zu)", meshNames.size());
            for (const auto& mesh : meshNames) {
                ImGui::BulletText("%s", mesh.c_str());
            }
        }

        if (!semaphoreNames.empty()) {
            ImGui::Separator();
            ImGui::Text("Semaphores (%zu)", semaphoreNames.size());
            for (const auto& semaphore : semaphoreNames) {
                ImGui::BulletText("%s", semaphore.c_str());
            }
        }

        if (!fenceNames.empty()) {
            ImGui::Separator();
            ImGui::Text("Fences (%zu)", fenceNames.size());
            for (const auto& fence : fenceNames) {
                ImGui::BulletText("%s", fence.c_str());
            }
        }

        if (!barrierSummaries.empty()) {
            ImGui::Separator();
            ImGui::Text("Barriers (%zu)", barrierSummaries.size());
            for (const auto& barrier : barrierSummaries) {
                ImGui::BulletText("%s", barrier.c_str());
            }
        }
    }

    void updateAssociations(int newPipelineId,
                             std::string newPipelineName,
                             std::vector<std::string> newStageNames,
                             std::vector<int> newMeshIds,
                             std::vector<std::string> newMeshNames,
                             std::vector<int> newSemaphoreIds,
                             std::vector<std::string> newSemaphoreNames,
                             std::vector<int> newFenceIds,
                             std::vector<std::string> newFenceNames,
                             std::vector<int> newBarrierIds,
                             std::vector<std::string> newBarrierSummaries) {
        pipelineId = newPipelineId;
        pipelineName = std::move(newPipelineName);
        stageNames = std::move(newStageNames);
        meshNodeIds = std::move(newMeshIds);
        meshNames = std::move(newMeshNames);
        semaphoreNodeIds = std::move(newSemaphoreIds);
        semaphoreNames = std::move(newSemaphoreNames);
        fenceNodeIds = std::move(newFenceIds);
        fenceNames = std::move(newFenceNames);
        barrierNodeIds = std::move(newBarrierIds);
        barrierSummaries = std::move(newBarrierSummaries);
    }

    void clearAssociations() {
        pipelineId = -1;
        pipelineName.clear();
        stageNames.clear();
        meshNodeIds.clear();
        meshNames.clear();
        semaphoreNodeIds.clear();
        semaphoreNames.clear();
        fenceNodeIds.clear();
        fenceNames.clear();
        barrierNodeIds.clear();
        barrierSummaries.clear();
    }

    int getPipelinePinId() const { return pipelineInputId; }
    int getMeshPinId() const { return meshInputId; }
    int getSemaphorePinId() const { return semaphoreInputId; }
    int getFencePinId() const { return fenceInputId; }
    int getBarrierPinId() const { return barrierInputId; }
    int getPipelineId() const { return pipelineId; }
    const std::string& getGroupName() const { return groupName; }
    size_t meshCount() const { return meshNames.size(); }
    size_t semaphoreCount() const { return semaphoreNames.size(); }
    size_t fenceCount() const { return fenceNames.size(); }
    size_t barrierCount() const { return barrierSummaries.size(); }
    const std::vector<std::string>& getMeshNames() const { return meshNames; }
    const std::vector<std::string>& getSemaphoreNames() const { return semaphoreNames; }
    const std::vector<std::string>& getFenceNames() const { return fenceNames; }
    const std::vector<std::string>& getBarrierSummaries() const { return barrierSummaries; }

private:
    int pipelineId = -1;
    std::string pipelineName;
    std::vector<std::string> stageNames;
    std::vector<int> meshNodeIds;
    std::vector<std::string> meshNames;
    std::vector<int> semaphoreNodeIds;
    std::vector<std::string> semaphoreNames;
    std::vector<int> fenceNodeIds;
    std::vector<std::string> fenceNames;
    std::vector<int> barrierNodeIds;
    std::vector<std::string> barrierSummaries;

    std::string groupName;

    int pipelineInputId = -1;
    int meshInputId = -1;
    int semaphoreInputId = -1;
    int fenceInputId = -1;
    int barrierInputId = -1;
};

class ShaderGraphNode : public GraphNode {
public:
    ShaderGraphNode(int nodeId, VkShaderStageFlagBits shaderStage, ShaderWorkspace* workspace)
        : GraphNode(nodeId, "Shader")
        , workspace(workspace)
        , stage(shaderStage) {
        nextPinId = id * 1000;
        UpdateName();
        PopulateFromReflection();
    }

    const char* GetTypeName() const override { return "Shader"; }

    void attachDocument(ShaderWorkspace::Document* doc) {
        document = doc;
        if (document) {
            document->stage = stage;
            shaderPath = document->hasPath() ? document->filePath.string() : std::string{};
        }
    }

    void UpdateName() {
        switch (stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: name = "Vertex Shader"; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: name = "Fragment Shader"; break;
            case VK_SHADER_STAGE_COMPUTE_BIT: name = "Compute Shader"; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: name = "Geometry Shader"; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: name = "Tess Control"; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: name = "Tess Eval"; break;
            default: name = "Shader"; break;
        }
    }

    void OnCompiled(const std::vector<uint32_t>& code) {
        spirvCode = code;
        std::string blob(reinterpret_cast<const char*>(spirvCode.data()), spirvCode.size() * sizeof(uint32_t));
        reflection = std::make_unique<ShaderReflection>(blob);
        loaded = true;
        PopulateFromReflection();
    }

    void Draw() override {
        ImNodes::BeginNode(id);

        ImNodes::BeginNodeTitleBar();
        const char* stageIcon = stage == VK_SHADER_STAGE_VERTEX_BIT   ? "V" :
                                stage == VK_SHADER_STAGE_FRAGMENT_BIT ? "F" :
                                stage == VK_SHADER_STAGE_COMPUTE_BIT  ? "C" :
                                stage == VK_SHADER_STAGE_GEOMETRY_BIT ? "G" :
                                stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ? "TC" :
                                stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ? "TE" : "S";
        ImGui::Text("%s %s", stageIcon, name.c_str());
        ImNodes::EndNodeTitleBar();

        if (document && document->dirty) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Modified");
        } else if (document && !document->compileMessage.empty()) {
            ImGui::TextColored(document->compileSucceeded ? ImVec4(0.5f, 1.0f, 0.6f, 1.0f)
                                                          : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "%s", document->compileMessage.c_str());
        }

        if (!shaderPath.empty()) {
            ImGui::TextDisabled("%s", shaderPath.c_str());
        }

        for (auto& input : inputs) {
            if (input.type == PinType::PushConstant || input.type == PinType::VertexInput) {
                continue;
            }
            ImNodes::BeginInputAttribute(input.id);
            if (input.type == PinType::ShaderStageIn) {
                ImGui::Text("← Stage In");
            } else if (input.type == PinType::CombinedImageSampler || input.type == PinType::StorageBuffer ||
                       input.type == PinType::UniformBuffer) {
                ImGui::Text("← [%u:%u] %s", input.set, input.binding, input.name.c_str());
            } else {
                ImGui::Text("← %s", input.name.c_str());
            }
            ImNodes::EndInputAttribute();
        }

        for (auto& output : outputs) {
            ImNodes::BeginOutputAttribute(output.id);
            if (output.type == PinType::ShaderStageOut) {
                ImGui::Text("Stage Out →");
            } else if (output.type == PinType::FragmentOutput) {
                ImGui::Text("[%u] %s →", output.location, output.name.c_str());
            } else {
                ImGui::Text("%s →", output.name.c_str());
            }
            ImNodes::EndOutputAttribute();
        }

        if (!inputs.empty()) {
            auto pushIt = std::find_if(inputs.begin(), inputs.end(), [](const Pin& pin) {
                return pin.type == PinType::PushConstant;
            });
            if (pushIt != inputs.end()) {
                ImGui::Spacing();
                ImGui::TextDisabled("Push Constants: %zu bytes", pushIt->size);
            }
        }

        ImNodes::EndNode();
    }

    void DrawProperties() override {
        ImGui::Text("Shader Properties");
        ImGui::Separator();

        const char* stageLabels[] = {"Vertex", "Fragment", "Compute", "Geometry", "Tess Control", "Tess Eval"};
        const VkShaderStageFlagBits stageValues[] = {
            VK_SHADER_STAGE_VERTEX_BIT,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            VK_SHADER_STAGE_COMPUTE_BIT,
            VK_SHADER_STAGE_GEOMETRY_BIT,
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
        };
        int currentStage = 0;
        for (int i = 0; i < IM_ARRAYSIZE(stageValues); ++i) {
            if (stageValues[i] == stage) {
                currentStage = i;
                break;
            }
        }
        if (ImGui::Combo("Stage", &currentStage, stageLabels, IM_ARRAYSIZE(stageLabels))) {
            stage = stageValues[currentStage];
            if (document) {
                document->stage = stage;
            }
            UpdateName();
            PopulateFromReflection();
        }

        if (document) {
            if (ImGui::Button("Focus Text Editor")) {
                workspace->setActiveDocument(document->nodeId);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Source")) {
                const std::string& candidate = !document->pendingPathInput.empty()
                                                   ? document->pendingPathInput
                                                   : shaderPath;
                if (!candidate.empty() && loadSourceFromDisk(candidate)) {
                    shaderPath = candidate;
                    document->pendingPathInput = candidate;
                    statusMessage = "Loaded " + candidate;
                } else {
                    statusMessage = "Failed to load shader";
                }
            }
            if (!statusMessage.empty()) {
                ImGui::TextWrapped("%s", statusMessage.c_str());
            }
        }

        if (reflection) {
            ImGui::Separator();
            ImGui::Text("Reflection Info:");
            ImGui::Text("Descriptor Sets: %zu", reflection->getDescriptorSetLayouts().size());
            ImGui::Text("Push Constants: %zu", reflection->getPushConstantRanges().size());
            ImGui::Text("Inputs: %zu", reflection->getInputVariables().size());
            ImGui::Text("Outputs: %zu", reflection->getOutputVariables().size());
        }
    }

    ShaderWorkspace::Document* getDocument() const { return document; }
    VkShaderStageFlagBits getStage() const { return stage; }
    const std::vector<uint32_t>& getSpirv() const { return spirvCode; }

private:
    ShaderWorkspace* workspace = nullptr;
    ShaderWorkspace::Document* document = nullptr;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirvCode;
    std::unique_ptr<ShaderReflection> reflection;
    bool loaded = false;
    int nextPinId = 0;
    std::string shaderPath;
    std::string statusMessage;

    bool hasStageInput() const {
        if (stage == VK_SHADER_STAGE_VERTEX_BIT || stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            return false;
        }
        return true;
    }

    bool hasStageOutput() const {
        if (stage == VK_SHADER_STAGE_FRAGMENT_BIT || stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            return false;
        }
        return true;
    }

    void PopulateFromReflection() {
        inputs.clear();
        outputs.clear();
        nextPinId = id * 1000;

        if (hasStageInput()) {
            Pin stageIn{};
            stageIn.id = nextPinId++;
            stageIn.nodeId = id;
            stageIn.name = "Previous Stage";
            stageIn.isInput = true;
            stageIn.type = PinType::ShaderStageIn;
            inputs.push_back(stageIn);
        }
        if (hasStageOutput()) {
            Pin stageOut{};
            stageOut.id = nextPinId++;
            stageOut.nodeId = id;
            stageOut.name = "Next Stage";
            stageOut.isInput = false;
            stageOut.type = PinType::ShaderStageOut;
            outputs.push_back(stageOut);
        }

        if (!hasStageOutput()) {
            Pin pipelineOut{};
            pipelineOut.id = nextPinId++;
            pipelineOut.nodeId = id;
            pipelineOut.name = "Pipeline";
            pipelineOut.isInput = false;
            pipelineOut.type = PinType::Pipeline;
            outputs.push_back(pipelineOut);
        }

        if (!reflection) {
            return;
        }

        for (const auto& setLayout : reflection->getDescriptorSetLayouts()) {
            size_t nameIndex = 0;
            for (const auto& binding : setLayout.bindings) {
                Pin input{};
                input.id = nextPinId++;
                input.nodeId = id;
                input.name = setLayout.names[nameIndex++];
                input.isInput = true;
                input.type = PinType::UniformBuffer;
                input.set = setLayout.set;
                input.binding = binding.binding;
                input.stages = binding.stageFlags;
                switch (binding.descriptorType) {
                    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        input.type = PinType::StorageBuffer;
                        break;
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                        input.type = PinType::CombinedImageSampler;
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        input.type = PinType::StorageImage;
                        break;
                    default:
                        input.type = PinType::UniformBuffer;
                        break;
                }
                inputs.push_back(input);
            }
        }

        for (const auto& pushRange : reflection->getPushConstantRanges()) {
            Pin push{};
            push.id = nextPinId++;
            push.nodeId = id;
            push.name = "PushConstants";
            push.isInput = true;
            push.type = PinType::PushConstant;
            push.size = pushRange.size;
            inputs.push_back(push);
        }

        if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
            for (const auto& inputVar : reflection->getInputVariables()) {
                Pin vertex{};
                vertex.id = nextPinId++;
                vertex.nodeId = id;
                vertex.name = inputVar.name;
                vertex.isInput = true;
                vertex.type = PinType::VertexInput;
                vertex.location = inputVar.location;
                inputs.push_back(vertex);
            }
        }

        if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            for (const auto& outputVar : reflection->getOutputVariables()) {
                Pin frag{};
                frag.id = nextPinId++;
                frag.nodeId = id;
                frag.name = outputVar.name;
                frag.isInput = false;
                frag.type = PinType::FragmentOutput;
                frag.location = outputVar.location;
                outputs.push_back(frag);
            }
        }
    }

    bool loadSourceFromDisk(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open() || !document) {
            return false;
        }
        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        document->editor.SetText(source);
        document->dirty = true;
        document->filePath = path;
        document->syncPathInput();
        shaderPath = path.string();
        return true;
    }
};

class VulkanNodeEditor {
public:
    struct PipelinePreview {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkExtent2D extent{0, 0};
    };

    struct GraphPipeline {
        int id = -1;
        std::string name;
        std::vector<int> stageNodeIds;
        std::vector<int> outputNodeIds;
        std::vector<int> meshNodeIds;
        bool compute = false;
    };

    VulkanNodeEditor(VulkanInstance& instance, Device& device, ShaderWorkspace& workspace)
        : workspace(workspace)
        , deviceRef(device)
        , device(device)
        , memoryAllocator(instance, device)
        , resourceManager(device, memoryAllocator)
        , stagingManager(device) {
        editorContext = ImNodes::EditorContextCreate();
        ImNodes::CreateContext();
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
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.0f},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.0f},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.0f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.0f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.0f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.0f},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}
        };

        size_t poolCount = 1024;
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve(poolSizes.size());
        for (auto [type, weight] : poolSizes) {
            sizes.push_back({type, static_cast<uint32_t>(poolCount * weight)});
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
        poolInfo.pPoolSizes = sizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(poolCount);
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
    }

    ~VulkanNodeEditor() {
        for (auto& [id, node] : nodes) {
            if (auto* shader = dynamic_cast<ShaderGraphNode*>(node.get())) {
                workspace.removeDocument(shader->id);
            }
        }
        ImNodes::EditorContextFree(editorContext);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }

    std::optional<PipelinePreview> getFocusedPreview() const {
        if (!previewActive || focusedOutputNodeId < 0) {
            return std::nullopt;
        }
        if (auto* image = getImageNode(focusedOutputNodeId)) {
            if (VkDescriptorSet descriptor = image->getDescriptorSet(); descriptor != VK_NULL_HANDLE) {
                return PipelinePreview{descriptor, image->getExtent2D()};
            }
        }
        return std::nullopt;
    }

    void AddImageNode() {
        auto node = std::make_unique<ImageResourceNode>(nextNodeId++, &resourceManager, &stagingManager, descriptorPool, device);
        nodes[node->id] = std::move(node);
        markPipelinesDirty();
    }

    void AddBufferNode(bool uniform = true) {
        auto node = std::make_unique<BufferResourceNode>(nextNodeId++, &resourceManager, uniform);
        nodes[node->id] = std::move(node);
        markPipelinesDirty();
    }

    void AddMeshNode() {
        auto node = std::make_unique<MeshResourceNode>(nextNodeId++, &resourceManager, &stagingManager);
        nodes[node->id] = std::move(node);
        markPipelinesDirty();
    }

    void AddSemaphoreNode() {
        auto node = std::make_unique<SemaphoreGraphNode>(nextNodeId++, device);
        nodes[node->id] = std::move(node);
    }

    void AddFenceNode() {
        auto node = std::make_unique<FenceGraphNode>(nextNodeId++, device);
        nodes[node->id] = std::move(node);
    }

    void AddBarrierNode() {
        auto node = std::make_unique<PipelineBarrierNode>(nextNodeId++);
        nodes[node->id] = std::move(node);
    }

    void AddPipelineGroupNode() {
        auto node = std::make_unique<PipelineGroupNode>(nextNodeId++);
        nodes[node->id] = std::move(node);
    }

    ShaderGraphNode& AddShaderNode(VkShaderStageFlagBits stage, const std::string& label = std::string{}) {
        auto node = std::make_unique<ShaderGraphNode>(nextNodeId++, stage, &workspace);
        auto& reference = *node;
        auto& document = workspace.createDocument(reference.id, stage, label);
        reference.attachDocument(&document);
        nodes[reference.id] = std::move(node);
        workspace.setActiveDocument(reference.id);
        markPipelinesDirty();
        return reference;
    }

    void onShaderCompiled(int nodeId, VkShaderStageFlagBits /*stage*/, const std::vector<uint32_t>& spirv, const std::string& /*entryPoint*/) {
        if (auto* node = getShaderNode(nodeId)) {
            node->OnCompiled(spirv);
            markPipelinesDirty();
        }
    }

    Pin* FindPin(int pinId) {
        for (auto& [nodeId, node] : nodes) {
            for (auto& pin : node->inputs) {
                if (pin.id == pinId) {
                    return &pin;
                }
            }
            for (auto& pin : node->outputs) {
                if (pin.id == pinId) {
                    return &pin;
                }
            }
        }
        return nullptr;
    }

    const Link* FindLinkToPin(int pinId) const {
        for (const auto& link : links) {
            if (link.endPinId == pinId) {
                return &link;
            }
        }
        return nullptr;
    }

    void Draw() {
        ImGui::Begin("Shader Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Add")) {
                if (ImGui::MenuItem("Image Resource")) {
                    AddImageNode();
                }
                if (ImGui::MenuItem("Uniform Buffer")) {
                    AddBufferNode(true);
                }
                if (ImGui::MenuItem("Storage Buffer")) {
                    AddBufferNode(false);
                }
                if (ImGui::MenuItem("Mesh Resource")) {
                    AddMeshNode();
                }
                if (ImGui::BeginMenu("New Shader")) {
                    if (ImGui::MenuItem("Vertex")) AddShaderNode(VK_SHADER_STAGE_VERTEX_BIT, "Vertex Shader");
                    if (ImGui::MenuItem("Fragment")) AddShaderNode(VK_SHADER_STAGE_FRAGMENT_BIT, "Fragment Shader");
                    if (ImGui::MenuItem("Compute")) AddShaderNode(VK_SHADER_STAGE_COMPUTE_BIT, "Compute Shader");
                    if (ImGui::MenuItem("Geometry")) AddShaderNode(VK_SHADER_STAGE_GEOMETRY_BIT, "Geometry Shader");
                    if (ImGui::MenuItem("Tess Control")) AddShaderNode(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "Tess Control");
                    if (ImGui::MenuItem("Tess Eval")) AddShaderNode(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "Tess Eval");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Synchronization")) {
                    if (ImGui::MenuItem("Semaphore")) { AddSemaphoreNode(); }
                    if (ImGui::MenuItem("Fence")) { AddFenceNode(); }
                    if (ImGui::MenuItem("Pipeline Barrier")) { AddBarrierNode(); }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Pipeline Group")) {
                    AddPipelineGroupNode();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        rebuildPipelinesIfNeeded();

        if (pipelines.empty()) {
            ImGui::TextDisabled("No pipelines built yet. Link shader stages to generate pipelines.");
        } else {
            const GraphPipeline* active = findPipeline(selectedPipelineId);
            if (!active && !pipelines.empty()) {
                selectedPipelineId = pipelines.front().id;
                active = &pipelines.front();
            }

            if (active) {
                if (ImGui::BeginCombo("Active Pipeline", active->name.c_str())) {
                    for (const auto& pipeline : pipelines) {
                        bool isSelected = pipeline.id == selectedPipelineId;
                        if (ImGui::Selectable(pipeline.name.c_str(), isSelected)) {
                            selectedPipelineId = pipeline.id;
                            ensureFocusedOutput();
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                if (!active->outputNodeIds.empty()) {
                    std::string label = "Select Output";
                    if (auto* currentImage = getImageNode(focusedOutputNodeId)) {
                        label = currentImage->name + "##FocusedOutput";
                    }
                    if (ImGui::BeginCombo("Preview Output", label.c_str())) {
                        for (size_t i = 0; i < active->outputNodeIds.size(); ++i) {
                            if (auto* image = getImageNode(active->outputNodeIds[i])) {
                                std::string option = image->name + "##" + std::to_string(image->id);
                                bool isSelected = focusedOutputNodeId == image->id;
                                if (ImGui::Selectable(option.c_str(), isSelected)) {
                                    focusedOutputNodeId = image->id;
                                }
                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    ImGui::TextDisabled("No render targets linked to this pipeline.");
                    focusedOutputNodeId = -1;
                    previewActive = false;
                }

                bool previewing = previewActive && focusedOutputNodeId >= 0;
                if (ImGui::Checkbox("Show in Viewport", &previewing)) {
                    previewActive = previewing;
                    if (previewActive) {
                        ensureFocusedOutput();
                        if (focusedOutputNodeId < 0) {
                            previewActive = false;
                        }
                    }
                }

                ImGui::Separator();
                ImGui::Text("Stages:");
                for (int stageNodeId : active->stageNodeIds) {
                    if (auto* shader = getShaderNode(stageNodeId)) {
                        ImGui::BulletText("%s", shader->name.c_str());
                    }
                }

                if (!active->meshNodeIds.empty()) {
                    ImGui::Separator();
                    ImGui::Text("Meshes:");
                    for (int meshNodeId : active->meshNodeIds) {
                        if (auto* mesh = getMeshNode(meshNodeId)) {
                            ImGui::BulletText("%s", mesh->displayName().c_str());
                        }
                    }
                }

                auto groups = gatherGroupsForPipeline(active->id);
                if (!groups.empty()) {
                    ImGui::Separator();
                    ImGui::Text("Groups:");
                    for (auto* group : groups) {
                        size_t syncTotal = group->semaphoreCount() + group->fenceCount() + group->barrierCount();
                        ImGui::BulletText("%s (%zu mesh%s, %zu sync)",
                                          group->getGroupName().c_str(),
                                          group->meshCount(),
                                          group->meshCount() == 1 ? "" : "es",
                                          syncTotal);
                        ImGui::Indent();
                        for (const auto& meshName : group->getMeshNames()) {
                            ImGui::Text("Mesh: %s", meshName.c_str());
                        }
                        for (const auto& semaphore : group->getSemaphoreNames()) {
                            ImGui::Text("Semaphore: %s", semaphore.c_str());
                        }
                        for (const auto& fence : group->getFenceNames()) {
                            ImGui::Text("Fence: %s", fence.c_str());
                        }
                        for (const auto& barrier : group->getBarrierSummaries()) {
                            ImGui::Text("Barrier: %s", barrier.c_str());
                        }
                        ImGui::Unindent();
                    }
                }
            }
        }

        ImNodes::EditorContextSet(editorContext);
        ImNodes::BeginNodeEditor();

        for (auto& [nodeId, node] : nodes) {
            node->Draw();
        }

        for (const auto& link : links) {
            ImNodes::Link(link.id, link.startPinId, link.endPinId);
        }

        int startPinId = 0;
        int endPinId = 0;
        if (ImNodes::IsLinkCreated(&startPinId, &endPinId)) {
            Pin* startPin = FindPin(startPinId);
            Pin* endPin = FindPin(endPinId);
            if (startPin && endPin) {
                if (startPin->isInput && !endPin->isInput) {
                    std::swap(startPin, endPin);
                    std::swap(startPinId, endPinId);
                }
                if (!startPin->isInput && endPin->isInput && canConnectPins(startPin, endPin)) {
                    links.erase(std::remove_if(links.begin(), links.end(), [endPinId](const Link& link) {
                        return link.endPinId == endPinId;
                    }), links.end());
                    Link newLink{nextLinkId++, startPinId, endPinId};
                    links.push_back(newLink);
                    markPipelinesDirty();
                    updateGroupAssociations();
                }
            }
        }

        int removedLink = 0;
        if (ImNodes::IsLinkDestroyed(&removedLink)) {
            links.erase(std::remove_if(links.begin(), links.end(), [removedLink](const Link& link) {
                return link.id == removedLink;
            }), links.end());
            markPipelinesDirty();
            updateGroupAssociations();
        }

        if (ImGui::BeginPopupContextWindow("NodeEditorContext",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Add Image Resource")) {
                AddImageNode();
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Add Uniform Buffer")) {
                AddBufferNode(true);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Add Storage Buffer")) {
                AddBufferNode(false);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Add Mesh Resource")) {
                AddMeshNode();
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::BeginMenu("Add Shader")) {
                if (ImGui::MenuItem("Vertex")) { AddShaderNode(VK_SHADER_STAGE_VERTEX_BIT, "Vertex Shader"); }
                if (ImGui::MenuItem("Fragment")) { AddShaderNode(VK_SHADER_STAGE_FRAGMENT_BIT, "Fragment Shader"); }
                if (ImGui::MenuItem("Geometry")) { AddShaderNode(VK_SHADER_STAGE_GEOMETRY_BIT, "Geometry Shader"); }
                if (ImGui::MenuItem("Compute")) { AddShaderNode(VK_SHADER_STAGE_COMPUTE_BIT, "Compute Shader"); }
                if (ImGui::MenuItem("Tess Control")) { AddShaderNode(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "Tess Control"); }
                if (ImGui::MenuItem("Tess Eval")) { AddShaderNode(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "Tess Eval"); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Add Synchronization")) {
                if (ImGui::MenuItem("Semaphore")) { AddSemaphoreNode(); }
                if (ImGui::MenuItem("Fence")) { AddFenceNode(); }
                if (ImGui::MenuItem("Pipeline Barrier")) { AddBarrierNode(); }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Add Pipeline Group")) {
                AddPipelineGroupNode();
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Rebuild Pipelines")) {
                markPipelinesDirty();
                rebuildPipelinesIfNeeded();
            }
            if (ImGui::MenuItem("Remove Unused Nodes")) {
                removeUnusedNodes();
                ImGui::CloseCurrentPopup();
            }
            if (!pipelines.empty() && ImGui::BeginMenu("Focus Pipeline")) {
                for (const auto& pipeline : pipelines) {
                    bool isSelected = pipeline.id == selectedPipelineId;
                    if (ImGui::MenuItem(pipeline.name.c_str(), nullptr, isSelected)) {
                        selectedPipelineId = pipeline.id;
                        ensureFocusedOutput();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        handleSelection();

        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            std::vector<int> selectedNodes;
            selectedNodes.resize(ImNodes::NumSelectedNodes());
            if (!selectedNodes.empty()) {
                ImNodes::GetSelectedNodes(selectedNodes.data());
                for (int nodeId : selectedNodes) {
                    removeLinksForNode(nodeId);
                    if (auto* shader = getShaderNode(nodeId)) {
                        workspace.removeDocument(shader->id);
                    }
                    nodes.erase(nodeId);
                    markPipelinesDirty();
                }
                ImNodes::ClearNodeSelection();
                updateGroupAssociations();
            }
        }

        ImNodes::EndNodeEditor();
        ImGui::End();

        ImGui::Begin("Node Properties");
        if (selectedNodeId >= 0) {
            if (auto it = nodes.find(selectedNodeId); it != nodes.end()) {
                it->second->DrawProperties();
            } else {
                ImGui::TextDisabled("Select a node to edit its properties.");
            }
        } else {
            ImGui::TextDisabled("No node selected.");
        }
        ImGui::End();
    }

private:
    std::unordered_map<int, std::unique_ptr<GraphNode>> nodes;
    std::vector<Link> links;
    int nextNodeId = 1;
    int nextLinkId = 1;
    int selectedNodeId = -1;
    int lastFocusedShaderNode = -1;

    std::vector<GraphPipeline> pipelines;
    bool pipelinesDirty = true;
    int selectedPipelineId = -1;
    int focusedOutputNodeId = -1;
    bool previewActive = false;

    ImNodesEditorContext* editorContext = nullptr;
    ShaderWorkspace& workspace;
    Device& deviceRef;
    VkDevice device;
    MemoryAllocator memoryAllocator;
    ResourceManager resourceManager;
    StagingBufferManager stagingManager;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    ShaderGraphNode* getShaderNode(int nodeId) {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return dynamic_cast<ShaderGraphNode*>(it->second.get());
        }
        return nullptr;
    }

    ImageResourceNode* getImageNode(int nodeId) const {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return dynamic_cast<ImageResourceNode*>(it->second.get());
        }
        return nullptr;
    }

    MeshResourceNode* getMeshNode(int nodeId) const {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return dynamic_cast<MeshResourceNode*>(it->second.get());
        }
        return nullptr;
    }

    SemaphoreGraphNode* getSemaphoreNode(int nodeId) const {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return dynamic_cast<SemaphoreGraphNode*>(it->second.get());
        }
        return nullptr;
    }

    FenceGraphNode* getFenceNode(int nodeId) const {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return dynamic_cast<FenceGraphNode*>(it->second.get());
        }
        return nullptr;
    }

    PipelineBarrierNode* getBarrierNode(int nodeId) const {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return dynamic_cast<PipelineBarrierNode*>(it->second.get());
        }
        return nullptr;
    }

    PipelineGroupNode* getGroupNode(int nodeId) const {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return dynamic_cast<PipelineGroupNode*>(it->second.get());
        }
        return nullptr;
    }

    GraphNode* getNode(int nodeId) {
        if (auto it = nodes.find(nodeId); it != nodes.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    const GraphPipeline* findPipeline(int pipelineId) const {
        for (const auto& pipeline : pipelines) {
            if (pipeline.id == pipelineId) {
                return &pipeline;
            }
        }
        return nullptr;
    }

    void ensureFocusedOutput() {
        if (pipelines.empty()) {
            focusedOutputNodeId = -1;
            selectedPipelineId = -1;
            previewActive = false;
            return;
        }
        const GraphPipeline* active = findPipeline(selectedPipelineId);
        if (!active) {
            selectedPipelineId = pipelines.front().id;
            active = &pipelines.front();
        }
        if (!active->outputNodeIds.empty()) {
            if (std::find(active->outputNodeIds.begin(), active->outputNodeIds.end(), focusedOutputNodeId) ==
                active->outputNodeIds.end()) {
                focusedOutputNodeId = active->outputNodeIds.front();
            }
        } else {
            focusedOutputNodeId = -1;
            previewActive = false;
        }
    }

    bool canConnectPins(Pin* start, Pin* end) {
        if (!start || !end) {
            return false;
        }
        if (start->nodeId == end->nodeId) {
            return false;
        }
        if (start->type == PinType::ShaderStageOut && end->type == PinType::ShaderStageIn) {
            auto* fromNode = getShaderNode(start->nodeId);
            auto* toNode = getShaderNode(end->nodeId);
            if (!fromNode || !toNode) {
                return false;
            }
            return canConnectStages(*fromNode, *toNode);
        }
        return start->type == end->type;
    }

    int stageOrder(VkShaderStageFlagBits stage) const {
        switch (stage) {
            case VK_SHADER_STAGE_VERTEX_BIT: return 0;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return 1;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return 2;
            case VK_SHADER_STAGE_GEOMETRY_BIT: return 3;
            case VK_SHADER_STAGE_FRAGMENT_BIT: return 4;
            default: return -1;
        }
    }

    bool canConnectStages(const ShaderGraphNode& from, const ShaderGraphNode& to) const {
        if (from.getStage() == VK_SHADER_STAGE_COMPUTE_BIT || to.getStage() == VK_SHADER_STAGE_COMPUTE_BIT) {
            return false;
        }
        int fromOrder = stageOrder(from.getStage());
        int toOrder = stageOrder(to.getStage());
        if (fromOrder < 0 || toOrder < 0) {
            return false;
        }
        return fromOrder < toOrder;
    }

    void markPipelinesDirty() { pipelinesDirty = true; }

    void rebuildPipelinesIfNeeded() {
        if (!pipelinesDirty) {
            return;
        }
        pipelinesDirty = false;
        pipelines.clear();

        int pipelineCounter = 1;
        std::unordered_set<int> visited;

        auto gatherOutputs = [&](const std::vector<int>& stageIds) {
            std::set<int> outputs;
            for (int stageId : stageIds) {
                if (auto* shader = getShaderNode(stageId)) {
                    for (const auto& pin : shader->outputs) {
                        if (pin.type == PinType::FragmentOutput) {
                            for (const auto* link : findLinksFromPin(pin.id)) {
                                if (Pin* endPin = FindPin(link->endPinId)) {
                                    if (auto* image = getImageNode(endPin->nodeId)) {
                                        outputs.insert(image->id);
                                    }
                                }
                            }
                        }
                    }
                    for (const auto& pin : shader->inputs) {
                        if (pin.type == PinType::StorageImage) {
                            if (const Link* link = FindLinkToPin(pin.id)) {
                                if (Pin* startPin = FindPin(link->startPinId)) {
                                    if (auto* image = getImageNode(startPin->nodeId)) {
                                        outputs.insert(image->id);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return std::vector<int>(outputs.begin(), outputs.end());
        };

        auto gatherMeshes = [&](const std::vector<int>& stageIds) {
            std::set<int> meshIds;
            for (int stageId : stageIds) {
                if (auto* shader = getShaderNode(stageId)) {
                    for (const auto& pin : shader->inputs) {
                        if (pin.type == PinType::VertexInput) {
                            if (const Link* link = FindLinkToPin(pin.id)) {
                                if (Pin* startPin = FindPin(link->startPinId)) {
                                    if (auto* mesh = getMeshNode(startPin->nodeId)) {
                                        meshIds.insert(mesh->id);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return std::vector<int>(meshIds.begin(), meshIds.end());
        };

        // Compute pipelines (single-stage)
        for (auto& [id, node] : nodes) {
            if (auto* shader = dynamic_cast<ShaderGraphNode*>(node.get())) {
                if (shader->getStage() == VK_SHADER_STAGE_COMPUTE_BIT) {
                    GraphPipeline pipeline{};
                    pipeline.id = pipelineCounter++;
                    pipeline.compute = true;
                    pipeline.stageNodeIds.push_back(shader->id);
                    pipeline.name = shader->name + " (Compute)";
                    pipeline.outputNodeIds = gatherOutputs(pipeline.stageNodeIds);
                    pipeline.meshNodeIds = gatherMeshes(pipeline.stageNodeIds);
                    pipelines.push_back(std::move(pipeline));
                    visited.insert(shader->id);
                }
            }
        }

        for (auto& [id, node] : nodes) {
            auto* shader = dynamic_cast<ShaderGraphNode*>(node.get());
            if (!shader) {
                continue;
            }
            if (shader->getStage() == VK_SHADER_STAGE_COMPUTE_BIT) {
                continue;
            }
            if (visited.count(shader->id)) {
                continue;
            }

            bool hasUpstream = false;
            for (const auto& pin : shader->inputs) {
                if (pin.type == PinType::ShaderStageIn) {
                    if (const Link* link = FindLinkToPin(pin.id)) {
                        hasUpstream = true;
                    }
                    break;
                }
            }
            if (hasUpstream) {
                continue;
            }

            GraphPipeline pipeline{};
            pipeline.id = pipelineCounter++;
            pipeline.stageNodeIds.push_back(shader->id);
            pipeline.name = shader->name + " Pipeline";

            ShaderGraphNode* current = shader;
            visited.insert(shader->id);
            while (true) {
                int stageOutPin = -1;
                for (const auto& outputPin : current->outputs) {
                    if (outputPin.type == PinType::ShaderStageOut) {
                        stageOutPin = outputPin.id;
                        break;
                    }
                }
                if (stageOutPin < 0) {
                    break;
                }
                const Link* link = findFirstLinkFrom(stageOutPin);
                if (!link) {
                    break;
                }
                Pin* endPin = FindPin(link->endPinId);
                if (!endPin) {
                    break;
                }
                ShaderGraphNode* next = getShaderNode(endPin->nodeId);
                if (!next || visited.count(next->id)) {
                    break;
                }
                pipeline.stageNodeIds.push_back(next->id);
                current = next;
                visited.insert(current->id);
            }

            pipeline.outputNodeIds = gatherOutputs(pipeline.stageNodeIds);
            pipeline.meshNodeIds = gatherMeshes(pipeline.stageNodeIds);
            pipelines.push_back(std::move(pipeline));
        }

        if (!pipelines.empty()) {
            if (!findPipeline(selectedPipelineId)) {
                selectedPipelineId = pipelines.front().id;
            }
        } else {
            selectedPipelineId = -1;
        }

        ensureFocusedOutput();
        updateGroupAssociations();
    }

    const Link* findFirstLinkFrom(int pinId) const {
        for (const auto& link : links) {
            if (link.startPinId == pinId) {
                return &link;
            }
        }
        return nullptr;
    }

    std::vector<const Link*> findLinksFromPin(int pinId) const {
        std::vector<const Link*> result;
        for (const auto& link : links) {
            if (link.startPinId == pinId) {
                result.push_back(&link);
            }
        }
        return result;
    }

    void handleSelection() {
        int selectedCount = ImNodes::NumSelectedNodes();
        if (selectedCount > 0) {
            std::vector<int> selectedIds(selectedCount);
            ImNodes::GetSelectedNodes(selectedIds.data());
            selectedNodeId = selectedIds.front();
            if (auto* shader = getShaderNode(selectedNodeId)) {
                if (lastFocusedShaderNode != shader->id) {
                    if (auto* document = shader->getDocument()) {
                        workspace.setActiveDocument(document->nodeId);
                        lastFocusedShaderNode = shader->id;
                    }
                    if (auto pipeline = findPipelineContaining(shader->id)) {
                        selectedPipelineId = pipeline->id;
                        ensureFocusedOutput();
                    }
                }
            }
        } else {
            selectedNodeId = -1;
            lastFocusedShaderNode = -1;
        }
    }

    void removeLinksForNode(int nodeId) {
        links.erase(std::remove_if(links.begin(), links.end(), [&](const Link& link) {
            Pin* start = FindPin(link.startPinId);
            Pin* end = FindPin(link.endPinId);
            return (start && start->nodeId == nodeId) || (end && end->nodeId == nodeId);
        }), links.end());
        markPipelinesDirty();
        updateGroupAssociations();
    }

    const GraphPipeline* findPipelineContaining(int nodeId) const {
        for (const auto& pipeline : pipelines) {
            if (std::find(pipeline.stageNodeIds.begin(), pipeline.stageNodeIds.end(), nodeId) != pipeline.stageNodeIds.end()) {
                return &pipeline;
            }
        }
        return nullptr;
    }

    bool pinHasLink(int pinId) const {
        for (const auto& link : links) {
            if (link.startPinId == pinId || link.endPinId == pinId) {
                return true;
            }
        }
        return false;
    }

    bool isNodeConnected(const GraphNode& node) const {
        for (const auto& pin : node.inputs) {
            if (pinHasLink(pin.id)) {
                return true;
            }
        }
        for (const auto& pin : node.outputs) {
            if (pinHasLink(pin.id)) {
                return true;
            }
        }
        return false;
    }

    void removeUnusedNodes() {
        std::vector<int> toRemove;
        for (auto& [id, node] : nodes) {
            if (!isNodeConnected(*node)) {
                if (auto* shader = dynamic_cast<ShaderGraphNode*>(node.get())) {
                    workspace.removeDocument(shader->id);
                }
                toRemove.push_back(id);
            }
        }
        for (int id : toRemove) {
            nodes.erase(id);
        }
        if (!toRemove.empty()) {
            markPipelinesDirty();
            updateGroupAssociations();
        }
    }

    void updateGroupAssociations() {
        for (auto& [id, node] : nodes) {
            auto* group = dynamic_cast<PipelineGroupNode*>(node.get());
            if (!group) {
                continue;
            }

            int pipelineId = -1;
            std::string pipelineName;
            std::vector<std::string> stageLabels;

            if (const Link* link = FindLinkToPin(group->getPipelinePinId())) {
                if (Pin* startPin = FindPin(link->startPinId)) {
                    if (const GraphPipeline* pipeline = findPipelineContaining(startPin->nodeId)) {
                        pipelineId = pipeline->id;
                        pipelineName = pipeline->name;
                        for (int stageNodeId : pipeline->stageNodeIds) {
                            if (auto* shader = getShaderNode(stageNodeId)) {
                                stageLabels.push_back(shader->name);
                            }
                        }
                    }
                }
            }

            auto gatherLinks = [&](int pinId, auto&& handler) {
                for (const auto& existing : links) {
                    if (existing.endPinId == pinId) {
                        if (Pin* startPin = FindPin(existing.startPinId)) {
                            handler(*startPin);
                        }
                    }
                }
            };

            std::vector<int> meshIds;
            std::vector<std::string> meshNames;
            gatherLinks(group->getMeshPinId(), [&](const Pin& startPin) {
                if (auto* mesh = getMeshNode(startPin.nodeId)) {
                    meshIds.push_back(mesh->id);
                    meshNames.push_back(mesh->displayName());
                }
            });

            std::vector<int> semaphoreIds;
            std::vector<std::string> semaphoreSummaries;
            gatherLinks(group->getSemaphorePinId(), [&](const Pin& startPin) {
                if (auto* semaphore = getSemaphoreNode(startPin.nodeId)) {
                    semaphoreIds.push_back(semaphore->id);
                    semaphoreSummaries.push_back(semaphore->name);
                }
            });

            std::vector<int> fenceIds;
            std::vector<std::string> fenceSummaries;
            gatherLinks(group->getFencePinId(), [&](const Pin& startPin) {
                if (auto* fence = getFenceNode(startPin.nodeId)) {
                    fenceIds.push_back(fence->id);
                    fenceSummaries.push_back(fence->name);
                }
            });

            std::vector<int> barrierIds;
            std::vector<std::string> barrierSummaries;
            gatherLinks(group->getBarrierPinId(), [&](const Pin& startPin) {
                if (auto* barrier = getBarrierNode(startPin.nodeId)) {
                    barrierIds.push_back(barrier->id);
                    barrierSummaries.push_back(barrier->summary());
                }
            });

            group->updateAssociations(pipelineId,
                                      std::move(pipelineName),
                                      std::move(stageLabels),
                                      std::move(meshIds),
                                      std::move(meshNames),
                                      std::move(semaphoreIds),
                                      std::move(semaphoreSummaries),
                                      std::move(fenceIds),
                                      std::move(fenceSummaries),
                                      std::move(barrierIds),
                                      std::move(barrierSummaries));
        }
    }

    std::vector<PipelineGroupNode*> gatherGroupsForPipeline(int pipelineId) const {
        std::vector<PipelineGroupNode*> result;
        for (const auto& [id, node] : nodes) {
            if (auto* group = dynamic_cast<PipelineGroupNode*>(node.get())) {
                if (group->getPipelineId() == pipelineId) {
                    result.push_back(group);
                }
            }
        }
        return result;
    }
};


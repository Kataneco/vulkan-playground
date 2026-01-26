#include "ResourceNode.h"

// Image Resource
ImageResourceNode::ImageResourceNode(int nodeId, ResourceManager* resourceManager, VkDescriptorPool descriptorPool, VkDevice device)
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
          .usage = VK_IMAGE_USAGE_SAMPLED_BIT
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

ImageResourceNode::~ImageResourceNode() {
    vkFreeDescriptorSets(device, descriptorPool, 1, &meowTargetSet);
    resourceManager->destroySampler("ImageSampler"+std::to_string(id));
}

void ImageResourceNode::Draw() {
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

void ImageResourceNode::DrawProperties() {
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

// Buffer resource
BufferResourceNode::BufferResourceNode(int nodeId, ResourceManager* resourceManager, bool uniform)
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

void BufferResourceNode::Draw() {
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

void BufferResourceNode::DrawProperties() {
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
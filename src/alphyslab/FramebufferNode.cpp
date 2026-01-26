#include "FramebufferNode.h"

FramebufferNode::FramebufferNode(int nodeId, ResourceManager* resourceManager, VkDescriptorPool descriptorPool, VkDevice device, std::unique_ptr<RenderPass>* renderPass)
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

FramebufferNode::~FramebufferNode() {
    if (displaySet != VK_NULL_HANDLE) {
        //vkFreeDescriptorSets(device, descriptorPool, 1, &displaySet);
    }
    resourceManager->destroySampler("RenderTargetSampler" + std::to_string(id));
}

void FramebufferNode::CreateRenderTarget() {
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

    displaySet = ImGui_ImplVulkan_AddTexture(sampler->getSampler(), targetImage->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    framebuffer = std::make_unique<Framebuffer>(device, **renderPassReference);
    framebuffer->create({targetImage->getImageView()}, extent.width, extent.height);
}

void FramebufferNode::Draw() {
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

void FramebufferNode::DrawProperties() {
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
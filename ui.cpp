#include "ui.h"

VulkanNodeEditor::VulkanNodeEditor(VulkanInstance& instance, Device& device)
    : memoryAllocator(instance, device),
      resourceManager(device, memoryAllocator),
      descriptorLayoutCache(device),
      pipelineLayoutCache(device, descriptorLayoutCache),
      device(device),
      commandPool(device, device.getGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
      graphicsQueue(device.getGraphicsQueue()) {

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

    commandBuffers = commandPool.allocateCommandBuffers(1);

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

VulkanNodeEditor::~VulkanNodeEditor() {
    ImNodes::EditorContextFree(editorContext);
    links.clear();
    nodes.clear();
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}

void VulkanNodeEditor::AddImageNode() {
    auto node = std::make_unique<ImageResourceNode>(nextNodeId++, &resourceManager, descriptorPool, device);
    nodes[node->id] = std::move(node);
}

void VulkanNodeEditor::AddBufferNode(bool uniform) {
    auto node = std::make_unique<BufferResourceNode>(nextNodeId++, &resourceManager, uniform);
    nodes[node->id] = std::move(node);
}

void VulkanNodeEditor::AddShaderNode(VkShaderStageFlagBits stage) {
    auto node = std::make_unique<ShaderNode>(nextNodeId++, stage);
    nodes[node->id] = std::move(node);
}

void VulkanNodeEditor::AddRenderTargetNode() {
    auto node = std::make_unique<FramebufferNode>(nextNodeId++, &resourceManager, descriptorPool, device, &renderPass);
    nodes[node->id] = std::move(node);
}

void VulkanNodeEditor::AddPipelineNode() {
    auto node = std::make_unique<PipelineNode>(nextNodeId++, device, &descriptorLayoutCache, &pipelineLayoutCache, renderPass.get());
    nodes[node->id] = std::move(node);
}

Pin* VulkanNodeEditor::FindPin(int pinId) const {
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

const Link* VulkanNodeEditor::FindLinkToPin(int pinId) const {
    for (const auto& link : links) {
        if (link.endPinId == pinId) return &link;
    }
    return nullptr;
}

GraphNode* VulkanNodeEditor::FindNodeByPin(int pinId) const {
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

void VulkanNodeEditor::UpdatePipelineConnections(PipelineNode* pipeline) {
    if (!pipeline) return;

    // Clear existing connections
    pipeline->vertexShader.node = nullptr;
    pipeline->fragmentShader.node = nullptr;
    pipeline->geometryShader.node = nullptr;

    // Find connected shader nodes
    for (const auto& link : links) {
        Pin* endPin = FindPin(link.endPinId);
        if (!endPin) continue;

        GraphNode* endNode = FindNodeByPin(link.endPinId);
        if (!endNode || endNode->id != pipeline->id) continue;

        GraphNode* startNode = FindNodeByPin(link.startPinId);
        if (!startNode || startNode->nodeType != NodeType::ShaderGraph) continue;

        auto* shader = dynamic_cast<ShaderNode*>(startNode);
        if (!shader) continue;

        if (link.endPinId == pipeline->vertexShaderInputPin) {
            pipeline->vertexShader.node = shader;
            pipeline->vertexShader.pinId = link.endPinId;
        } else if (link.endPinId == pipeline->fragmentShaderInputPin) {
            pipeline->fragmentShader.node = shader;
            pipeline->fragmentShader.pinId = link.endPinId;
        } else if (link.endPinId == pipeline->geometryShaderInputPin) {
            pipeline->geometryShader.node = shader;
            pipeline->geometryShader.pinId = link.endPinId;
        }
    }

    // Update descriptor pins based on connected shaders
    std::vector<ShaderNode*> connectedShaders;
    if (pipeline->vertexShader.node) connectedShaders.push_back(pipeline->vertexShader.node);
    if (pipeline->fragmentShader.node) connectedShaders.push_back(pipeline->fragmentShader.node);
    if (pipeline->geometryShader.node) connectedShaders.push_back(pipeline->geometryShader.node);

    pipeline->UpdateDescriptorPins(connectedShaders);
}

FramebufferNode* VulkanNodeEditor::GetConnectedRenderTarget(PipelineNode* pipeline) {
    if (!pipeline) return nullptr;

    // Find render target connected to pipeline's output
    for (const auto& link : links) {
        if (link.startPinId != pipeline->renderTargetOutputPin) continue;

        GraphNode* targetNode = FindNodeByPin(link.endPinId);
        if (!targetNode || targetNode->nodeType != NodeType::RenderTarget) continue;

        return dynamic_cast<FramebufferNode*>(targetNode);
    }

    return nullptr;
}

void VulkanNodeEditor::SetTextEditor(TextEditor* editor) {
    textEditor = editor;
}

void VulkanNodeEditor::EditShaderNode(int nodeId) {
    if (!nodes.contains(nodeId)) return;

    auto* shaderNode = dynamic_cast<ShaderNode*>(nodes[nodeId].get());
    if (!shaderNode) return;

    if (textEditor) {
        if (editingNodeId >= 0 && nodes.contains(editingNodeId)) {
            if (auto* prevNode = dynamic_cast<ShaderNode*>(nodes[editingNodeId].get())) {
                prevNode->sourceCode = textEditor->GetText();
            }
        }

        textEditor->SetText(shaderNode->sourceCode);
        auto lang = TextEditor::LanguageDefinition::GLSL();
        textEditor->SetLanguageDefinition(lang);
        editingNodeId = nodeId;
    }
}

void VulkanNodeEditor::SaveCurrentShaderEdit() {
    if (editingNodeId >= 0 && nodes.contains(editingNodeId) && textEditor) {
        if (auto* shaderNode = dynamic_cast<ShaderNode*>(nodes[editingNodeId].get())) {
            shaderNode->sourceCode = textEditor->GetText();
        }
    }
}

void VulkanNodeEditor::Draw(CommandBuffer& commandBuffer) {
    // Prepare render targets
    std::vector<FramebufferNode*> renderTargets;
    for (auto& [id, node] : nodes) {
        if (node->nodeType == NodeType::RenderTarget) {
            renderTargets.push_back(dynamic_cast<FramebufferNode*>(node.get()));
        }
    }

    for (auto renderTarget: renderTargets) {
        ResourceBarrier::transitionImageLayout(commandBuffer, renderTarget->targetImage->getImage(),
            renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Execute pipelines
    for (auto& [id, node] : nodes) {
        if (node->nodeType != NodeType::Pipeline) continue;

        auto* pipeline = dynamic_cast<PipelineNode*>(node.get());
        if (!pipeline || !pipeline->isBuilt) continue;

        auto* renderTarget = GetConnectedRenderTarget(pipeline);
        if (!renderTarget) continue;

        // Update viewport/scissor from render target
        pipeline->viewport.width = static_cast<float>(renderTarget->extent.width);
        pipeline->viewport.height = static_cast<float>(renderTarget->extent.height);
        pipeline->viewport.minDepth = 0.0f;
        pipeline->viewport.maxDepth = 1.0f;
        pipeline->scissor.extent = {
            static_cast<uint32_t>(renderTarget->extent.width),
            static_cast<uint32_t>(renderTarget->extent.height)
        };

        ResourceBarrier::transitionImageLayout(commandBuffer, renderTarget->targetImage->getImage(),
            renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        renderPass->begin(commandBuffer, *renderTarget->framebuffer,
            {.extent = {static_cast<uint32_t>(renderTarget->extent.width),
                       static_cast<uint32_t>(renderTarget->extent.height)}},
            {{.color = {0.0f, 0.5f, 1.0f, 1.0f}}, {.depthStencil = {1.0f, 0}}});

        vkCmdSetViewport(commandBuffer, 0, 1, &pipeline->viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &pipeline->scissor);

        commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        commandBuffer.draw(3);

        renderPass->end(commandBuffer);

        ResourceBarrier::transitionImageLayout(commandBuffer, renderTarget->targetImage->getImage(),
            renderTarget->format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    ImGui::Begin("Shader Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("P Pipeline")) AddPipelineNode();
            ImGui::Separator();
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
            // Allow shader to pipeline connections
            if (start->type == PinType::ShaderStageOut && end->type == PinType::ShaderStageIn) {
                canConnect = true;
            }
            // Allow pipeline to render target
            else if (start->type == PinType::RenderTarget && end->type == PinType::FragmentOutput) {
                canConnect = true;
            }
            // Allow resource connections (resources to shaders or pipelines)
            else if (start->type == end->type) {
                canConnect = true;
            }
        }

        if (canConnect) {
            Link newLink{};
            newLink.id = nextLinkId++;
            newLink.startPinId = startPin;
            newLink.endPinId = endPin;
            links.push_back(newLink);

            // Update pipeline connections if we connected to a pipeline
            GraphNode* endNode = FindNodeByPin(endPin);
            if (endNode && endNode->nodeType == NodeType::Pipeline) {
                UpdatePipelineConnections(dynamic_cast<PipelineNode*>(endNode));
            }
        }
    }

    int linkId;
    if (ImNodes::IsLinkDestroyed(&linkId)) {
        // Find the link being destroyed
        auto linkIt = std::ranges::find_if(links, [linkId](const Link& link) {
            return link.id == linkId;
        });

        if (linkIt != links.end()) {
            // Check if this link was connected to a pipeline
            GraphNode* endNode = FindNodeByPin(linkIt->endPinId);
            if (endNode && endNode->nodeType == NodeType::Pipeline) {
                auto* pipeline = dynamic_cast<PipelineNode*>(endNode);
                // Remove the link first
                links.erase(linkIt);
                // Then update pipeline connections
                UpdatePipelineConnections(pipeline);
            } else {
                links.erase(linkIt);
            }
        }
    }

    const int numSelected = ImNodes::NumSelectedNodes();
    if (numSelected > 0) {
        std::vector<int> selectedNodes(numSelected);
        ImNodes::GetSelectedNodes(selectedNodes.data());
        selectedNodeId = selectedNodes[0];
    } else {
        selectedNodeId = -1;
    }

    // Detect click on nodes
    int clickedNodeId = -1;
    if (ImNodes::IsNodeHovered(&clickedNodeId) && ImGui::IsMouseClicked(0)) {
        if (nodes.contains(clickedNodeId)) {
            if (auto* shaderNode = dynamic_cast<ShaderNode*>(nodes[clickedNodeId].get())) {
                EditShaderNode(clickedNodeId);
            } else if (auto* framebufferNode = dynamic_cast<FramebufferNode*>(nodes[clickedNodeId].get())) {
                focused_image = framebufferNode->displaySet;
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

            if (nodes.contains(nodeId)) {
                if (auto* framebufferNode = dynamic_cast<FramebufferNode*>(nodes[nodeId].get())) {
                    if (focused_image == framebufferNode->displaySet) {
                        focused_image = VK_NULL_HANDLE;
                    }
                }
            }

            links.erase(std::ranges::remove_if(links,
                [this, nodeId](const Link& link) {
                    Pin* start = FindPin(link.startPinId);
                    Pin* end = FindPin(link.endPinId);
                    auto startNode = std::ranges::find_if(nodes,
                        [start](const auto& pair) {
                            for (auto& p : pair.second->outputs)
                                if (&p == start) return true;
                            return false;
                        });
                    auto endNode = std::ranges::find_if(nodes,
                        [end](const auto& pair) {
                            for (auto& p : pair.second->inputs)
                                if (&p == end) return true;
                            return false;
                        });
                    return (startNode != nodes.end() && startNode->first == nodeId) ||
                           (endNode != nodes.end() && endNode->first == nodeId);
                }).begin(), links.end());

            nodes.erase(nodeId);
        }

        ImNodes::ClearNodeSelection();
        selectedNodeId = -1;
    }

    ImGui::End();

    // Properties panel
    ImGui::Begin("Node Properties");
    if (selectedNodeId >= 0 && nodes.contains(selectedNodeId)) {
        nodes[selectedNodeId]->DrawProperties();
    } else {
        ImGui::TextDisabled("No node selected");
        ImGui::Separator();
    }
    ImGui::End();
}

VkDescriptorSet VulkanNodeEditor::getFocusedImage() {
    return focused_image;
}
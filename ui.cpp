#include "ui.h"

bool PipelineBuilder::IsValid() const {
    return vertexShader && fragmentShader && renderTarget;
}

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

    // FIXME: WHATTT??!?!?!??!
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

void VulkanNodeEditor::AnalyzePipelines() {
    std::cout << "Analyzing!!" << std::endl;
    detectedPipelines.clear();

    // Find all render target nodes
    std::vector<FramebufferNode*> renderTargets;
    for (auto& [id, node] : nodes) {
        if (node->nodeType == NodeType::RenderTarget) {
            std::cout << "Render target found " << node->name << std::endl;
            renderTargets.push_back(dynamic_cast<FramebufferNode*>(node.get()));
        }
    }

    // For each render target, trace back to find the pipeline
    for (auto* rt : renderTargets) {
        PipelineBuilder builder;
        builder.renderTarget = rt;

        // Find fragment shader connected to render target
        for (const auto& link : links) {
            Pin* startPin = FindPin(link.startPinId);
            Pin* endPin = FindPin(link.endPinId);
            if (!startPin || !endPin) continue;
            if (endPin->type == PinType::FragmentOutput) {
                GraphNode* node = FindNodeByPin(link.endPinId);
                if (node && node->id == rt->id) {
                    GraphNode* faggot = FindNodeByPin(link.startPinId);
                    if (faggot->nodeType == NodeType::ShaderGraph) {
                        auto* fragShader = dynamic_cast<ShaderNode*>(faggot);
                        if (fragShader->stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
                            std::cout << "Fragment shader found" << std::endl;
                            builder.fragmentShader = fragShader;
                            break;
                        }
                    }
                }
            }
        }

        if (!builder.fragmentShader) {
            std::cout << "Fragment shader not found" << std::endl;
            continue;
        }

        // Find vertex shader connected to fragment shader
        if (builder.fragmentShader->stageInputPinId >= 0) {
            for (const auto& link : links) {
                if (link.endPinId == builder.fragmentShader->stageInputPinId) {
                    Pin* startPin = FindPin(link.startPinId);
                    if (startPin && startPin->type == PinType::ShaderStageOut) {
                        GraphNode* node = FindNodeByPin(link.startPinId);
                        if (node->nodeType == NodeType::ShaderGraph) {
                            auto* vertShader = dynamic_cast<ShaderNode*>(node);
                            if (vertShader->stage == VK_SHADER_STAGE_VERTEX_BIT) {
                                builder.vertexShader = vertShader;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (builder.IsValid()) {
            // Collect all resources connected to shaders
            std::vector<ShaderNode*> shaders = {builder.vertexShader, builder.fragmentShader};
            for (auto* shader : shaders) {
                for (const auto& input : shader->inputs) {
                    if (input.type != PinType::ShaderStageIn &&
                        input.type != PinType::PushConstant &&
                        input.type != PinType::VertexInput) {
                        // Find what's connected to this input
                        for (const auto& link : links) {
                            if (link.endPinId == input.id) {
                                builder.descriptorBindings[input.id] = {input.set, input.binding};
                                break;
                            }
                        }
                    }
                }
            }

            detectedPipelines.push_back(builder);
        } else {
            std::cout << "Invalid pipeline!!" << std::endl;
        }
    }
}

bool VulkanNodeEditor::BuildPipeline(PipelineBuilder& builder) {
    if (!builder.IsValid()) {
        std::cerr << "Invalid pipeline builder" << std::endl;
        return false;
    }

    if (!builder.vertexShader->loaded || !builder.fragmentShader->loaded) {
        std::cerr << "Shaders not compiled" << std::endl;
        return false;
    }

    std::cout << "Building pipeline:" << std::endl;
    std::cout << "  Vertex Shader: " << builder.vertexShader->name << std::endl;
    std::cout << "  Fragment Shader: " << builder.fragmentShader->name << std::endl;
    std::cout << "  Render Target: " << builder.renderTarget->extent.width << "x"
              << builder.renderTarget->extent.height << std::endl;
    std::cout << "  Descriptor Bindings: " << builder.descriptorBindings.size() << std::endl;

    auto vertCode = builder.vertexShader->spirvCode;
    auto fragCode = builder.fragmentShader->spirvCode;
    ShaderModule vertModule(device, vertCode), fragModule(device, fragCode);
    ShaderReflection vertexShader(vertCode), fragmentShader(fragCode);
    VkPipelineLayout pipelineLayout = pipelineLayoutCache.createPipelineLayout(vertexShader+fragmentShader);

    builder.viewport.width = static_cast<float>(builder.renderTarget->extent.width);
    builder.viewport.height = static_cast<float>(builder.renderTarget->extent.height);
    builder.viewport.minDepth = 0.0f;
    builder.viewport.maxDepth = 1.0f;

    builder.scissor.extent = {static_cast<uint32_t>(builder.renderTarget->extent.width), static_cast<uint32_t>(builder.renderTarget->extent.height)};

    // TODO: Ability to run arbitrary vector buffers through pipelines
    // TODO: Figure out how to customize render passes!!
    builder.pipeline = GraphicsPipelineBuilder()
    .setShaders(vertModule, fragModule)
    .setViewportState(builder.viewport, builder.scissor)
    .setRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 1.0f)
    .setColorBlendState({alphaBlend})
    .setDepthStencilState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS)
    .setLayout(pipelineLayout)
    .setRenderPass(*renderPass, 0)
    .setDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
    .build(device);

    return true;
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
    std::vector<FramebufferNode*> renderTargets;
    for (auto& [id, node] : nodes) {
        if (node->nodeType == NodeType::RenderTarget) {
            renderTargets.push_back(dynamic_cast<FramebufferNode*>(node.get()));
        }
    }

    for (auto renderTarget: renderTargets) {
        ResourceBarrier::transitionImageLayout(commandBuffer, renderTarget->targetImage->getImage(), renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    for (auto& pipeline: detectedPipelines) {
        if (pipeline.pipeline != VK_NULL_HANDLE) {
            ResourceBarrier::transitionImageLayout(commandBuffer, pipeline.renderTarget->targetImage->getImage(), pipeline.renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            renderPass->begin(commandBuffer, *pipeline.renderTarget->framebuffer, {.extent = {static_cast<uint32_t>(pipeline.renderTarget->extent.width), static_cast<uint32_t>(pipeline.renderTarget->extent.height)}}, {{.color = {0.0f, 0.5f, 1.0f, 1.0f}}, {.depthStencil = {1.0f, 0}}});

            vkCmdSetViewport(commandBuffer, 0, 1, &pipeline.viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &pipeline.scissor);

            commandBuffer.bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
            commandBuffer.draw(3);

            renderPass->end(commandBuffer);

            ResourceBarrier::transitionImageLayout(commandBuffer, pipeline.renderTarget->targetImage->getImage(), pipeline.renderTarget->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    ImGui::Begin("Shader Graph Editor", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Add")) {
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

        if (ImGui::BeginMenu("Pipeline")) {
            if (ImGui::MenuItem("OwO Analyze Graph")) {
                AnalyzePipelines();
            }
            ImGui::Separator();
            ImGui::TextDisabled("Detected Pipelines: %zu", detectedPipelines.size());
            for (size_t i = 0; i < detectedPipelines.size(); i++) {
                if (ImGui::MenuItem(("Build Pipeline " + std::to_string(i)).c_str())) {
                    BuildPipeline(detectedPipelines[i]);
                }
            }
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
            // Allow stage connections
            if (start->type == PinType::ShaderStageOut && end->type == PinType::ShaderStageIn) {
                canConnect = true;
            }
            // Allow fragment output to render target
            else if (start->type == PinType::FragmentOutput && end->type == PinType::FragmentOutput) {
                canConnect = true;
            }
            // Allow resource connections
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
        }
    }

    int linkId;
    if (ImNodes::IsLinkDestroyed(&linkId)) {
        links.erase(std::ranges::remove_if(links,
                                           [linkId](const Link& link) { return link.id == linkId; }).begin(),
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

    // Detect click on shader nodes
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

            if (auto* framebufferNode = dynamic_cast<FramebufferNode*>(nodes[clickedNodeId].get())) {
                if (focused_image == framebufferNode->displaySet) {
                    focused_image = VK_NULL_HANDLE;
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

    // Pipeline info panel
    ImGui::Begin("Pipeline Info");
    if (detectedPipelines.empty()) {
        ImGui::TextDisabled("No pipelines detected");
        ImGui::Separator();
        ImGui::TextWrapped("Build a complete pipeline:");
        ImGui::BulletText("Add a Vertex Shader");
        ImGui::BulletText("Add a Fragment Shader");
        ImGui::BulletText("Add a Render Target");
        ImGui::BulletText("Connect: Vertex → Fragment → Render Target");
        ImGui::BulletText("Use 'Pipeline → Analyze Graph'");
    } else {
        ImGui::Text("O_O Detected %zu pipeline(s)", detectedPipelines.size());
        ImGui::Separator();

        for (size_t i = 0; i < detectedPipelines.size(); i++) {
            auto& pipeline = detectedPipelines[i];
            if (ImGui::CollapsingHeader(("Pipeline " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "+ Complete Pipeline");
                ImGui::BulletText("Vertex: %s", pipeline.vertexShader->name.c_str());
                ImGui::BulletText("Fragment: %s", pipeline.fragmentShader->name.c_str());
                ImGui::BulletText("Target: %dx%d",
                    pipeline.renderTarget->extent.width,
                    pipeline.renderTarget->extent.height);
                ImGui::BulletText("Resources: %zu", pipeline.descriptorBindings.size());

                bool shadersReady = pipeline.vertexShader->loaded && pipeline.fragmentShader->loaded;

                if (!shadersReady) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "- Compile shaders first!");
                }

                ImGui::Spacing();
                if (ImGui::Button(("UwU Build Pipeline " + std::to_string(i)).c_str(), ImVec2(-1, 0))) {
                    if (shadersReady) {
                        BuildPipeline(pipeline);
                    }
                }

                if (!shadersReady) {
                    ImGui::SetItemTooltip("Compile all shaders before building the pipeline");
                }

                ImGui::Unindent();
            }
        }
    }
    ImGui::End();
}

VkDescriptorSet VulkanNodeEditor::getFocusedImage() {
    return focused_image;
}

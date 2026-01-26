#include "PipelineNode.h"

PipelineNode::PipelineNode(int nodeId, VkDevice dev, DescriptorLayoutCache* descLayoutCache,
                           PipelineLayoutCache* pipeLayoutCache, RenderPass* rp)
    : GraphNode(nodeId, "Graphics Pipeline", NodeType::Pipeline),
      device(dev), layoutCache(descLayoutCache), pipelineCache(pipeLayoutCache), renderPass(rp) {
    CreatePins();
}

PipelineNode::~PipelineNode() {
    DestroyPipeline();
}

void PipelineNode::CreatePins() {
    int pinId = id * 1000;

    // Shader stage inputs
    vertexShaderInputPin = pinId++;
    Pin vertInput;
    vertInput.id = vertexShaderInputPin;
    vertInput.name = "Vertex Shader";
    vertInput.isInput = true;
    vertInput.type = PinType::ShaderStageIn;
    vertInput.stages = VK_SHADER_STAGE_VERTEX_BIT;
    inputs.push_back(vertInput);

    fragmentShaderInputPin = pinId++;
    Pin fragInput;
    fragInput.id = fragmentShaderInputPin;
    fragInput.name = "Fragment Shader";
    fragInput.isInput = true;
    fragInput.type = PinType::ShaderStageIn;
    fragInput.stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    inputs.push_back(fragInput);

    geometryShaderInputPin = pinId++;
    Pin geomInput;
    geomInput.id = geometryShaderInputPin;
    geomInput.name = "Geometry Shader";
    geomInput.isInput = true;
    geomInput.type = PinType::ShaderStageIn;
    geomInput.stages = VK_SHADER_STAGE_GEOMETRY_BIT;
    inputs.push_back(geomInput);

    // Render target output
    renderTargetOutputPin = pinId++;
    Pin rtOutput;
    rtOutput.id = renderTargetOutputPin;
    rtOutput.name = "Render Target";
    rtOutput.isInput = false;
    rtOutput.type = PinType::RenderTarget;
    outputs.push_back(rtOutput);
}

void PipelineNode::UpdateDescriptorPins(const std::vector<ShaderNode*>& shaders) {
    // Remove old descriptor pins
    inputs.erase(std::ranges::remove_if(inputs, [](const Pin& p) {
        return p.type != PinType::ShaderStageIn;
    }).begin(), inputs.end());

    descriptorBindings.clear();
    descriptorInputPins.clear();

    int pinId = id * 1000 + 1000; // Offset to avoid conflicts with shader input pins

    // Collect all descriptor bindings from connected shaders
    std::map<std::pair<uint32_t, uint32_t>, Pin> uniqueBindings; // (set, binding) -> Pin

    for (auto* shader : shaders) {
        if (!shader || !shader->reflection) continue;

        for (const auto& setLayout : shader->reflection->getDescriptorSetLayouts()) {
            size_t nameIdx = 0;
            for (const auto& binding : setLayout.bindings) {
                auto key = std::make_pair(setLayout.set, binding.binding);

                // Only add if we haven't seen this binding yet
                if (uniqueBindings.find(key) == uniqueBindings.end()) {
                    Pin input;
                    input.id = pinId++;
                    input.name = setLayout.names[nameIdx];
                    input.isInput = true;
                    input.set = setLayout.set;
                    input.binding = binding.binding;
                    input.stages = binding.stageFlags;

                    switch(binding.descriptorType) {
                        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                            input.type = PinType::UniformBuffer;
                            break;
                        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                            input.type = PinType::StorageBuffer;
                            break;
                        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                            input.type = PinType::CombinedImageSampler;
                            break;
                        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                            input.type = PinType::StorageImage;
                            break;
                        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                            input.type = PinType::InputAttachment;
                            break;
                        default:
                            nameIdx++;
                            continue;
                    }

                    uniqueBindings[key] = input;
                    descriptorBindings[input.id] = key;
                    descriptorInputPins.push_back(input.id);
                }
                nameIdx++;
            }
        }
    }

    // Add all unique bindings as input pins
    for (auto& [key, pin] : uniqueBindings) {
        inputs.push_back(pin);
    }
}

bool PipelineNode::HasValidShaders() const {
    return vertexShader.node && vertexShader.node->loaded &&
           fragmentShader.node && fragmentShader.node->loaded;
}

std::vector<ShaderNode*> PipelineNode::GetConnectedShaders() const {
    std::vector<ShaderNode*> shaders;
    if (vertexShader.node) shaders.push_back(vertexShader.node);
    if (fragmentShader.node) shaders.push_back(fragmentShader.node);
    if (geometryShader.node) shaders.push_back(geometryShader.node);
    return shaders;
}

bool PipelineNode::BuildPipeline() {
    DestroyPipeline();

    if (!HasValidShaders()) {
        buildError = "Missing or uncompiled shaders";
        isBuilt = false;
        return false;
    }

    if (!layoutCache || !pipelineCache || !renderPass) {
        buildError = "Missing required pipeline infrastructure";
        isBuilt = false;
        return false;
    }

    try {
        // Create shader modules
        auto vertCode = vertexShader.node->spirvCode;
        auto fragCode = fragmentShader.node->spirvCode;

        ShaderModule vertModule(device, vertCode);
        ShaderModule fragModule(device, fragCode);

        // Create combined reflection for pipeline layout
        std::string vertSpirv(reinterpret_cast<const char*>(vertCode.data()), vertCode.size() * sizeof(uint32_t));
        std::string fragSpirv(reinterpret_cast<const char*>(fragCode.data()), fragCode.size() * sizeof(uint32_t));

        ShaderReflection vertReflection(vertSpirv);
        ShaderReflection fragReflection(fragSpirv);

        // Create pipeline layout
        pipelineLayout = pipelineCache->createPipelineLayout(vertReflection + fragReflection);

        // Build the graphics pipeline
        pipeline = GraphicsPipelineBuilder()
            .setShaders(vertModule, fragModule)
            .setViewportState(viewport, scissor)
            .setRasterizationState(polygonMode, cullMode, frontFace, lineWidth)
            .setColorBlendState({alphaBlend})
            .setDepthStencilState(depthTestEnable, depthWriteEnable, depthCompareOp)
            .setLayout(pipelineLayout)
            .setRenderPass(*renderPass, 0)
            .setDynamicState({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
            .build(device);

        isBuilt = true;
        buildError.clear();
        return true;

    } catch (const std::exception& e) {
        buildError = std::string("Pipeline build failed: ") + e.what();
        isBuilt = false;
        return false;
    }
}

void PipelineNode::DestroyPipeline() {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    // Note: pipelineLayout is managed by the cache, don't destroy it here
    pipelineLayout = VK_NULL_HANDLE;
    isBuilt = false;
}

void PipelineNode::UpdatePipelineState() {
    // This method is called when pipeline state properties are changed
    // and the pipeline needs to be rebuilt with the new state

    if (!isBuilt) {
        // Pipeline hasn't been built yet, nothing to update
        return;
    }

    // Mark as needing rebuild
    isBuilt = false;

    // Optionally, auto-rebuild if shaders are ready
    if (HasValidShaders()) {
        BuildPipeline();
    }
}

void PipelineNode::Draw() {
    ImNodes::BeginNode(id);

    ImNodes::BeginNodeTitleBar();
    ImGui::Text("GP Graphics Pipeline");
    ImNodes::EndNodeTitleBar();

    // Status indicator
    if (!isBuilt) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "○ Not Built");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "● Active");
    }

    // Shader inputs
    ImGui::TextDisabled("Shader Stages:");
    for (auto& input : inputs) {
        if (input.type == PinType::ShaderStageIn) {
            ImNodes::BeginInputAttribute(input.id);

            bool connected = false;
            if (input.id == vertexShaderInputPin && vertexShader.node) connected = true;
            if (input.id == fragmentShaderInputPin && fragmentShader.node) connected = true;
            if (input.id == geometryShaderInputPin && geometryShader.node) connected = true;

            if (connected) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "● %s", input.name.c_str());
            } else {
                ImGui::Text("○ %s", input.name.c_str());
            }

            ImNodes::EndInputAttribute();
        }
    }

    // Descriptor inputs
    if (!descriptorInputPins.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Resources:");
        for (auto& input : inputs) {
            if (input.type != PinType::ShaderStageIn) {
                ImNodes::BeginInputAttribute(input.id);
                ImGui::Text("← [%d:%d] %s", input.set, input.binding, input.name.c_str());
                ImNodes::EndInputAttribute();
            }
        }
    }

    // Render target output
    ImGui::Separator();
    ImNodes::BeginOutputAttribute(renderTargetOutputPin);
    ImGui::Text("Render Target →");
    ImNodes::EndOutputAttribute();

    ImNodes::EndNode();
}

void PipelineNode::DrawProperties() {
    ImGui::Text("Pipeline Properties");
    ImGui::Separator();

    // Shader status
    ImGui::Text("Connected Shaders:");
    ImGui::BulletText("Vertex: %s", vertexShader.node ?
        (vertexShader.node->loaded ? "Ready" : "Not compiled") : "Not connected");
    ImGui::BulletText("Fragment: %s", fragmentShader.node ?
        (fragmentShader.node->loaded ? "Ready" : "Not compiled") : "Not connected");
    ImGui::BulletText("Geometry: %s", geometryShader.node ?
        (geometryShader.node->loaded ? "Ready" : "Not compiled") : "Optional");

    ImGui::Separator();

    // Rasterization state
    if (ImGui::CollapsingHeader("Rasterization", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* polygonModes[] = {"Fill", "Line", "Point"};
        const VkPolygonMode polygonModeValues[] = {
            VK_POLYGON_MODE_FILL,
            VK_POLYGON_MODE_LINE,
            VK_POLYGON_MODE_POINT
        };

        int currentMode = 0;
        for (int i = 0; i < 3; i++) {
            if (polygonModeValues[i] == polygonMode) {
                currentMode = i;
                break;
            }
        }

        if (ImGui::Combo("Polygon Mode", &currentMode, polygonModes, 3)) {
            polygonMode = polygonModeValues[currentMode];
            UpdatePipelineState();
        }

        const char* cullModes[] = {"None", "Front", "Back", "Front & Back"};
        const VkCullModeFlags cullModeValues[] = {
            VK_CULL_MODE_NONE,
            VK_CULL_MODE_FRONT_BIT,
            VK_CULL_MODE_BACK_BIT,
            VK_CULL_MODE_FRONT_AND_BACK
        };

        int currentCull = 0;
        for (int i = 0; i < 4; i++) {
            if (cullModeValues[i] == cullMode) {
                currentCull = i;
                break;
            }
        }

        if (ImGui::Combo("Cull Mode", &currentCull, cullModes, 4)) {
            cullMode = cullModeValues[currentCull];
            UpdatePipelineState();
        }

        if (ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 10.0f)) {
            UpdatePipelineState();
        }
    }

    // Depth state
    if (ImGui::CollapsingHeader("Depth/Stencil")) {
        if (ImGui::Checkbox("Depth Test", &depthTestEnable)) {
            UpdatePipelineState();
        }
        if (ImGui::Checkbox("Depth Write", &depthWriteEnable)) {
            UpdatePipelineState();
        }
    }

    ImGui::Separator();

    // Build button
    bool canBuild = HasValidShaders();
    if (!canBuild) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button(isBuilt ? "⚙ Rebuild Pipeline" : "⚙ Build Pipeline", ImVec2(-1, 0))) {
        BuildPipeline();
    }

    if (!canBuild) {
        ImGui::EndDisabled();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
            "Connect and compile vertex and fragment shaders first");
    }

    if (!buildError.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build Error:");
        ImGui::TextWrapped("%s", buildError.c_str());
    }

    if (isBuilt) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Pipeline is built and ready!");
        ImGui::BulletText("Descriptor Bindings: %zu", descriptorBindings.size());
    }
}
#include "ShaderNode.h"

ShaderNode::ShaderNode(int nodeId, VkShaderStageFlagBits shaderStage)
    : GraphNode(nodeId, "Shader", NodeType::ShaderGraph), stage(shaderStage) {
    UpdateName();

    // Create stage connection pins
    int basePinId = nodeId * 1000 + 10000; // Offset to avoid conflicts

    // Output pin for connecting to pipeline
    // All shader stages can connect to a pipeline node
    stageOutputPinId = basePinId++;
    Pin stageOut;
    stageOut.id = stageOutputPinId;

    // Name based on stage type
    if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
        stageOut.name = "Vertex Stage";
    } else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
        stageOut.name = "Fragment Stage";
    } else if (stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
        stageOut.name = "Geometry Stage";
    } else {
        stageOut.name = "Stage Output";
    }

    stageOut.isInput = false;
    stageOut.type = PinType::ShaderStageOut;
    stageOut.stages = stage;
    outputs.push_back(stageOut);
}

void ShaderNode::UpdateName() {
    switch(stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: name = "Vertex Shader"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: name = "Fragment Shader"; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: name = "Compute Shader"; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT: name = "Geometry Shader"; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: name = "Tess Control"; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: name = "Tess Eval"; break;
        default: name = "Shader"; break;
    }
}

bool ShaderNode::LoadSourceCode(const std::string& path) {
    shaderPath = path;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader source: " << path << std::endl;
        return false;
    }

    sourceCode = std::string((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    file.close();
    return true;
}

bool ShaderNode::LoadShader(const std::string& path) {
    shaderPath = path;

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader: " << path << std::endl;
        return false;
    }

    size_t fileSize = file.tellg();
    spirvCode.resize(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirvCode.data()), fileSize);
    file.close();

    std::string spirvString(reinterpret_cast<const char*>(spirvCode.data()),
                           spirvCode.size() * sizeof(uint32_t));
    reflection = std::make_unique<ShaderReflection>(spirvString);

    PopulateFromReflection();

    loaded = true;
    compileError.clear();
    return true;
}

bool ShaderNode::CompileShader() {
    shaderc_shader_kind kind;
    switch(stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: kind = shaderc_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: kind = shaderc_compute_shader; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT: kind = shaderc_geometry_shader; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: kind = shaderc_tess_control_shader; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: kind = shaderc_tess_evaluation_shader; break;
        default:
            compileError = "Unknown shader stage";
            return false;
    }

    std::string error;
    spirvCode = compileGLSL(sourceCode, kind, &error);

    if (spirvCode.empty()) {
        compileError = error;
        loaded = false;
        return false;
    }

    std::string spirvString(reinterpret_cast<const char*>(spirvCode.data()),
                           spirvCode.size() * sizeof(uint32_t));
    reflection = std::make_unique<ShaderReflection>(spirvString);

    PopulateFromReflection();

    loaded = true;
    compileError.clear();
    return true;
}

void ShaderNode::PopulateFromReflection() {
    if (!reflection) return;

    // Clear ALL pins except stage output pin (no stage input pins in new architecture)
    inputs.clear();

    outputs.erase(std::ranges::remove_if(outputs,
                                         [](const Pin& p) {
                                             return p.type != PinType::ShaderStageOut;
                                         }).begin(), outputs.end());

    // Reset pin ID counter to avoid ID conflicts after recompilation
    int pinId = id * 1000;

    // Skip past stage output pin ID if it exists
    if (stageOutputPinId >= 0) pinId = std::max(pinId, stageOutputPinId + 1);

    // Add descriptor set bindings as input pins
    for (const auto& setLayout : reflection->getDescriptorSetLayouts()) {
        size_t nameidx = 0;
        for (const auto& binding : setLayout.bindings) {
            Pin input;
            input.id = pinId++;
            input.name = setLayout.names[nameidx++];
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
                    continue;
            }

            inputs.push_back(input);
        }
    }

    // Add push constants
    for (const auto& pcRange : reflection->getPushConstantRanges()) {
        Pin input;
        input.id = pinId++;
        input.name = "PushConstants";
        input.isInput = true;
        input.type = PinType::PushConstant;
        input.size = pcRange.size;
        input.stages = pcRange.stageFlags;
        inputs.push_back(input);
    }

    // Add vertex inputs (for vertex shaders only)
    if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
        for (const auto& inputVar : reflection->getInputVariables()) {
            Pin input;
            input.id = pinId++;
            input.name = inputVar.name;
            input.isInput = true;
            input.type = PinType::VertexInput;
            input.location = inputVar.location;
            input.format = inputVar.format;
            inputs.push_back(input);
        }
    }

    // NOTE: Fragment outputs and stage inputs are no longer exposed as pins
    // Shaders now connect to pipelines via their stage output pin
}

void ShaderNode::Draw() {
    ImNodes::BeginNode(id);

    ImNodes::BeginNodeTitleBar();
    const char* icon = stage == VK_SHADER_STAGE_VERTEX_BIT ? "V" :
                      stage == VK_SHADER_STAGE_FRAGMENT_BIT ? "F" :
                      stage == VK_SHADER_STAGE_COMPUTE_BIT ? "C" : "G";
    ImGui::Text("%s %s", icon, name.c_str());
    ImNodes::EndNodeTitleBar();

    if (!shaderPath.empty()) {
        size_t lastSlash = shaderPath.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ?
            shaderPath.substr(lastSlash + 1) : shaderPath;
        ImGui::TextDisabled("%s", filename.c_str());
    }

    if (!loaded) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "- Not compiled");
    } else if (!compileError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "X Compile error");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "+ Ready");
    }

    bool hascontent = false;

    // Group inputs by type
    std::vector<Pin*> descriptorInputs;
    std::vector<Pin*> pushConstantInputs;
    std::vector<Pin*> vertexInputs;

    for (auto& input : inputs) {
        if (input.type == PinType::PushConstant)
            pushConstantInputs.push_back(&input);
        else if (input.type == PinType::VertexInput)
            vertexInputs.push_back(&input);
        else
            descriptorInputs.push_back(&input);
    }

    // Draw descriptor inputs
    if (!descriptorInputs.empty()) {
        hascontent = true;
        ImGui::TextDisabled("Resources:");
        for (auto* input : descriptorInputs) {
            ImNodes::BeginInputAttribute(input->id);
            ImGui::Text("← [%d:%d] %s", input->set, input->binding, input->name.c_str());
            ImNodes::EndInputAttribute();
        }
    }

    // Draw push constants
    if (!pushConstantInputs.empty()) {
        hascontent = true;
        if (!descriptorInputs.empty()) ImGui::Spacing();
        ImGui::TextDisabled("Push Constants:");
        for (auto* input : pushConstantInputs) {
            ImGui::Text(" %zu bytes", input->size);
        }
    }

    // Draw vertex inputs
    if (!vertexInputs.empty()) {
        hascontent = true;
        if (!descriptorInputs.empty() || !pushConstantInputs.empty()) ImGui::Spacing();
        ImGui::TextDisabled("Vertex Inputs:");
        for (auto* input : vertexInputs) {
            ImGui::Text(" [%d] %s", input->location, input->name.c_str());
        }
    }

    // Draw stage output pin
    if (stageOutputPinId >= 0) {
        if (!descriptorInputs.empty() || !pushConstantInputs.empty() || !vertexInputs.empty())
            ImGui::Separator();
        ImNodes::BeginOutputAttribute(stageOutputPinId);
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.8f, 1.0f), "To Pipeline →");
        ImNodes::EndOutputAttribute();
        hascontent = true;
    }

    if (!hascontent) {
        ImGui::Text("- No reflection data");
    }

    ImNodes::EndNode();
}

void ShaderNode::DrawProperties() {
    ImGui::Text("Shader Properties");
    ImGui::Separator();

    const char* stages[] = {"Vertex", "Fragment", "Compute", "Geometry", "Tess Control", "Tess Eval"};
    const VkShaderStageFlagBits stageMap[] = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
    };

    int currentStage = 0;
    for (int i = 0; i < IM_ARRAYSIZE(stageMap); i++) {
        if (stageMap[i] == stage) { currentStage = i; break; }
    }

    if (ImGui::Combo("Stage", &currentStage, stages, IM_ARRAYSIZE(stages))) {
        stage = stageMap[currentStage];
        UpdateName();
    }

    if (!sourceCode.empty()) {
        if (ImGui::Button("^-^ Compile Shader", ImVec2(-1, 0))) {
            if (CompileShader()) {
                ImGui::OpenPopup("Compile Success");
            } else {
                ImGui::OpenPopup("Compile Error");
            }
        }

        if (ImGui::BeginPopup("Compile Success")) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "+ Compilation successful!");
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("Compile Error")) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "X Compilation failed:");
            ImGui::Separator();
            ImGui::TextWrapped("%s", compileError.c_str());
            ImGui::EndPopup();
        }
    }

    if (!compileError.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Compile Error:");
        ImGui::TextWrapped("%s", compileError.c_str());
    }

    if (reflection) {
        ImGui::Separator();
        ImGui::Text("Reflection Info:");
        ImGui::BulletText("Descriptor Sets: %zu", reflection->getDescriptorSetLayouts().size());
        ImGui::BulletText("Push Constants: %zu", reflection->getPushConstantRanges().size());
        ImGui::BulletText("Inputs: %zu", reflection->getInputVariables().size());
        ImGui::BulletText("Outputs: %zu", reflection->getOutputVariables().size());
    }
}
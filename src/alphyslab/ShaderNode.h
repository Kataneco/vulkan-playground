#pragma once
#include "node.h"


class ShaderNode : public GraphNode {
public:
    std::string shaderPath;
    std::string sourceCode;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirvCode;
    std::unique_ptr<ShaderReflection> reflection;
    bool loaded = false;
    std::string compileError;

    int stageOutputPinId = -1;
    int stageInputPinId = -1;

    ShaderNode(int nodeId, VkShaderStageFlagBits shaderStage);

    const char* GetTypeName() const override { return "Shader"; }

    void UpdateName();

    bool LoadSourceCode(const std::string& path);
    bool LoadShader(const std::string& path);

    bool CompileShader();
    void PopulateFromReflection();

    void Draw() override;
    void DrawProperties() override;
};
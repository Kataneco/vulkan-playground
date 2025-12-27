#pragma once
#include <alphyslab/node.h>
#include <shader/ShaderReflection.h>
#include <shader/ShaderCompiler.h>

class ShaderNode : public GraphNode {
public:
    VkShaderStageFlagBits stage;
    std::string shaderPath;
    std::string sourceCode;
    std::vector<uint32_t> spirvCode;

    std::unique_ptr<ShaderReflection> reflection;

    bool loaded = false;
    std::string compileError;

    // Pin IDs for stage connections
    int stageOutputPinId = -1;  // Output to pipeline node
    // Note: stageInputPinId removed - shaders no longer chain together

public:
    ShaderNode(int nodeId, VkShaderStageFlagBits shaderStage);

    void Draw() override;
    void DrawProperties() override;

    const char* GetTypeName() const override { return "Shader"; }

    bool LoadSourceCode(const std::string& path);
    bool LoadShader(const std::string& path);
    bool CompileShader();

    void PopulateFromReflection();
    void UpdateName();
};
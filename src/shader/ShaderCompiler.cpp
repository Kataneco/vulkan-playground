#include "ShaderCompiler.h"

#include <sstream>

namespace {
shaderc_shader_kind stageToShadercKind(VkShaderStageFlagBits stage) {
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return shaderc_vertex_shader;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return shaderc_fragment_shader;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return shaderc_geometry_shader;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return shaderc_compute_shader;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return shaderc_tess_control_shader;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return shaderc_tess_evaluation_shader;
        default:
            return shaderc_glsl_infer_from_source;
    }
}
}

ShaderCompiler::Result ShaderCompiler::compileGlsl(const std::string& source,
                                                   VkShaderStageFlagBits stage,
                                                   const std::string& sourceName,
                                                   const std::string& entryPoint,
                                                   const std::vector<std::filesystem::path>& includeDirs,
                                                   const std::vector<MacroDefinition>& defines) const {
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetSourceLanguage(shaderc_source_language_glsl);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);

    for (const auto& dir : includeDirs) {
        if (!dir.empty()) {
            options.AddIncludeDirectory(dir.string());
        }
    }

    for (const auto& define : defines) {
        options.AddMacroDefinition(define.name, define.value);
    }

    shaderc_shader_kind shaderKind = stageToShadercKind(stage);

    auto result = compiler.CompileGlslToSpv(source, shaderKind, sourceName.c_str(), entryPoint.c_str(), options);

    Result compileResult;
    compileResult.warningCount = result.GetNumWarnings();
    compileResult.errorCount = result.GetNumErrors();
    compileResult.message = result.GetErrorMessage();

    if (result.GetCompilationStatus() == shaderc_compilation_status_success) {
        compileResult.success = true;
        compileResult.spirv.assign(result.cbegin(), result.cend());
    }

    if (compileResult.success && compileResult.message.empty()) {
        std::ostringstream oss;
        if (compileResult.warningCount > 0) {
            oss << compileResult.warningCount << " warning(s) emitted";
        } else {
            oss << "Compilation succeeded.";
        }
        compileResult.message = oss.str();
    }

    return compileResult;
}


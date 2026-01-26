#include "ShaderCompiler.h"

std::vector<uint32_t> compileGLSL(const std::string& code, shaderc_shader_kind kind, std::string* error) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetSpirv(shaderc_spirv_version::shaderc_spirv_version_1_6);
    options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);
    options.SetSourceLanguage(shaderc_source_language::shaderc_source_language_glsl);
    options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, shaderc_env_version::shaderc_env_version_vulkan_1_3);
    options.SetWarningsAsErrors();
    options.SetGenerateDebugInfo();

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(code, kind, "shader", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success) {
        if (error != nullptr)
        {
            *error = module.GetErrorMessage();
        }
        return {};
    }

    return {module.cbegin(), module.cend()};
}

std::vector<uint32_t> compileHLSL(const std::string& code, shaderc_shader_kind kind, std::string* error) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetSpirv(shaderc_spirv_version::shaderc_spirv_version_1_6);
    options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);
    options.SetSourceLanguage(shaderc_source_language::shaderc_source_language_hlsl);
    options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, shaderc_env_version::shaderc_env_version_vulkan_1_3);
    options.SetWarningsAsErrors();
    options.SetGenerateDebugInfo();

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(code, kind, "shader", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success) {
        if (error != nullptr)
        {
            *error = module.GetErrorMessage();
        }
        return {};
    }

    return {module.cbegin(), module.cend()};
}

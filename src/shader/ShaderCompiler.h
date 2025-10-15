#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

class ShaderCompiler {
public:
    struct MacroDefinition {
        std::string name;
        std::string value;
    };

    struct Result {
        bool success = false;
        std::vector<uint32_t> spirv;
        std::string message;
        size_t warningCount = 0;
        size_t errorCount = 0;
    };

    Result compileGlsl(const std::string& source,
                       VkShaderStageFlagBits stage,
                       const std::string& sourceName,
                       const std::string& entryPoint,
                       const std::vector<std::filesystem::path>& includeDirs = {},
                       const std::vector<MacroDefinition>& defines = {}) const;

private:
    shaderc::Compiler compiler;

};


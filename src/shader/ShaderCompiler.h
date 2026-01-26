#pragma once
#include "volk.h"
#include <shaderc/shaderc.hpp>
#include <vector>
#include <string>

std::vector<uint32_t> compileGLSL(const std::string& code, shaderc_shader_kind kind, std::string* error = nullptr);
std::vector<uint32_t> compileHLSL(const std::string& code, shaderc_shader_kind kind, std::string* error = nullptr);

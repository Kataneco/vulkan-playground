#pragma once
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <memory>
#include <algorithm>

#define VK_NO_PROTOTYPES
#include "volk.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"

//#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include "spirv_reflect.h"

size_t hash_combine(size_t lhs, size_t rhs);

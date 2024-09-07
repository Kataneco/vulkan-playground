#include "VulkanUtils.h"

size_t hash_combine(size_t lhs, size_t rhs) {
    lhs ^= rhs + 0x517cc1b727220a95 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

std::string readFile(const char* filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if(!file.is_open()) throw std::runtime_error("Failed to open file");

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::string data(fileSize, ' ');

    file.seekg(0);
    file.read(data.data(), static_cast<long>(fileSize));
    file.close();

    return data;
}
#include "VulkanUtils.h"

size_t hash_combine(size_t lhs, size_t rhs) {
    lhs ^= rhs + 0x517cc1b727220a95 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

std::string readFile(const char* filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if(!file.is_open()) throw std::runtime_error("Failed to open file: "+std::string(filePath));

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::string data(fileSize, ' ');

    file.seekg(0);
    file.read(data.data(), static_cast<long>(fileSize));
    file.close();

    return data;
}

bool DescriptorSetLayoutData::operator==(const DescriptorSetLayoutData &other) const {
    if (other.bindings.size() != bindings.size()) {
        return false;
    } else {
        for (int i = 0; i < bindings.size(); i++) {
            if (other.bindings[i].binding != bindings[i].binding)
                return false;
            if (other.bindings[i].descriptorType != bindings[i].descriptorType)
                return false;
            if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
                return false;
            if (other.bindings[i].stageFlags != bindings[i].stageFlags)
                return false;
        }
        return true;
    }
}

VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutData::getCreateInfo() {
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = bindings.size();
    createInfo.pBindings = bindings.data();
    return createInfo;
}
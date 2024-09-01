#pragma once
#include "util/VulkanUtils.h"

#include "Resource.h"
#include "MemoryAllocator.h"

class Sampler : public Resource {
public:
    Sampler(VkDevice device, const VkSamplerCreateInfo &samplerInfo, const std::string &name);
    ~Sampler() override;

    void destroy() override;

    VkSampler getSampler() const { return sampler; }

private:
    VkSampler sampler;
};

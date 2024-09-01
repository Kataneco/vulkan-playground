#include "Sampler.h"

Sampler::Sampler(VkDevice device, const VkSamplerCreateInfo &samplerInfo, const std::string &name) : Resource(device, name) {
    vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
}

Sampler::~Sampler() {
    destroy();
}

void Sampler::destroy() {
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
}

#include "Sampler.h"

Sampler::Sampler(VkDevice device, const VkSamplerCreateInfo &samplerInfo, const std::string &name) : Resource(device, name) {
    vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
}

Sampler::~Sampler() {
    Sampler::destroy();
#ifdef KITTEN_MEMORY_DEBUG
    std::clog << "Destroyed sampler: " << name << std::endl;
#endif
}

void Sampler::destroy() {
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
}

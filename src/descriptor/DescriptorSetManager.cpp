#include "DescriptorSetManager.h"

VkDescriptorPool createPool(VkDevice device, const DescriptorAllocator::PoolSizes &poolSizes, int count, VkDescriptorPoolCreateFlags flags) {
    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(poolSizes.sizes.size());
    for (auto sz: poolSizes.sizes) {
        sizes.push_back({sz.first, uint32_t(sz.second*count)});
    }
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.maxSets = count;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);
    return descriptorPool;
}

void DescriptorAllocator::resetPools() {
    for (auto p: usedPools) {
        vkResetDescriptorPool(device, p, 0);
    }
    freePools = usedPools;
    usedPools.clear();
    currentPool = VK_NULL_HANDLE;
}

bool DescriptorAllocator::allocate(VkDescriptorSet *set, VkDescriptorSetLayout layout) {
    if (currentPool == VK_NULL_HANDLE) {
        currentPool = grabPool();
        usedPools.push_back(currentPool);
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pSetLayouts = &layout;
    allocInfo.descriptorPool = currentPool;
    allocInfo.descriptorSetCount = 1;

    VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
    bool needReallocate = false;

    switch (allocResult) {
        case VK_SUCCESS:
            //all good, return
            return true;
            break;
        case VK_ERROR_FRAGMENTED_POOL:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            //reallocate pool
            needReallocate = true;
            break;
        default:
            //unrecoverable error
            return false;
    }

    if (needReallocate) {
        //allocate a new pool and retry
        currentPool = grabPool();
        usedPools.push_back(currentPool);

        allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);

        //if it still fails then we have big issues
        if (allocResult == VK_SUCCESS) {
            return true;
        }
    }

    return false;
}

DescriptorAllocator::DescriptorAllocator(VkDevice device) : device(device) {}

DescriptorAllocator::~DescriptorAllocator() {
    destroy();
}

void DescriptorAllocator::destroy() {
    //delete every pool held
    for (auto p: freePools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    for (auto p: usedPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
}

VkDescriptorPool DescriptorAllocator::grabPool() {
    if (freePools.size() > 0) {
        VkDescriptorPool pool = freePools.back();
        freePools.pop_back();
        return pool;
    } else {
        return createPool(device, descriptorSizes, 1000, 0);
    }
}

DescriptorLayoutCache::DescriptorLayoutCache(VkDevice device) : device(device) {}

DescriptorLayoutCache::~DescriptorLayoutCache() {
    destroy();
}

void DescriptorLayoutCache::destroy() {
    //delete every descriptor layout held
    for (auto pair: layoutCache) {
        vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
    }
}

VkDescriptorSetLayout DescriptorLayoutCache::createDescriptorLayout(VkDescriptorSetLayoutCreateInfo *info) {
    DescriptorSetLayoutData layoutinfo;
    layoutinfo.bindings.reserve(info->bindingCount);
    bool isSorted = true;
    int32_t lastBinding = -1;
    for (uint32_t i = 0; i < info->bindingCount; i++) {
        layoutinfo.bindings.push_back(info->pBindings[i]);

        //check that the bindings are in strict increasing order
        if (static_cast<int32_t>(info->pBindings[i].binding) > lastBinding) {
            lastBinding = info->pBindings[i].binding;
        } else {
            isSorted = false;
        }
    }
    if (!isSorted) {
        std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(), [](VkDescriptorSetLayoutBinding &a, VkDescriptorSetLayoutBinding &b) {
            return a.binding < b.binding;
        });
    }

    if (layoutCache.contains(layoutinfo))
        return layoutCache[layoutinfo];

    VkDescriptorSetLayoutCreateInfo sortedInfo = layoutinfo.getCreateInfo();

    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(device, &sortedInfo, nullptr, &layout);
    layoutCache[layoutinfo] = layout;
    return layout;
}

DescriptorBuilder::DescriptorBuilder(DescriptorLayoutCache &cache, DescriptorAllocator &alloc) : cache(cache), alloc(alloc) {}

DescriptorBuilder& DescriptorBuilder::bind_buffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding newBinding{};
    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    bindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite{};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pBufferInfo = bufferInfo;
    newWrite.dstBinding = binding;

    writes.push_back(newWrite);
    return *this;
}

DescriptorBuilder& DescriptorBuilder::bind_image(uint32_t binding, VkDescriptorImageInfo *imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding newBinding{};
    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    bindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite{};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pImageInfo = imageInfo;
    newWrite.dstBinding = binding;

    writes.push_back(newWrite);
    return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet &set, VkDescriptorSetLayout &layout) {
    //build layout first
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pBindings = bindings.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());

    layout = cache.createDescriptorLayout(&layoutInfo);

    //allocate descriptor
    bool success = alloc.allocate(&set, layout);
    if (!success) return false;

    //write descriptor
    for (VkWriteDescriptorSet &w: writes) {
        w.dstSet = set;
    }

    vkUpdateDescriptorSets(alloc.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    return true;
}

bool DescriptorBuilder::build(VkDescriptorSet &set) {
    VkDescriptorSetLayout layout;
    return build(set, layout);
}

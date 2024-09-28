#include "StagingBufferManager.h"

StagingBufferManager::StagingBufferManager(Device &device, VkDeviceSize bufferSize) : device(device), stagingBufferSize(bufferSize) {
    createStagingBuffer();
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
    vkMapMemory(device, stagingBufferMemory, 0, stagingBufferSize, 0, &mappedData);
}

StagingBufferManager::~StagingBufferManager() {
    vkUnmapMemory(device, stagingBufferMemory);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    vkDestroySemaphore(device, transferCompleteSemaphore, nullptr);
    vkDestroyFence(device, transferFence, nullptr);
}

void StagingBufferManager::stageData(void *srcData, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dstOffset) {
    operationQueue.push({srcData, dstBuffer, size, dstOffset});
}

void StagingBufferManager::flush() {
    VkDeviceSize currentOffset = 0;
    std::vector<BufferCopyInfo> copies;

    while (!operationQueue.empty()) {
        auto &op = operationQueue.front();
        VkDeviceSize remainingSize = op.size;
        VkDeviceSize srcOffset = 0;

        while (remainingSize > 0) {
            VkDeviceSize availableSpace = stagingBufferSize - currentOffset;
            VkDeviceSize copySize = std::min(remainingSize, availableSpace);

            void *srcDataOffset = static_cast<char*>(op.srcData) + srcOffset;
            copyToStagingBuffer(srcDataOffset, copySize, currentOffset);
            copies.push_back({op.dstBuffer, {currentOffset, op.dstOffset + srcOffset, copySize}});

            currentOffset += copySize;
            remainingSize -= copySize;
            srcOffset += copySize;

            if (currentOffset == stagingBufferSize) {
                submitAndWait(copies);
                copies.clear();
                currentOffset = 0;
            }
        }

        operationQueue.pop();
    }

    if (!copies.empty()) {
        submitAndWait(copies);
    }
}

void StagingBufferManager::createStagingBuffer() {
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = stagingBufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer!");
    }

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate staging buffer memory!");
    }

    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);
}

void StagingBufferManager::createCommandPool() {
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = device.getTransferFamily();
    vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
}

void StagingBufferManager::createCommandBuffer() {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
}

void StagingBufferManager::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &transferCompleteSemaphore);
    vkCreateFence(device, &fenceInfo, nullptr, &transferFence);
}

void StagingBufferManager::copyToStagingBuffer(void *srcData, VkDeviceSize size, VkDeviceSize offset) {
    memcpy(static_cast<char*>(mappedData) + offset, srcData, static_cast<size_t>(size));
}

void StagingBufferManager::submitAndWait(const std::vector<BufferCopyInfo> &copies) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetCommandBuffer(commandBuffer, 0);
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    std::vector<VkBufferMemoryBarrier> preCopyBarriers;
    preCopyBarriers.reserve(copies.size());
    std::vector<VkBufferMemoryBarrier> postCopyBarriers;
    postCopyBarriers.reserve(copies.size());

    for (const auto& copy : copies) {
        VkBufferMemoryBarrier preCopyBarrier{};
        preCopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        preCopyBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        preCopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        preCopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        preCopyBarrier.dstQueueFamilyIndex = device.getTransferFamily();
        preCopyBarrier.buffer = copy.dstBuffer;
        preCopyBarrier.offset = copy.copyRegion.dstOffset;
        preCopyBarrier.size = copy.copyRegion.size;

        preCopyBarriers.push_back(preCopyBarrier);

        VkBufferMemoryBarrier postCopyBarrier{};
        postCopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        postCopyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        postCopyBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        postCopyBarrier.srcQueueFamilyIndex = device.getTransferFamily();
        postCopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        postCopyBarrier.buffer = copy.dstBuffer;
        postCopyBarrier.offset = copy.copyRegion.dstOffset;
        postCopyBarrier.size = copy.copyRegion.size;

        postCopyBarriers.push_back(postCopyBarrier);
    }

    if (!preCopyBarriers.empty()) {
        vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                static_cast<uint32_t>(preCopyBarriers.size()), preCopyBarriers.data(),
                0, nullptr);
    }

    for (const auto& copy : copies) {
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, copy.dstBuffer, 1, &copy.copyRegion);
    }

    if (!postCopyBarriers.empty()) {
        vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                static_cast<uint32_t>(postCopyBarriers.size()), postCopyBarriers.data(),
                0, nullptr);
    }

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    //submitInfo.signalSemaphoreCount = operationQueue.size() <= 1;
    //submitInfo.pSignalSemaphores = &transferCompleteSemaphore;

    vkResetFences(device, 1, &transferFence);

    if (vkQueueSubmit(device.getTransferQueue(), 1, &submitInfo, transferFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit transfer command buffer!");
    }

    vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
}
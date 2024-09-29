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

void StagingBufferManager::stageBufferData(void *srcData, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dstOffset) {
    operationQueue.push({srcData, dstBuffer, nullptr, size, dstOffset, {}, {}, {}, false});
}

void StagingBufferManager::stageImageData(void *srcData, VkImage dstImage, VkDeviceSize size, const VkImageSubresourceLayers &subresource, const VkOffset3D &offset, const VkExtent3D &extent) {
    operationQueue.push({srcData, nullptr, dstImage, size, 0, subresource, offset, extent, true});
}

void StagingBufferManager::flush() {
    VkDeviceSize currentOffset = 0;
    std::vector<BufferCopyInfo> bufferCopies;
    std::vector<ImageCopyInfo> imageCopies;

    while (!operationQueue.empty()) {
        auto &op = operationQueue.front();
        VkDeviceSize remainingSize = op.size;
        VkDeviceSize srcOffset = 0;

        while (remainingSize > 0) {
            VkDeviceSize availableSpace = stagingBufferSize - currentOffset;
            VkDeviceSize copySize = std::min(remainingSize, availableSpace);

            void *srcDataOffset = static_cast<char*>(op.srcData) + srcOffset;
            copyToStagingBuffer(srcDataOffset, copySize, currentOffset);

            if (op.isImage) {
                VkBufferImageCopy imageCopyRegion{};
                imageCopyRegion.bufferOffset = currentOffset;
                imageCopyRegion.bufferRowLength = 0;
                imageCopyRegion.bufferImageHeight = 0;
                imageCopyRegion.imageSubresource = op.imageSubresource;
                imageCopyRegion.imageOffset = op.imageOffset;
                imageCopyRegion.imageExtent = op.imageExtent;
                imageCopies.push_back({op.dstImage, imageCopyRegion});
            } else {
                bufferCopies.push_back({op.dstBuffer, {currentOffset, op.dstOffset + srcOffset, copySize}});
            }

            currentOffset += copySize;
            remainingSize -= copySize;
            srcOffset += copySize;

            if (currentOffset == stagingBufferSize) {
                submitAndWait(bufferCopies, imageCopies);
                bufferCopies.clear();
                imageCopies.clear();
                currentOffset = 0;
            }
        }

        operationQueue.pop();
    }

    if (!bufferCopies.empty() || !imageCopies.empty()) {
        submitAndWait(bufferCopies, imageCopies);
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

void StagingBufferManager::submitAndWait(const std::vector<BufferCopyInfo> &bufferCopies, const std::vector<ImageCopyInfo> &imageCopies) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetCommandBuffer(commandBuffer, 0);
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    std::vector<VkBufferMemoryBarrier> preCopyBufferBarriers;
    std::vector<VkImageMemoryBarrier> preCopyImageBarriers;
    std::vector<VkBufferMemoryBarrier> postCopyBufferBarriers;
    std::vector<VkImageMemoryBarrier> postCopyImageBarriers;

    // Set up pre-copy barriers
    for (const auto& copy : bufferCopies) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        barrier.dstQueueFamilyIndex = device.getTransferFamily();
        barrier.buffer = copy.dstBuffer;
        barrier.offset = copy.copyRegion.dstOffset;
        barrier.size = copy.copyRegion.size;
        preCopyBufferBarriers.push_back(barrier);
    }

    for (const auto& copy : imageCopies) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        barrier.dstQueueFamilyIndex = device.getTransferFamily();
        barrier.image = copy.dstImage;
        barrier.subresourceRange.aspectMask = copy.copyRegion.imageSubresource.aspectMask;
        barrier.subresourceRange.baseMipLevel = copy.copyRegion.imageSubresource.mipLevel;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = copy.copyRegion.imageSubresource.baseArrayLayer;
        barrier.subresourceRange.layerCount = copy.copyRegion.imageSubresource.layerCount;
        preCopyImageBarriers.push_back(barrier);
    }

    // Apply pre-copy barriers
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            static_cast<uint32_t>(preCopyBufferBarriers.size()), preCopyBufferBarriers.data(),
            static_cast<uint32_t>(preCopyImageBarriers.size()), preCopyImageBarriers.data()
    );

    // Perform copies
    for (const auto& copy : bufferCopies) {
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, copy.dstBuffer, 1, &copy.copyRegion);
    }

    for (const auto& copy : imageCopies) {
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, copy.dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy.copyRegion);
    }

    // Set up post-copy barriers
    for (const auto& copy : bufferCopies) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.srcQueueFamilyIndex = device.getTransferFamily();
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        barrier.buffer = copy.dstBuffer;
        barrier.offset = copy.copyRegion.dstOffset;
        barrier.size = copy.copyRegion.size;
        postCopyBufferBarriers.push_back(barrier);
    }

    for (const auto& copy : imageCopies) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = device.getTransferFamily();
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
        barrier.image = copy.dstImage;
        barrier.subresourceRange.aspectMask = copy.copyRegion.imageSubresource.aspectMask;
        barrier.subresourceRange.baseMipLevel = copy.copyRegion.imageSubresource.mipLevel;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = copy.copyRegion.imageSubresource.baseArrayLayer;
        barrier.subresourceRange.layerCount = copy.copyRegion.imageSubresource.layerCount;
        postCopyImageBarriers.push_back(barrier);
    }

    // Apply post-copy barriers
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            static_cast<uint32_t>(postCopyBufferBarriers.size()), postCopyBufferBarriers.data(),
            static_cast<uint32_t>(postCopyImageBarriers.size()), postCopyImageBarriers.data()
    );

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkResetFences(device, 1, &transferFence);

    if (vkQueueSubmit(device.getTransferQueue(), 1, &submitInfo, transferFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit transfer command buffer!");
    }

    vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
}
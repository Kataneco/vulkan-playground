#pragma once
#include "util/VulkanUtils.h"
#include "core/Device.h"

class StagingBufferManager {
public:
    struct StagingOperation {
        void *srcData;
        VkBuffer dstBuffer;
        VkDeviceSize size;
        VkDeviceSize dstOffset;
    };

    struct BufferCopyInfo {
        VkBuffer dstBuffer;
        VkBufferCopy copyRegion;
    };

    StagingBufferManager(Device &device, VkDeviceSize bufferSize = 16 * 1024 * 1024);
    ~StagingBufferManager();

    void stageData(void *srcData, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dstOffset = 0);
    void flush();

private:
    Device &device;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkDeviceSize stagingBufferSize;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore transferCompleteSemaphore;
    VkFence transferFence;
    std::queue<StagingOperation> operationQueue;
    void *mappedData = nullptr;

    void createStagingBuffer();
    void createCommandPool();
    void createCommandBuffer();
    void createSyncObjects();
    void copyToStagingBuffer(void *srcData, VkDeviceSize size, VkDeviceSize offset);
    void submitAndWait(const std::vector<BufferCopyInfo> &copies);
};
#pragma once
#include "util/VulkanUtils.h"
#include "core/Device.h"

class StagingBufferManager {
public:
    struct StagingOperation {
        void *srcData;
        VkBuffer dstBuffer;
        VkImage dstImage;
        VkDeviceSize size;
        VkDeviceSize dstOffset;
        VkImageSubresourceLayers imageSubresource;
        VkOffset3D imageOffset;
        VkExtent3D imageExtent;
        bool isImage;
    };

    struct BufferCopyInfo {
        VkBuffer dstBuffer;
        VkBufferCopy copyRegion;
    };

    struct ImageCopyInfo {
        VkImage dstImage;
        VkBufferImageCopy copyRegion;
    };

    StagingBufferManager(Device &device, VkDeviceSize bufferSize = 16 * 1024 * 1024);
    ~StagingBufferManager();

    void stageBufferData(void *srcData, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dstOffset = 0);
    void stageImageData(void *srcData, VkImage dstImage, VkDeviceSize size, const VkImageSubresourceLayers &subresource, const VkOffset3D &offset, const VkExtent3D &extent);
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
    void submitAndWait(const std::vector<BufferCopyInfo> &bufferCopies, const std::vector<ImageCopyInfo> &imageCopies);
};
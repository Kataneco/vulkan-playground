#include "ResourceBarrier.h"

void ResourceBarrier::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }

    barrier.srcAccessMask = getAccessMask(oldLayout);
    barrier.dstAccessMask = getAccessMask(newLayout);

    VkPipelineStageFlags sourceStage = getPipelineStage(oldLayout);
    VkPipelineStageFlags destinationStage = getPipelineStage(newLayout);

    vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
    );
}

void ResourceBarrier::bufferMemoryBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            1, &barrier,
            0, nullptr
    );
}

VkAccessFlags ResourceBarrier::getAccessMask(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return 0;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_ACCESS_HOST_WRITE_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        default:
            return 0;
    }
}

VkPipelineStageFlags ResourceBarrier::getPipelineStage(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_PIPELINE_STAGE_HOST_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
}
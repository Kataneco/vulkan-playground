#include "MeshRenderer.h"

MeshRenderer::MeshRenderer(const Mesh &mesh, const std::vector<Material> &materials) : mesh(mesh), materials(materials), enabled(true) {}

MeshRenderer::~MeshRenderer() {}

void MeshRenderer::render(VkCommandBuffer commandBuffer) {
    VkDeviceSize offset = 0;
    VkBuffer vertexBuffer = mesh.vertexBuffer->getBuffer();
    VkBuffer indexBuffer = mesh.indexBuffer->getBuffer();
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, offset, VK_INDEX_TYPE_UINT32);

    for (const auto& material : materials) {
        if (material.pipeline == VK_NULL_HANDLE || material.layout == VK_NULL_HANDLE) continue;

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);

        if (material.set != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.layout, 0, 1, &material.set, 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);
    }
}
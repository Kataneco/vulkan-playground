#pragma once
#include "util/VulkanUtils.h"

#include "Mesh.h"
#include "Material.h"

class MeshRenderer {
public:
    MeshRenderer(const Mesh &mesh, const std::vector<Material> &materials);
    ~MeshRenderer();

    void render(VkCommandBuffer commandBuffer);

private:
    bool enabled;
    Mesh mesh;
    std::vector<Material> materials;
};
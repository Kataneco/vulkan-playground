#pragma once
#include "util/VulkanUtils.h"

#include "resource/ResourceManager.h"
#include "resource/StagingBufferManager.h"
#include "resource/Buffer.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    bool operator==(const Vertex &other) const {
        return position == other.position && normal == other.normal && texCoord == other.texCoord;
    }

    static std::vector<VkVertexInputBindingDescription> bindings();
    static std::vector<VkVertexInputAttributeDescription> attributes();
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const &vertex) const {
            return std::hash<glm::vec3>()(vertex.position) ^ (std::hash<glm::vec3>()(vertex.normal) << 1) ^ (std::hash<glm::vec2>()(vertex.texCoord) << 2);
        }
    };
}

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::shared_ptr<Buffer> vertexBuffer;
    std::shared_ptr<Buffer> indexBuffer;

    static Mesh loadObj(const std::string &filePath);
    void pushMesh(ResourceManager& resourceManager, StagingBufferManager& stagingBufferManager);
};
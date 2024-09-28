#pragma once
#include "util/VulkanUtils.h"

#include "resource/ResourceManager.h"
#include "resource/StagingBufferManager.h"
#include "resource/Buffer.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;

    bool operator==(const Vertex &other) const {
        return position == other.position && normal == other.normal && texCoords == other.texCoords;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const &vertex) const {
            return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoords) << 1);
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
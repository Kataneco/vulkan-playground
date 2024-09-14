#pragma once
#include "util/VulkanUtils.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;

    bool operator==(const Vertex &other) const {
        return position == other.position && normal == other.normal && color == other.color;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const &vertex) const {
            return ((hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^ (hash<glm::vec3>()(vertex.color) << 1);
        }
    };
}

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    static Mesh loadObj(const std::string &filePath);
};
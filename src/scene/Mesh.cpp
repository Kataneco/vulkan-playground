#include "Mesh.h"

//TODO: This is a testing function, replace later
Mesh Mesh::loadObj(const std::string& filePath) {
    Mesh mesh{};
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str()))
        throw std::runtime_error(warn + err);

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto &shape: shapes) {
        for (const auto &index: shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.normal = {
                    attrib.normals[3 * index.vertex_index + 0],
                    attrib.normals[3 * index.vertex_index + 1],
                    attrib.normals[3 * index.vertex_index + 2]
            };

            vertex.texCoords = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
            };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.push_back(vertex);
            }

            mesh.indices.push_back(uniqueVertices[vertex]);
        }
    }
    return mesh;
}

void Mesh::pushMesh(ResourceManager &resourceManager, StagingBufferManager& stagingBufferManager) {
    vertexBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = vertices.size() * sizeof(vertices[0]), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO});
    indexBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = indices.size() * sizeof(indices[0]), .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO});
    stagingBufferManager.stageData(vertices.data(), vertexBuffer->getBuffer(), vertices.size()*sizeof(vertices[0]));
    stagingBufferManager.stageData(indices.data(), indexBuffer->getBuffer(), indices.size()*sizeof(indices[0]));
    stagingBufferManager.flush();
}

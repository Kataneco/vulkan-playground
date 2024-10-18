#include "Mesh.h"

std::vector<VkVertexInputBindingDescription> Vertex::bindings() {
    return {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}};
}

std::vector<VkVertexInputAttributeDescription> Vertex::attributes() {
    return {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
            {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord)}
    };
}

Mesh Mesh::loadObj(const std::string& filePath) {
    Mesh mesh{};
    tinyobj::ObjReaderConfig reader_config;
    reader_config.triangulate = true;
    reader_config.triangulation_method = "earcut";

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(filePath, reader_config)) {
        if (!reader.Error().empty()) {
            throw std::runtime_error(reader.Error());
        }
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning() << std::endl;
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    mesh.vertices.reserve(attrib.vertices.size() / 3);
    uniqueVertices.reserve(attrib.vertices.size() / 3);
    for (const auto &shape: shapes) {
        mesh.indices.reserve(mesh.indices.size() + shape.mesh.indices.size());
        for (const auto &index: shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                };
            }

            if (index.texcoord_index >= 0) {
                vertex.texCoord = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.push_back(vertex);
            }

            mesh.indices.push_back(uniqueVertices[vertex]);
        }
    }

    mesh.vertices.shrink_to_fit();
    mesh.indices.shrink_to_fit();

    return mesh;
}

void Mesh::pushMesh(ResourceManager &resourceManager, StagingBufferManager& stagingBufferManager) {
    vertexBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = vertices.size() * sizeof(vertices[0]), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO});
    indexBuffer = resourceManager.createBuffer({.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = indices.size() * sizeof(indices[0]), .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}, {.usage = VMA_MEMORY_USAGE_AUTO});
    stagingBufferManager.stageBufferData(vertices.data(), vertexBuffer->getBuffer(), vertices.size()*sizeof(vertices[0]));
    stagingBufferManager.stageBufferData(indices.data(), indexBuffer->getBuffer(), indices.size()*sizeof(indices[0]));
    stagingBufferManager.flush();
}

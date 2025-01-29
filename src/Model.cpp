#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "Types.h"
#include "Model.h"
#include "VulkanHelper.h"
#include "VulkanDeviceManager.h"

#include <unordered_map>
#include <cmath>

Model::~Model()
{
    vkDestroyBuffer(VK.Device(), m_indexBuffer, nullptr);
    vkFreeMemory(VK.Device(), m_indexBufferMemory, nullptr);

    vkDestroyBuffer(VK.Device(), m_vertexBuffer, nullptr);
    vkFreeMemory(VK.Device(), m_vertexBufferMemory, nullptr);
}

void ComputeSmoothNormals(const std::vector<uint32_t>& indices, std::vector<Vertex>& vertices)
{
    // Initialize all normals to zero
    for (auto& vertex : vertices)
    {
        vertex.normal = glm::vec3(0.0f);
    }

    // Accumulate face normals
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        glm::vec3 v0 = vertices[i0].pos;
        glm::vec3 v1 = vertices[i1].pos;
        glm::vec3 v2 = vertices[i2].pos;

        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

        // Add to vertex normals
        vertices[i0].normal += normal;
        vertices[i1].normal += normal;
        vertices[i2].normal += normal;
    }

    // Normalize each normal
    for (auto& vertex : vertices)
    {
        vertex.normal = glm::normalize(vertex.normal);
    }
}

void Model::Load(std::string fileNmae)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fileNmae.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    m_boundaries[0] = attrib.vertices[0];
    m_boundaries[1] = attrib.vertices[0];
    m_boundaries[2] = attrib.vertices[1];
    m_boundaries[3] = attrib.vertices[1];
    m_boundaries[4] = attrib.vertices[2];
    m_boundaries[5] = attrib.vertices[2];

#define USE_POSITION_AS_COLOR 0

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

#if USE_POSITION_AS_COLOR
            vertex.color = { vertex.pos[0], vertex.pos[1], vertex.pos[2] };
#else
            vertex.color =
            {
                attrib.colors[3 * index.vertex_index + 0],
                attrib.colors[3 * index.vertex_index + 1],
                attrib.colors[3 * index.vertex_index + 2]
            };
#endif // USE_POSITION_AS_COLOR

            vertex.pos =
            {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            m_boundaries[0] = std::min(m_boundaries[0], vertex.pos[0]);
            m_boundaries[1] = std::max(m_boundaries[1], vertex.pos[0]);
            m_boundaries[2] = std::min(m_boundaries[2], vertex.pos[1]);
            m_boundaries[3] = std::max(m_boundaries[3], vertex.pos[1]);
            m_boundaries[4] = std::min(m_boundaries[4], vertex.pos[2]);
            m_boundaries[5] = std::max(m_boundaries[5], vertex.pos[2]);

            if (index.texcoord_index != -1)
            {
                vertex.texCoord =
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            m_indices.push_back(m_vertices.size());
            m_vertices.push_back(vertex);
        }
    }

    ComputeSmoothNormals(m_indices, m_vertices);

    float maxLength = std::max(std::max((m_boundaries[1] - m_boundaries[0]) / 2,
                                        (m_boundaries[3] - m_boundaries[2]) / 2),
                               (m_boundaries[5] - m_boundaries[4]) / 2);
    m_normalizeMatrix = glm::mat4(1.0f);
    m_normalizeMatrix = glm::scale(m_normalizeMatrix, glm::vec3(1.0f / maxLength));
    m_normalizeMatrix = glm::translate(m_normalizeMatrix,
                                       -glm::vec3((m_boundaries[0] + m_boundaries[1]) / 2,
                                                  (m_boundaries[2] + m_boundaries[3]) / 2,
                                                  (m_boundaries[4] + m_boundaries[5]) / 2));

#if USE_POSITION_AS_COLOR
    for (auto& vertex : m_vertices)
    {
        vertex.color[0] = (vertex.color[0] - m_boundaries[0]) / (m_boundaries[1] - m_boundaries[0]);
        vertex.color[1] = (vertex.color[1] - m_boundaries[2]) / (m_boundaries[3] - m_boundaries[2]);
        vertex.color[2] = (vertex.color[2] - m_boundaries[4]) / (m_boundaries[5] - m_boundaries[4]);
    }
#endif // USE_POSITION_AS_COLOR
}

void Model::CreateVertexBuffer()
{
    // Calculate the size of the buffer required to store all vertex data
    VkDeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

    // Staging buffer and its memory to upload data from the host (CPU) to the device (GPU)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingBufferMemory);

    // Map the staging buffer memory to CPU-accessible address space and copy vertex data
    void* data;
    VkDevice device = VK.Device();
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create a GPU-local buffer for vertex data
    CreateBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_vertexBuffer,
                 m_vertexBufferMemory);

    // Copy data from the staging buffer to the GPU-local vertex buffer
    CopyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

    // Clean up the staging buffer and its memory after data has been transferred
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Model::CreateIndexBuffer()
{
    // Calculate the size of the buffer required to store all index data
    VkDeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

    // Staging buffer and its memory to upload data from the host (CPU) to the device (GPU)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingBufferMemory);

    // Map the staging buffer memory to CPU-accessible address space and copy index data
    void* data;
    VkDevice device = VK.Device();
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create a GPU-local buffer for index data
    CreateBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_indexBuffer,
                 m_indexBufferMemory);

    // Copy data from the staging buffer to the GPU-local index buffer
    CopyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

    // Clean up the staging buffer and its memory after data has been transferred
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

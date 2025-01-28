#ifndef __MODEL_H__
#define __MODEL_H__

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <vector>
#include <string>

#include "Types.h"

enum class EModelType
{
    ChessBoard = 0,
    ChessPiece = 1,
};

enum class ERenderMode : int
{
    Color   = 0,
    Texture = 1
};

struct ModelPushConstants
{
    struct ModelVsPushConstants
    {
        glm::mat4 model;
        glm::mat4 normailzeMatrix;
    } vs;

    struct ModelFsPushConstants
    {
        ERenderMode renderMode;
    } fs;
};

class Model
{
public:
    Model(std::string fileName, EModelType type) : m_type(type)
    {
        Load(fileName);
    }

    ~Model();

    size_t    Indices()         const { return m_indices.size(); }
    glm::mat4 NormalizeMatrix() const { return m_normalizeMatrix; }
    glm::mat4 ModelMatrix()     const { return m_modelMatrix; }

    VkBuffer VertexBuffer()
    {
        if (m_vertexBuffer == VK_NULL_HANDLE)
        {
            CreateVertexBuffer();
        }
        return m_vertexBuffer;
    }

    VkBuffer IndexBuffer()
    {
        if (m_indexBuffer == VK_NULL_HANDLE)
        {
            CreateIndexBuffer();
        }
        return m_indexBuffer;
    }

    void Scale(glm::vec3 scale)
    {
        m_modelMatrix = glm::scale(m_modelMatrix, scale);
    }

    void Translate(glm::vec3 tranlation)
    {
        m_modelMatrix = glm::translate(m_modelMatrix, tranlation);
    }

    void Rotate(float degree, glm::vec3 axis)
    {
        m_modelMatrix = glm::rotate(m_modelMatrix, glm::radians(degree), axis);
    }

    void RescaleNormalizeMatrix(float scale)
    {
        m_normalizeMatrix = glm::scale(m_normalizeMatrix, glm::vec3(MaxScale()));
        m_normalizeMatrix = glm::scale(m_normalizeMatrix, glm::vec3(scale));
    }

    float MaxScale()
    {
        return std::max(std::max((m_boundaries[1] - m_boundaries[0]),
                                 (m_boundaries[3] - m_boundaries[2])),
                        (m_boundaries[5] - m_boundaries[4])) / 2;
    }

    ERenderMode RenderMode()
    {
        ERenderMode renderMode = ERenderMode::Color;

        switch (m_type)
        {
            case EModelType::ChessPiece:
                renderMode = ERenderMode::Color;
                break;
            case EModelType::ChessBoard:
                renderMode = ERenderMode::Texture;
                break;
            default:
                assert(false);
                break;
        }

        return renderMode;
    }

private:
    void CreateIndexBuffer();
    void CreateVertexBuffer();
    void Load(std::string fileName);

    EModelType m_type;

    glm::mat4               m_modelMatrix = glm::mat4(1.0f);
    std::vector<Vertex>     m_vertices;
    std::vector<uint32_t>   m_indices;
    float                   m_boundaries[6] = {};
    glm::mat4               m_normalizeMatrix = glm::mat4(1.0f);

    VkBuffer        m_vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory  m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer        m_indexBuffer        = VK_NULL_HANDLE;
    VkDeviceMemory  m_indexBufferMemory  = VK_NULL_HANDLE;
};

#endif // __MODEL_H__
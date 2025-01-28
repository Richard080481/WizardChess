#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(push_constant) uniform constants
{
    // vs
    layout(offset = 0)  mat4 model;
    layout(offset = 64) mat4 normailzeMatrix;

    ///@note fs
    ///      layout(offset = 128) int renderMode;
} pushConstant;

void main()
{
    gl_Position = ubo.proj * ubo.view * pushConstant.model * pushConstant.normailzeMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}

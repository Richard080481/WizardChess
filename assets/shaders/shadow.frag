#version 450

layout(push_constant) uniform PushConstants
{
    mat4 model;
    mat4 normailzeMatrix;
    bool isWhite;
    bool useTexture;
} pc;

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform UniformBufferObjectFs
{
    vec3 lightPos;
    vec3 lightColor;
    vec3 cameraPos;
} ubo;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPosition;

void main()
{
}
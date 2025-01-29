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
    layout(offset = 0)   mat4 world;
    layout(offset = 64)  mat4 model;
    layout(offset = 128) mat4 normailzeMatrix;
    layout(offset = 192) int  isBlack;

    ///@note fs
    ///      layout(offset = 196) int renderMode;
} pushConstant;

void main()
{
    gl_Position = ubo.proj * ubo.view * pushConstant.world * pushConstant.model * pushConstant.normailzeMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragColor.rgb = fragColor.rgb * 0.8 + 0.2;
    if (pushConstant.isBlack != 0)
    {
        fragColor.rgb *= 0.2f;
    }
    fragTexCoord = inTexCoord;
}

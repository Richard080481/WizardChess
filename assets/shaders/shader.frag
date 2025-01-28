#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform constants
{
    ///@note vs
    ///      layout(offset = 0)  mat4 model;
    ///      layout(offset = 64) mat4 normailzeMatrix;

    // fs
    layout(offset = 128) int renderMode;
} pushConstant;

void main()
{
    if (pushConstant.renderMode == 0)
    {
        outColor = vec4(fragColor, 1.0);
    }
    else if (pushConstant.renderMode == 1)
    {
        outColor = texture(texSampler, fragTexCoord);
    }
}

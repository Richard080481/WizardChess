#version 450

layout(push_constant) uniform PushConstants
{
    mat4 model;
    mat4 normailzeMatrix;
    vec3 color;
    bool useTexture;
} pc;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    if (pc.useTexture)
    {
        outColor = texture(texSampler, fragTexCoord);
    }
    else
    {
        // outColor = vec4(pc.color, 1.0);
        vec3 v3Normal = fragNormal * 0.5 + 0.5;
    }
}
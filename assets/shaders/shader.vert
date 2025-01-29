#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragPosition;

layout(binding = 0) uniform UniformBufferObjectVs
{
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform constants
{
    ///@note VS part
    layout(offset = 0)   mat4 world;
    layout(offset = 64)  mat4 model;
    layout(offset = 128) mat4 normalizeMatrix;
    layout(offset = 192) int  isBlack;

    ///@note FS part
    ///      layout(offset = 196) int renderMode;
} pushConstant;

void main()
{
    mat4 modelView = ubo.view * pushConstant.world * pushConstant.model * pushConstant.normalizeMatrix;

    // Transform position
    fragPosition = vec3(modelView * vec4(inPosition, 1.0));
    gl_Position = ubo.proj * vec4(fragPosition, 1.0);

    // Transform normal properly (important for lighting)
    fragNormal = normalize(mat3(transpose(inverse(modelView))) * inNormal);

    // Apply color modification
    fragColor = inColor;
    fragColor.rgb = fragColor.rgb * 0.8 + 0.2;
    if (pushConstant.isBlack != 0)
    {
        fragColor.rgb *= 0.05f;
    }

    // Texture coordinates
    fragTexCoord = inTexCoord;
}

#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    mat4 lightView;
    mat4 lightProj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPosition;
layout(location = 4) out vec4 lightViewPosition;

//push constants block
layout( push_constant ) uniform constants
{
    mat4 model;
    mat4 normailzeMatrix;
    bool isWhite;
    bool useTexture;
} pushConstant;

void main()
{
    mat4 normalizedModelMatrix = pushConstant.model * pushConstant.normailzeMatrix;
    vec4 position = ubo.view * normalizedModelMatrix * vec4(inPosition, 1.0);
    fragPosition = vec3(position);
    gl_Position = ubo.proj * position;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = normalize(mat3(transpose(inverse(ubo.view * pushConstant.model * pushConstant.normailzeMatrix))) * inNormal);
    lightViewPosition = ubo.lightProj * ubo.lightView * normalizedModelMatrix * vec4(inPosition, 1.0);
}
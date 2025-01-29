#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform UniformBufferObjectFs
{
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
} ubo;

layout(binding = 2) uniform sampler2D texSampler;

layout(push_constant) uniform constants
{
    ///@note vs
    ///      layout(offset = 0)   mat4 world;
    ///      layout(offset = 64)  mat4 model;
    ///      layout(offset = 128) mat4 normalizeMatrix;
    ///      layout(offset = 192) int  isBlack;

    ///@note fs
    layout(offset = 196) int renderMode;
} pushConstant;

void main()
{
    if (pushConstant.renderMode == 0)
    {
        vec3 N = normalize(fragNormal);
        vec3 L = normalize(ubo.lightPos - fragPosition);
        vec3 V = normalize(ubo.viewPos - fragPosition);
        vec3 R = reflect(-L, N);

        // Ambient lighting
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * ubo.lightColor;

        // Diffuse lighting (Lambertian reflection)
        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * ubo.lightColor;

        // Specular lighting (Phong reflection model)
        float specularStrength = 0.5;
        float shininess = 32.0;
        float spec = pow(max(dot(V, R), 0.0), shininess);
        vec3 specular = specularStrength * spec * ubo.lightColor;

        vec3 lighting = ambient + diffuse + specular;

        // Color Mode - Apply lighting to vertex color
        outColor = vec4(lighting * fragColor, 1.0);
    }
    else if (pushConstant.renderMode == 1)
    {
        outColor = texture(texSampler, fragTexCoord);
    }
}

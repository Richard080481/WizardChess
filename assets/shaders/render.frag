#version 450

layout(push_constant) uniform PushConstants
{
    mat4 model;
    mat4 normailzeMatrix;
    bool isWhite;
    bool useTexture;
} pc;

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 3) uniform sampler2D texSamplerShadow;

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
layout(location = 4) in vec4 lightViewPosition;

layout(location = 0) out vec4 outColor;

void main()
{
    if (pc.useTexture)
    {
        outColor = texture(texSampler, fragTexCoord);
    }
    else
    {
        vec3 N = normalize(fragNormal);
        vec3 L = normalize(ubo.lightPos - fragPosition);
        vec3 V = normalize(ubo.cameraPos - fragPosition);
        vec3 R = reflect(-L, N);

        // Ambient lighting
        float ambientStrength = 0.01;
        vec3 ambient = ambientStrength * ubo.lightColor;

        // Diffuse lighting (Lambertian reflection)
        float diffuseStrength = 1.0;
        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * ubo.lightColor * diffuseStrength;

        // Specular lighting (Phong reflection model)
        float specularStrength = 100;
        float shininess = 32.0;
        float spec = pow(max(dot(V, R), 0.0), shininess);
        vec3 specular = specularStrength * spec * ubo.lightColor;

        vec3 lighting = ambient + diffuse + specular;
        vec3 color = fragColor;

        if (!pc.isWhite)
        {
            color = color * 0.03;
        }
        // Color Mode - Apply lighting to vertex color
        outColor = vec4(lighting * color, 1.0);
    }

    vec3 p = lightViewPosition.xyz / lightViewPosition.w;
    outColor *= texture(texSamplerShadow, p.xy).r > p.z ? 1.0f : 0.0f;
}
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

const vec2 POISSON_8[8] = vec2[](
    vec2(-0.326, -0.406),
    vec2(-0.840, -0.074),
    vec2(-0.696,  0.457),
    vec2(-0.203,  0.621),
    vec2( 0.962, -0.195),
    vec2( 0.473, -0.480),
    vec2( 0.519,  0.767),
    vec2( 0.185, -0.893)
);

float ShadowPoissonPCF(
    sampler2D shadowMap,
    vec2 uv,
    float depthRef,
    float radiusPx
){
    ivec2 sz = textureSize(shadowMap, 0);
    vec2 texel = 1.0 / vec2(sz);

    float sum = 0.0;
    for (int i = 0; i < 8; ++i)
    {
        vec2 offset = POISSON_8[i] * radiusPx * texel;
        float depthMap = texture(shadowMap, uv + offset).r;
        sum += (depthRef <= depthMap) ? 1.0 : 0.0;
    }
    return sum / 8.0;
}


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

    vec3 ndc = lightViewPosition.xyz / lightViewPosition.w; // x,y in [-1,1]
    vec2 uv  = ndc.xy * 0.5 + 0.5;                          // -> [0,1]

    float depthRef = ndc.z;

    float NoL = clamp(dot(normalize(fragNormal), normalize(ubo.lightPos - fragPosition)), 0.0, 1.0);
    float bias = max(0.0005, 0.002 * (1.0 - NoL));
    depthRef -= bias;

    float radiusPx = 2.0;
    float shadow = ShadowPoissonPCF(texSamplerShadow, uv, depthRef, radiusPx);

    outColor.rgb *= shadow;
}
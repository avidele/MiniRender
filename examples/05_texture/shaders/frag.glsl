#version 450

layout(binding = 1) uniform sampler2D texSampler; // Texture sampler

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord; // Input texture coordinates from vertex shader

layout(location = 0) out vec4 outColor;

void main() {
    // Sample the texture using the texture coordinates
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Output the sampled texture color
    // You could also blend it with fragColor: outColor = texColor * vec4(fragColor, 1.0);
    outColor = texColor;
}
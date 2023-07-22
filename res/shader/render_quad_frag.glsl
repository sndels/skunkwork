#version 410

#include "uniforms.glsl"

uniform sampler2D uQuad;

out vec4 fragColor;

void main() {
    // Avoid nags if these aren't used
    if (uTime < -1 || uRes.x < -1)
        discard;

    vec2 texCoord = gl_FragCoord.xy / uRes;

    fragColor = texture(uQuad, texCoord);
}

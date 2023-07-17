#version 410

#include "uniforms.glsl"
#include "noise.glsl"

uniform vec3 dColor;
uniform float uMultiplier;

uniform sampler2D uScenePingColorDepth;
uniform sampler2D uScenePongColorDepth;

out vec4 fragColor;

void main()
{
    vec4 ping = texture(uScenePingColorDepth, gl_FragCoord.xy / uRes);
    vec4 pong = texture(uScenePongColorDepth, gl_FragCoord.xy / uRes);

    vec3 color = vec3(0);
    if (fbm(vec3(gl_FragCoord.xy * 0.01, uTime), 0.4, 4) > 0.8)
    {
        color = ping.rgb;
    } else {
        color = pong.rgb;
    }
    fragColor = vec4(color, 1);
}

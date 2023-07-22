#version 410

#include "uniforms.glsl"

uniform vec3 dColor;

out vec4 fragColor;

void main()
{
    // Avoid nags if these aren't used
    if (uTime < -1 || uRes.x < -1)
        discard;

    vec2 uv = gl_FragCoord.xy*2.0 / uRes.xy - 1.0;

    vec2 rOff = vec2(sin(uTime*0.432552), cos(uTime*0.127652));
    vec2 gOff = vec2(sin(uTime*0.245313), cos(uTime*0.628314));
    vec2 bOff = vec2(sin(uTime*0.120647), cos(uTime*0.394022));

    vec3 color = dColor + vec3(
        0.5+0.5*sin(length(uv + rOff)*3.4 + 10.0*cos(uTime * 1.0)),
        0.5+0.5*sin(length(uv + gOff)*2.9 + 10.0*cos(uTime * 1.2)),
        0.5+0.5*sin(length(uv + bOff)*1.3 + 10.0*cos(uTime * 0.9))
    );

    fragColor = vec4(color, 1);
}

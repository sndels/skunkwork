#version 410

#define PI 3.14159265
#define saturate(x) clamp(x, 0.0, 1.0)

#include "uniforms.glsl"
#include "shading.glsl"
#include "tonemap.glsl"

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 worldNormal;

uniform vec3 uEye;

out vec4 outColor;

// TODO: Uniform inputs
//vec3 light_dir = vec3(-1, -1, -1);
vec3 light_dir = vec3(1, 1, -1);
vec3 light_int = vec3(4);

void main()
{
    Material m;
    m.albedo = vec3(1);
    m.metallic = 0;
    m.roughness = 1;

    vec3 v = normalize(uEye - worldPos);
    vec3 l = -normalize(light_dir);
    vec3 color = light_int * evalBRDF(normalize(worldNormal), v, l, m);
    color += vec3(0.2, 0.2, 0.5);

    outColor = vec4(tonemap(color), 1.f);
}

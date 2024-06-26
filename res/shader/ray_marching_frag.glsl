#version 430

#include "hg_sdf.glsl"
#include "noise.glsl"
#include "shading.glsl"
#include "tonemap.glsl"
#include "uniforms.glsl"

out vec4 fragColor;

#define INF (1.0 / 0.0)

// Returns distance to hit and material index
vec2 scene(vec3 p)
{
    vec2 h = vec2(INF);

    {
        vec3 pp = p;
        pR(pp.xz, uTime);
        pR(pp.yz, uTime);
        float d = fBox(pp, vec3(1));
        h = d < h.x ? vec2(d, 0) : h;
    }

    return h;
}

// Naive sphere tracing
vec2 march(vec3 ro, vec3 rd, float prec, float tMax, int iMax)
{
    vec2 t = vec2(0.001, 0);
    for (int i = 0; i < iMax; ++i)
    {
        vec2 h = scene(ro + rd * t.x);
        if (h.x < prec || t.x > tMax)
            break;
        t.x += h.x;
        t.y = h.y;
    }
    if (t.x > tMax)
        t.x = INF;
    return t;
}

vec3 shade(vec3 p, vec3 n, vec3 v, float m) { return vec3(1); }

vec3 normal(vec3 p)
{
    vec3 e = vec3(0.0001, 0, 0);
    vec3 n = vec3(
        scene(vec3(p + e.xyy)).x - scene(vec3(p - e.xyy)).x,
        scene(vec3(p + e.yxy)).x - scene(vec3(p - e.yxy)).x,
        scene(vec3(p + e.yyx)).x - scene(vec3(p - e.yyx)).x);
    return normalize(n);
}

vec3 rayDir(vec2 px)
{
    // Neutral camera ray (+Z)
    vec2 uv = px / uRes.xy;          // uv
    uv -= 0.5;                       // origin at center
    uv /= vec2(uRes.y / uRes.x, 1);  // fix aspect ratio
    return normalize(vec3(uv, 0.7)); // pull ray
}

vec3 lookAt(vec3 eye, vec3 target, vec3 viewRay)
{
    vec3 up = vec3(0, 1, 0);
    vec3 fwd = normalize(target - eye);
    vec3 right = normalize(cross(up, fwd));
    vec3 newUp = normalize(cross(fwd, right));

    return mat3(
               right.x, right.y, right.z, newUp.x, newUp.y, newUp.z, fwd.x,
               fwd.y, fwd.z) *
           viewRay;
}

uniform vec3 dCamPos;
uniform vec3 dCamDir;
uniform vec3 dCamTarget;

void main()
{
    // Avoid nags if these aren't used
    if (uTime < -1 || uRes.x < -1 && uAspectRatio < -1)
        discard;

    // Generate camera ray
    vec3 rd = rayDir(gl_FragCoord.xy);
    vec3 ro = vec3(0, 0, -3) - dCamPos;
    vec3 target = vec3(0, 1, 1);

    // Look at target or raw pitch/yaw angles
    rd = lookAt(ro, dCamTarget, rd);
    //   pR(rd.yz, dCamDir.y);
    //   pR(rd.xz, dCamDir.x);

    // Trace them spheres
    vec2 t = march(ro, rd, 0.001, 128, 256);
    if (t.x > 128)
    {
        fragColor = vec4(0);
        return;
    }

    // Get hit parameters
    vec3 p = ro + rd * t.x;
    vec3 n = normal(p);
    vec3 v = -rd;
    float m = t.y;

    // Shade
    vec3 color = shade(p, n, v, m);

    // Color the pixel
    fragColor = vec4(pow(color, vec3(1 / 2.2)), 1);
}

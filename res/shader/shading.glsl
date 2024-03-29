struct Material
{
    vec3 albedo;
    float metallic;
    float roughness;
};

// Lambert diffuse term
vec3 lambertBRFD(vec3 c_diff) { return c_diff / PI; }

// GGX distribution function
float ggx(float NoH, float alpha)
{
    // Match gltf spec
    float a2 = alpha * alpha;

    float denom = NoH * NoH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Schlick fresnel function
vec3 schlickFresnel(float VoH, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - VoH, 5.0);
}

// Schlick-GGX geometry function
float schlick_ggx(float NoL, float NoV, float alpha)
{
    float k = alpha + 1.0;
    k *= k * 0.125;
    float gl = NoL / (NoL * (1.0 - k) + k);
    float gv = NoV / (NoV * (1.0 - k) + k);
    return gl * gv;
}

// Evaluate the Cook-Torrance specular BRDF
vec3 cookTorranceBRDF(
    float NoL, float NoV, float NoH, float VoH, vec3 f0, float roughness)
{
    // Match gltf spec
    float alpha = roughness * roughness;

    float D = ggx(NoH, alpha);
    vec3 F = schlickFresnel(VoH, f0);
    float G = schlick_ggx(NoL, NoV, alpha);

    float denom = 4.0 * NoL * NoV + 0.0001;
    return D * F * G / denom;
}

// Evaluate combined diffuse and specular BRDF
vec3 evalBRDF(vec3 n, vec3 v, vec3 l, Material m)
{
    // Common dot products
    vec3 h = normalize(v + l);
    float NoV = saturate(dot(n, v));
    float NoL = saturate(dot(n, l));
    float NoH = saturate(dot(n, h));
    float VoH = saturate(dot(v, h));

    // Use standard approximation of default fresnel
    vec3 f0 = mix(vec3(0.04), m.albedo, m.metallic);

    // Match glTF spec
    vec3 c_diff = mix(m.albedo * (1 - 0.04), vec3(0), m.metallic);

    return (lambertBRFD(c_diff) +
            cookTorranceBRDF(NoL, NoV, NoH, VoH, f0, m.roughness)) *
           NoL;
}

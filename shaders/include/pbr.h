// -*- mode: glsl; -*-

#ifndef PBR_H
#define PBR_H

#include "types.h"
#include "maths.h"

// "Sampling the GGX Distribution of Visible Normals", http://jcgt.org/published/0007/04/01/
// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
vec3 sampleGGXVNDF(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1,0,0);
	vec3 T2 = cross(Vh, T1);
	// Section 4.2: parameterization of the projected area
	float r = sqrt(U1);
	float phi = 2.0 * PI * U2;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + Vh.z);
	t2 = (1.0 - s)*sqrt(1.0 - t1*t1) + s*t2;
	// Section 4.3: reprojection onto hemisphere
	vec3 Nh = t1*T1 + t2*T2 + sqrt(max(0.0, 1.0 - t1*t1 - t2*t2))*Vh;
	// Section 3.4: transforming the normal back to the ellipsoid configuration
	vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
	return Ne;
}

// importance sample the ggx visible normals
float3 sample_ggx_vndf(float3 V, float roughness, inout uint rng_seed)
{
    float u1 = random_float_01(rng_seed);
    float u2 = random_float_01(rng_seed);
    return sampleGGXVNDF(V, roughness, roughness, u1, u2);
}

// Smith's GGX shadow masking function, "PBR Diffuse Lighting for GGX+SmithMicrosurfaces"
// Input V: view direction
// Input N: surface normal
// Output: visible microfacets
float smith_ggx_g1(float3 V, float3 N, float roughness)
{
    float NdotV = dot(N, V);
    return            (2.0 * NdotV)
        / //----------------------------------//
        (NdotV * (2.0 - roughness) + roughness);
}


// Smith's GGX shadow masking function, "PBR Diffuse Lighting for GGX+SmithMicrosurfaces"
// Input V: view direction
// Input L: light direction
// Input N: surface normal
// Input roughness
// Output: visible microfacets
float smith_ggx_g2(float3 V, float3 L, float3 N, float roughness)
{
    float NdotV = abs(dot(N, V));
    float NdotL = abs(dot(N, L));
    return            (2.0 * NdotL * NdotV)
        / //-----------------------------------------------------//
        (2.0 * mix(2.0 * NdotL * NdotV, NdotL + NdotV, roughness));
}

// GGX/Trowbridge-Reitz normal distribution function
// Input H: half light vector
// Input N: surface normal
// Input roughness
// Output:
float ggx_ndf(float NdotH, float roughness)
{
    float a2 = roughness * roughness;
    float NdotH2 = NdotH*NdotH;

    return                 a2
        / //---------------------------------------------------------------//
        (    PI * ((NdotH2 * (a2 - 1.0) + 1.0)*(NdotH2 * (a2 - 1.0) + 1.0)));
}

// Spherical Gaussian approximation of Shlick's approximation
float3 fresnel_shlick(float3 V, float3 H, float3 F0)
{
    const float a = -5.55473;
    const float b = -6.98316;
    float VdotH = safe_dot(V, H);
    // return F0 - (1.0 - F0) * pow(2, (a * VdotH + b) * VdotH);
    return F0 + (1.0 - F0) * pow(max(1.0 - VdotH, 0.0), 5.0);
}

float3 smith_ggx_brdf(float3 N, float3 V, float3 L, float3 albedo, float roughness, float metallic)
{
    float3 H = normalize(V + L);
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    float NdotH = safe_dot(N, H);

    float D = ggx_ndf(NdotH, roughness);
    float G = smith_ggx_g2(V, L, N, roughness);

    float3 F0 = float3(0.04);
    float3 F = fresnel_shlick(V, H, F0);

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    // metallic materials dont have diffuse reflections
    kD *= 1.0 - metallic;

    float3 lambert_brdf = albedo / PI;

    float3 specular_brdf = (D * G * F)
                      / //--------------------//
                       (4.0 * NdotL * NdotV);

    return kD * lambert_brdf + specular_brdf;
}

// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
float smith_ggx_pdf(float3 wo, float3 wm, float roughness)
{
    float G1 = smith_ggx_g1(wo, wm, roughness);
    float D  = ggx_ndf(wm.z, roughness);

    return D * G1 * safe_dot(wo, wm) / wo.z;
}


#endif

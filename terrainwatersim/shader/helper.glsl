#include "constantbuffers.glsl"

vec3 ApplyFog(vec3 color, vec3 cameraPosition, float cameraDistance, vec3 toCameraVec)
{
	const float fogDensity = 0.001;
	const float fogIntensity = 0.6;
	float fogAmount = clamp( - fogIntensity * exp(-cameraPosition.y * fogDensity) * (1.0 - exp( cameraDistance * toCameraVec.y * fogDensity)) / toCameraVec.y, 0, 1);
	vec3 fogColor = vec3(0.18867780436772762, 0.4978442963618773, 0.6616065586417131); // air color
	return mix(color, fogColor, fogAmount);
}

float Fresnel(float nDotV, float R0)
{
	float base = 1.0 - nDotV;
	float exponential = pow(base, 5.0);
	return exponential + R0 * (1.0 - exponential);
}

vec3 Refract(float enterDotNormal, vec3 enteringRay, vec3 normal, float eta)
{
    float k = 1.0 - eta * eta * (1.0 - enterDotNormal * enterDotNormal);
    if (k < 0.0)
        return vec3(0.0);
    else
        return eta * enteringRay - (eta * enterDotNormal + sqrt(k)) * normal;
}

vec3 Refract(vec3 enteringRay, vec3 normal, float eta)
{
	float cosi = dot(enteringRay, normal);
	return Refract(cosi, enteringRay, normal, eta);
}

vec3 ComputeRayDirection(in vec2 screenTexCor, in mat4 inverseViewProjection)
{
	vec2 deviceCor = 2.0 * screenTexCor - 1.0;
	vec4 rayOrigin = inverseViewProjection * vec4(deviceCor, -1.0, 1.0);
	rayOrigin.xyz /= rayOrigin.w;
	vec4 rayTarget = inverseViewProjection * vec4(deviceCor, 0.0, 1.0) ;
	rayTarget.xyz /= rayTarget.w;
	return normalize(rayTarget.xyz - rayOrigin.xyz);
};

float CalcLuminance(vec3 color)
{
	return dot(color, vec3(0.299, 0.587, 0.114));
}

// Combines two normalvectors from normalmaps to a new one.
// The "from normalmaps" part is quite important since it makes assumptions to the orientation!
// -> Please insert raw normalmap data!
// http://blog.selfshadow.com/publications/blending-in-detail/
vec3 CombineNormalmaps(vec3 normalmap1, vec3 normalmap2)
{
    vec3 t = normalmap1.xyz * vec3( 2,  2, 2) + vec3(-1, -1,  0);
    vec3 u = normalmap2.xyz * vec3(-2, -2, 2) + vec3( 1,  1, -1);
    vec3 r = t * dot(t, u) - u*t.z;
    return normalize(r);
}

// Basically removes the w division from z again.
// Good explanation: http://stackoverflow.com/questions/6652253/getting-the-true-z-value-from-the-depth-buffer
float LinearizeZBufferDepth(float depthFromZBuffer)
{
    return (2.0 * NearPlane * FarPlane) / (FarPlane + NearPlane - (2.0 * depthFromZBuffer - 1.0) * (FarPlane - NearPlane));
}

#define saturate(value) clamp((value), 0, 1)

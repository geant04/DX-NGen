#include "Common.hlsli"
#include "CommonBuffers.hlsli"

RWTexture2D<float4> outputTexture : register(u0);

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

#define FURNACE_TEST 0
static const float FURNACE_RADIANCE = 1.0f;

// Scene details
static const Sphere sphere = 
{ 
	float3(0.0f, 5.0f, 0.0f), 
	1.0f, 
	{ 
		float3(0.88f, 0.33f, 0.33f),
		0.0f,
		0.0f,
		false
	} 
};
static const Sphere metalSphere =
{
	float3(1.8f, 5.0f, -0.25f),
	0.75f,
	{
		float3(0.33f, 0.88f, 0.33f),
		1.0f, // metal
		0.0f,
		false
	}
};
static const Sphere glassSphere =
{
	float3(-1.8f, 5.0f, -0.25f),
	0.75f,
	{
		float3(0.88f, 0.33f, 0.33f),
		0.0f,
		1.0f, // transmissive
		false
	}
};
static const Plane groundPlane = 
{ 
	float3(0.0f, 0.0f, -1.0f), 
	float3(0.0f, 0.0f, 1.0f), 
	float2(0.0f, 0.0f), 
	{ 
		float3(0.88f, 0.88f, 0.88f), 
		0.0f,
		0.0f,
		false 
	}
};
static const Plane wallPlane1 =
{
	float3(-4.0f, 10.0f, -1.0f),
	float3(1.0f, 0.0f, 0.0f),
	float2(0.0f, 0.0f),
	{
		float3(0.88f, 0.33f, 0.33f),
		0.0f,
		0.0f,
		false
	}
};
static const Plane wallPlane2 =
{
	float3(4.0f, 10.0f, -1.0f),
	float3(-1.0f, 0.0f, 0.0f),
	float2(0.0f, 0.0f),
	{
		float3(0.33f, 0.88f, 0.33f),
		0.0f,
		0.0f,
		false
	}
};
static const Plane wallPlane3 =
{
	float3(0.0f, 10.0f, -1.0f),
	float3(0.0f, -1.0f, 0.0f),
	float2(0.0f, 0.0f),
	{
		float3(0.88f, 0.88f, 0.88f),
		0.0f,
		0.0f,
		false
	}
};
static const Plane lightPlane =
{
	float3(0.0f, 3.0f, 4.0f),
	normalize(float3(0.0f, 1.0f, -1.0f)),
	float2(1.0f, 1.0f),
	{
		10.0f * float3(0.88f, 0.88f, 0.88f),
		0.0f,
		0.0f,
		true //  emissive
	}
};

static const Sphere furnaceDiffuse =
{
	float3(0.0f, 5.0f, 0.0f), 1.0f,
	{ float3(1.0f, 1.0f, 1.0f), 0.0f, 0.0f, false }
};
static const Sphere furnaceMetal =
{
	float3(2.2f, 5.0f, 0.0f), 0.75f,
	{ float3(1.0f, 1.0f, 1.0f), 1.0f, 0.0f, false }
};
static const Sphere furnaceGlass =
{
	float3(-2.2f, 5.0f, 0.0f), 0.75f,
	{ float3(1.0f, 1.0f, 1.0f), 0.0f, 1.0f, false }
};

static const float3 lightPosition = float3(0.0, 2.0, 5.0f);

// Rendering details
static const uint NUM_ITERATIONS = 20;

void GetIntersection(in Ray ray, inout Intersect intersect)
{
	intersect.t = 100000.0f;
	intersect.isHit = false;
	
	// Will expand upon this later when we have more geometry with more complex intersection tests.
	
#if !FURNACE_TEST
	GetPlaneIntersection(groundPlane, ray, intersect);
	GetPlaneIntersection(wallPlane1, ray, intersect);
	GetPlaneIntersection(wallPlane2, ray, intersect);
	GetPlaneIntersection(wallPlane3, ray, intersect);
	GetPlaneIntersection(lightPlane, ray, intersect);
	GetSphereIntersection(sphere, ray, intersect);
	GetSphereIntersection(metalSphere, ray, intersect);
	GetSphereIntersection(glassSphere, ray, intersect);
#endif // #if !FURNACE_TEST
#if FURNACE_TEST
	GetSphereIntersection(furnaceDiffuse, ray, intersect);
	GetSphereIntersection(furnaceMetal, ray, intersect);
	GetSphereIntersection(furnaceGlass, ray, intersect);
#endif // #if FURNACE_TEST
}

float3 GetDiffuseWi(in float3 worldNormal, inout uint state)
{
	float u1 = rng(state);
	float u2 = rng(state);
	
	// Cosine-weighted hemisphere sample (local space, z = normal)
	float r = sqrt(u1);
	float phi = 2.0f * PI * u2;
	float3 local = float3(r * cos(phi), r * sin(phi), sqrt(max(0.0f, 1.0f - u1)));

	// Build an orthonormal basis around the normal
	float3 referenceUp = abs(worldNormal.z) > 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
	float3 tangent = normalize(cross(referenceUp, worldNormal));
	float3 bitangent = cross(worldNormal, tangent);

	return normalize(tangent * local.x + bitangent * local.y + worldNormal * local.z);
}

// inDir vector points "outwards", if that makes sense.
float3 GetWi(in float3 worldInDir, in float3 worldNormal, in Material material, inout uint state)
{
	float cosThetaI = dot(worldInDir, worldNormal);

	if (material.metallic > 0.0f)
	{
		// Reflect doesn't "reflect" in the way that the in-vector points outwards.
		return normalize(reflect(-worldInDir, worldNormal));
	}
	if (material.transmissive > 0.0f)
	{
		// TODO: This material assumes for perfectly smooth surface.
		// Thus, it will always perfectly sample as if perfectly specular.
		// We will have to introduce a distribution function later on that provides a check for "isEffectivelySmooth".
		float etaA = 1.0f;
		float etaB = 1.49f;
		float eta = cosThetaI < 0.0f ? etaB / etaA : etaA / etaB;
		
		float fresnel = FresnelDielectric(cosThetaI, etaB / etaA);
		float probRefl = fresnel;
		float probTrans = 1.0f - probRefl;
		
		if (cosThetaI < 0.0f)
		{
			worldNormal = -worldNormal;
		}
		
		if (rng(state) < probRefl / (probRefl + probTrans))
		{
			return normalize(reflect(-worldInDir, worldNormal));
		}
		else
		{
			return normalize(refract(-worldInDir, worldNormal, eta));
		}
	}
	
	// Will add more complicated BSDFs later.
	// For now, treat all materials as a diffuse material.
	return GetDiffuseWi(worldNormal, state);
}

float3 GetReflectance(float cosThetaI, in Material material)
{
	if (material.transmissive > 0.0f)
	{
		// Debug
		float fresnel = FresnelDielectric(cosThetaI, 1.46f);
		return 1.0f;
	}
	
	return material.color;
}

float3 NaivePathTracer(in Ray ray, uint iterations, inout uint state)
{
	float3 outColor = 1.0f;
	Intersect intersect;
	
	for (uint i = 1; i <= iterations; i++)
	{
		GetIntersection(ray, intersect);
		
		// Hit nothing - i.e the sky
		if (!intersect.isHit)
		{
#if FURNACE_TEST
			return FURNACE_RADIANCE * outColor;
#endif // #if FURNACE_TEST
			return 0.0f;
		}
		
		// If we hit a light source, return.
		if (intersect.material.isEmissive)
		{
			return intersect.material.color * outColor;
		}
		
		float3 normal = intersect.normal;
		float cosThetaI = dot(-ray.direction, normal);
		float3 bsdfColor = GetReflectance(cosThetaI, intersect.material); // intersect.material.color;
		float3 sampleDirection = GetWi(-ray.direction, normal, intersect.material, state);
		
		outColor *= bsdfColor;
		ray.direction = sampleDirection;
		ray.origin = intersect.position + ray.direction * 0.0001f;
	}
	
	// Return nothing, since we didn't hit a light.
	return 0.0f;
}

float3 NaiveRayTracer(in Ray ray, in float3 animatedLightPosition)
{
	float3 outColor = 0.0f;
	
	Intersect intersect;
	intersect.t = 100000.0f;
	intersect.isHit = false;
	
	GetIntersection(ray, intersect);
	
	if (intersect.isHit)
	{
		// Lighting!
		float3 normal = intersect.normal;
		float3 color = intersect.material.color;
		
		float3 intersectToLight = animatedLightPosition - intersect.position;
		float lightDistanceSq = dot(intersectToLight, intersectToLight);
		float lightPower = 40.0f;
		
		float3 light = normalize(intersectToLight);
		float lambert = max(dot(light, normal), 0.0f);
		
		// Shadow Test
		Intersect shadowIntersect;
		shadowIntersect.t = 100000.0f;
		shadowIntersect.isHit = false;
		
		Ray shadowRay;
		shadowRay.direction = light;
		shadowRay.origin = intersect.position + shadowRay.direction * 0.001f;
		
		GetIntersection(shadowRay, shadowIntersect);
		float shadow = 1.0f;
		
		if (shadowIntersect.isHit && shadowIntersect.t * shadowIntersect.t < lightDistanceSq)
		{
			shadow = 0.0f;
		}
		
		color *= lambert * shadow * (lightPower / lightDistanceSq);
		
		outColor = color;
	}
	
	return outColor;
}

[numthreads(8, 8, 1)]
void CSMain( uint3 id : SV_DispatchThreadID )
{	
	// RNG setup
	uint state = initRNG(id.xy, data.frameNumber);
	
	// AA jitter hack
	float2 jitter = float2(rng(state), rng(state));
	
	float2 uv = (float2(id.x, id.y) + jitter) / float2(1280.0f, 720.0f);
	uv.y = 1.0f - uv.y;
	
	float2 ndc = 2.0f * uv - 1.0f;
	float aspectRatio = 720.0f / 1280.0f;
	
	// Let our coordinate space be Z-up... I guess
	float3 direction = normalize(float3(ndc.x, 1.0f, ndc.y * aspectRatio));
	float3 origin = float3(0.0f, 0.0f, 0.5f);
	
	float3 animatedLightPosition = lightPosition;
	
	// Ray-tracing setup...
	Ray ray;
	ray.direction = direction;
	ray.origin = origin;
	
	Intersect intersect;
	intersect.t = 100000.0f;
	intersect.isHit = false;
	
	//float3 outColor = NaiveRayTracer(ray, animatedLightPosition);
	float3 outColor = NaivePathTracer(ray, NUM_ITERATIONS, state);
	
	// Lane-independent smoothing call
	float3 accumulatedColor = outputTexture[id.xy].rgb;
	if ( data.resetPathTracer )
	{
		accumulatedColor = float3( 0.0f, 0.0f, 0.0f );
	}
	
	outColor = lerp(accumulatedColor, outColor, 1.0f / (data.frameNumber + 1));
	
	// float randR = rng(id.xy * frameNumber);
	outputTexture[id.xy] = float4(outColor, 1);
}
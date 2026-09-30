#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

// Header file used for various things, such as defining structs, and random functions.
// Will need a dedicated shader file for uniform rng() ray generation code when we path-trace eventually.
#define PI 3.14159265359f

struct Material
{
	float3 color;
	float metallic;
	float transmissive;
	bool isEmissive;
};

struct Sphere
{
	float3 center;
	float radius;
	Material material;
};

struct Plane
{
	float3 center;
	float3 normal;
	float2 scaleXY;
	Material material;
};

struct Ray
{
	float3 origin;
	float3 direction;
};

// Intersection Code
struct Intersect
{
	float3 position;
	float3 normal;
	Material material;
	float t;
	bool isHit;
};

void GetSphereIntersection(in Sphere sphere, in Ray ray, inout Intersect intersect)
{
	float3 l = sphere.center - ray.origin;
	float lengthSq = dot(l, l);
	float projLOnDir = dot(l, ray.direction);
	
	if (projLOnDir < 0.0f)
		return;
	
	float distanceToTangentSq = lengthSq - (projLOnDir * projLOnDir);
	float radiusSq = sphere.radius * sphere.radius;
	if (distanceToTangentSq > radiusSq)
		return;
	
	// Need to check for intersection behind camera too!
	float thc = sqrt(radiusSq - distanceToTangentSq);
	float t1 = projLOnDir - thc;
	float t2 = projLOnDir + thc;
	
	if (t1 < 0.0f)
	{
		t1 = t2;
		if (t2 < 0.0f)
		{
			return;
		}
	}
	
	if (intersect.t > t1)
	{			
		float3 outPosition = ray.origin + ray.direction * t1;
		float3 outNormal = normalize(outPosition - sphere.center);
		// If we have a backwards intersection, we can just reverse the normal.
		// But I think this makes it harder later on, since we perform intersection test first
		// before we perform sample Wi test. Thus, comment it out.
		// outNormal = dot(ray.direction, outNormal) > 0.0f ? -outNormal : outNormal;

		intersect.isHit = true;
		intersect.t = t1;
		intersect.position = outPosition;
		intersect.normal = outNormal;
		intersect.material = sphere.material;
	}
}
	
void GetPlaneIntersection(in Plane plane, in Ray ray, inout Intersect intersect)
{
	// p = ray * t + ray.origin - plane.center , s.t dot(p, plane.normal) = 0
	// dot(ray * t, plane.normal) + dot(ray.origin, plane.normal) - dot(plane.center, plane.normal) = 0
	// t * dot(ray, plane.normal) + dot(ray.origin, plane.normal) - dot(plane.center, plane.normal) = 0
	// t = dot(plane.normal, plane.center - ray.origin) / dot(ray, plane.normal)
	
	float rayDirDotNormal = dot(ray.direction, plane.normal);
	
	// Back-face cull as well
	if (abs(rayDirDotNormal) < 0.001f || rayDirDotNormal > 0.0f)
	{
		return;
	}
	
	float t = dot(plane.normal, plane.center - ray.origin) / rayDirDotNormal;
	
	// Plane intersection is behind the camera
	if (t <= 0)
	{
		return;
	}
	
	// Check if plane intersection is within bounds of scaleX and scaleY
	// For now, we will simply do a disk check because it's honestly too much work
	// to refactor everything to work off of a transform matrix-based system...
	// TODO: Replace this code with the transform system!
	// For now, we just want to make sure the path-tracer works.
	float3 pHit = ray.origin + ray.direction * t;
	
	if (any(plane.scaleXY))
	{
		float3 hitToCenter = plane.center - pHit;
		float distToCenterSq = dot(hitToCenter, hitToCenter);
		if (distToCenterSq > plane.scaleXY.x * plane.scaleXY.x)
		{
			return;
		}
	}
	
	if (intersect.t > t)
	{
		intersect.isHit = true;
		intersect.t = t;
		intersect.position = ray.origin + ray.direction * t;
		intersect.normal = plane.normal;
		intersect.material = plane.material;
	}
}

// RNG code, replacae with state-based RNG
float pcgHash(inout uint state)
{
	state = state * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float rng(inout uint state)
{
	return float(pcgHash(state)) / float(0xFFFFFFFFU);
}

uint initRNG(uint2 id, uint frame)
{
	// Ripped this magic from the internet.
	uint seed = id.x ^ (id.y << 16) ^ (frame * 0x9E3779B9u);
	return pcgHash(seed);
}

// Fresnel equations, cosThetaI should be in with wi and normal
float FresnelDielectric(float cosThetaI, float eta)
{
	cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);
	
	// Potentially flip orientations
	if (cosThetaI < 0.0f)
	{
		eta = 1.0f / eta;
		cosThetaI = -cosThetaI;
	}
	
	// Compute cosThetaT
	float sin2ThetaI = 1.0f - (cosThetaI * cosThetaI);
	float sin2ThetaT = sin2ThetaI / (eta * eta);
	if (sin2ThetaT >= 1.0f)
	{
		return 1.0f;
	}
	float cosThetaT = sqrt(1.0f - sin2ThetaT);
	
	// Bring it all together, computing parallel and perpindicular oscillation values
	float rParl = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
	float rPerp = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
	
	return 0.5f * ((rParl * rParl) + (rPerp * rPerp));
}


#endif // __COMMON_HLSLI__
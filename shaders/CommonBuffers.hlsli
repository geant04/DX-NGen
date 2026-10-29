#ifndef __COMMON_BUFFERS_HLSLI__
#define __COMMON_BUFFERS_HLSLI__

struct CameraConstantsData
{
	float4x4 modelViewProj;
	float4x4 viewProjInverse;
	float4x4 lightViewProj;
	float4 cameraPosition;
	float3 sunDirection;
	uint frameNumber;
	bool resetPathTracer;
};

#endif // #ifndef __COMMON_BUFFERS_HLSLI__
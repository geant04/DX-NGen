#ifndef __COMMON_BUFFERS_HLSLI__
#define __COMMON_BUFFERS_HLSLI__

struct CameraConstantsData
{
	float4x4 modelViewProj;
	float4x4 viewProjInverse;
	float4 cameraPosition;
	uint frameNumber;
};

#endif // #ifndef __COMMON_BUFFERS_HLSLI__
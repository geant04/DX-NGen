#ifndef __COMMON_BUFFERS_HLSLI__
#define __COMMON_BUFFERS_HLSLI__

#define NULL_ID 0xFFFFFFFF

struct CameraConstantsData
{
	float4x4 modelViewProj;
	float4x4 viewProjInverse;
	float4x4 lightViewProj;
	float4 cameraPosition;
	float3 sunDirection;
	uint frameNumber;
	bool resetPathTracer;
	float invScreenWidth;
	float invScreenHeight;
	uint numTransforms;
};

struct TransformData
{
	float4x4 model;
	float4x4 rotate;
	float4x4 scale;
	float4x4 translate;
};

struct IndirectCommand
{
	uint vertexCountPerInstance;
	uint instanceCount;
	uint startVertexLocation;
	uint startInstanceLocation;
};

#endif // #ifndef __COMMON_BUFFERS_HLSLI__
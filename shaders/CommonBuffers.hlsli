#ifndef __COMMON_BUFFERS_HLSLI__
#define __COMMON_BUFFERS_HLSLI__

#define NULL_ID 0xFFFFFFFF

struct CameraConstantsData
{
	float4x4 modelViewProj;
	float4x4 viewProjInverse;
	float4x4 lightViewProj;
	float4x4 projInverse; // NEW ADDITION, UPDATE THIS STRUCT!!!
	float4x4 view; // NEW ADDITION, UPDATE THIS STRUCT!!!
	float4 cameraPosition;
	float3 sunDirection;
	uint frameNumber;
	bool resetPathTracer;
	float invScreenWidth;
	float invScreenHeight;
	uint numTransforms;
	uint numPointLights;
	uint3 clusterDims; // NEW ADDITION, UPDATE THIS STRUCT!!!
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

struct PointLight
{
	float3 worldPosition;
	float squaredRadius;
	float3 color;
	uint globalIdx;
};

#define MAX_LIGHTS_IN_CLUSTER 128

struct Cluster
{
	uint pointLightIndices[MAX_LIGHTS_IN_CLUSTER];
	uint numLights;
	uint globalIdx;
};

struct ClusterBBOX
{
	float3 min;
	float3 max;
	uint globalIdx;
};

#endif // #ifndef __COMMON_BUFFERS_HLSLI__
#include "CommonBuffers.hlsli"

StructuredBuffer<TransformData> transforms : register(t0, space1);
StructuredBuffer<uint> transformBatchIds : register(t1, space1);
RWStructuredBuffer<int> transformIndexOffsets : register(u0, space1);
RWStructuredBuffer<IndirectCommand> indirectDrawArgs : register(u1, space1);

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

#define CS_CULL_WIDTH 8

bool IsInFrustum( in float4x4 viewProj, in TransformData transform )
{
	float3 worldPosition = mul( transform.model, float4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xyz;
	float arbitraryRadiusForNowBecauseISaidSo = 10.0f;
	
	// Frustum plane check does its frustum plane check thing ma-bober. Sweet.
	// For now, skip this culling process. Check if we can run this compute shader with all my refactors in-place for RW buffers.
	return true;
}

[numthreads( CS_CULL_WIDTH, 1, 1 )]
void main( uint3 dtid : SV_DispatchThreadID )
{
	// There's not really an easy way to ignore dtid if we're out of bounds I think. I need to somehow pass in the # of transforms in
	uint transformId = dtid.x;
	
	if ( transformId >= data.numTransforms )
	{
		return;
	}
	
	TransformData transform = transforms[transformId];
	float4x4 viewProj = data.modelViewProj;
	
	if ( IsInFrustum( viewProj, transform ) )
	{
		uint batchId = transformBatchIds[transformId];
		uint writeIndex;
		
		InterlockedAdd( indirectDrawArgs[batchId].instanceCount, 1, writeIndex );
		transformIndexOffsets[indirectDrawArgs[batchId].startInstanceLocation + writeIndex] = transformId;
	}
}
#include "CommonBuffers.hlsli"

StructuredBuffer<TransformData> transforms : register(t0, space1);
StructuredBuffer<uint> transformBatchIds : register(t1, space1);
RWStructuredBuffer<uint> transformIndexOffsets : register(u0, space1);
RWStructuredBuffer<IndirectCommand> indirectDrawArgs : register(u1, space1);

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

#define CS_CULL_WIDTH 64

// TODO: NEED A SEPARATE SOLUTION FOR SHADOW CULLING, WHICH SHOULD RELY ON ALREADY CULLED DATA.
// The problem is: how do we actually identify whether or not a given transform is culled?
// I suppose a start is to map 1 thread per transform, then derive the indices of each transform in indirectDrawArgs,
// and then repopulate indirectDrawArgs in the shadow... at that point, shadow should just have its own cull? or we add a param to
// read from an existing drawArgs buffer that stores instance counts? Maybe we don't worry about this for now.

bool IsInFrustum( in float4x4 viewProj, in TransformData transform, out uint isLimitedCulled )
{
	// The cube is centered on its pivot, so the local center is the origin.
	float3 center = mul( transform.model, float4( 0.0f, 0.0f, 0.0f, 1.0f ) ).xyz;

	float4 homogClipPos = mul( viewProj, float4( center, 1.0f ) );
	float x = homogClipPos.x;
	float y = homogClipPos.y;
	float z = homogClipPos.z;
	float w = homogClipPos.w;

	bool depthCheck = ( 0 < z && z < w );
	
	float wp = w * 1.0f;
	bool xCheck = ( -w < x && x < w );
	bool yCheck = ( -w < y && y < w );
	bool xLimitCheck = ( -wp < x && x < wp );
	bool yLimitCheck = ( -wp < y && y < wp );
	
	isLimitedCulled = ( xLimitCheck && yLimitCheck && depthCheck ) ? 1u : 0u;
	return xCheck && yCheck && depthCheck;
}

[numthreads( CS_CULL_WIDTH, 1, 1 )]
void main( uint3 dtid : SV_DispatchThreadID )
{
	// There's not really an easy way to ignore dtid if we're out of bounds I think. I need to somehow pass in the # of transforms in
	uint transformId = dtid.x;
	
	if ( transformId >= data.numTransforms || transformBatchIds[transformId] == NULL_ID )
	{
		return;
	}
	
	TransformData transform = transforms[transformId];
	float4x4 viewProj = ( data.modelViewProj );
	uint isLimitCulled = 0;
	
	if ( IsInFrustum( viewProj, transform, isLimitCulled ) )
	{
		uint batchId = transformBatchIds[transformId];
		uint writeIndex;
		
		InterlockedAdd( indirectDrawArgs[batchId].instanceCount, 1, writeIndex );
		
		// I'll just like bit flip the most significant bit 
		// as a flag depending on the isLimitCulled thing, and then
		// in the vert shader, acquire index by (index & 0x0FFFFFF) or something
		uint index = transformId;
		index |= (isLimitCulled << 31);
		transformIndexOffsets[indirectDrawArgs[batchId].startInstanceLocation + writeIndex] = index;
	}
}
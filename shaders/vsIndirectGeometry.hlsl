#include "CommonBuffers.hlsli"

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

StructuredBuffer<TransformData> transforms : register(t0, space1);

#if !SHADOW_PASS
StructuredBuffer<uint> transformIndexOffsets : register(t1, space1);
#endif

struct VSInput
{
	float4 position : POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct VSOutput
{
	float4 position : SV_POSITION; // viewspace position
	float4 lightPosition : COLOR0;
	float4 worldPosition : COLOR1;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
	nointerpolation uint limitCulled : TEXCOORD1;
};

VSOutput VSMain( VSInput input, uint instanceId : SV_InstanceID )
{
	VSOutput output;
	
	bool isLimitCulled = false;
	uint indexOffset = instanceId;
#if !SHADOW_PASS
	uint packed = transformIndexOffsets[instanceId];
	isLimitCulled = ( packed >> 31 ) != 0;
	indexOffset = packed & 0x7FFFFFFF;
#endif
	TransformData transform = transforms[indexOffset];
	
	float3 funOffset = min( float(indexOffset), 1.0f ) * float3( 0.0f, sin( float(data.frameNumber + indexOffset) * 0.01f ), 0.0f );
	float4 worldPosition = mul( transform.model, float4( input.position.xyz + funOffset, 1.0f ) );
	
	// Did a bit of a hack so that in this indirect call, "modelViewProj" actually just
	// stores the viewProj matrix, so we have to do a bit of extra matrix math here on the GPU...
	output.position = mul( data.modelViewProj, worldPosition );
	output.worldPosition = worldPosition;
	output.normal = input.normal;
	output.uv = input.uv;
	output.lightPosition = mul( data.lightViewProj, worldPosition );
	output.limitCulled = indexOffset;
	
	return output;
}
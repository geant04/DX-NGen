#include "CommonBuffers.hlsli"

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

StructuredBuffer<TransformData> transforms : register(t0, space1);

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
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

VSOutput VSMain( VSInput input, uint instanceId : SV_InstanceID )
{
	VSOutput output;
	
	TransformData transform = transforms[instanceId];
	float4 worldPosition = mul( transform.model, float4( input.position.xyz, 1.0f ) );
	
	// Did a bit of a hack so that in this indirect call, "modelViewProj" actually just
	// stores the viewProj matrix, so we have to do a bit of extra matrix math here on the GPU...
	output.position = mul( data.modelViewProj, worldPosition );
	output.normal = input.normal;
	output.uv = input.uv;
	output.lightPosition = mul( data.lightViewProj, worldPosition );
	
	return output;
}
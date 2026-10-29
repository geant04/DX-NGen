#include "CommonBuffers.hlsli"

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

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

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	
	output.position = mul( data.modelViewProj, float4( input.position.xyz, 1.0f ) );
	output.normal = input.normal;
	output.uv = input.uv;
	output.lightPosition = mul( data.lightViewProj, float4( input.position.xyz, 1.0f ) );
	
	return output;
}
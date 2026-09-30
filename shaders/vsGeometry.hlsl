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
	float4 position : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

VSOutput VSMain(VSInput input)
{
	VSOutput output;
	
	output.position = mul( data.modelViewProj, float4( input.position.xyz, 1.0f ) );
	output.normal = input.normal;
	output.uv = input.uv;
	
	return output;
}
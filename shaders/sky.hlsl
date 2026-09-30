#include "CommonBuffers.hlsli"

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

float4 PSMain(PSInput input) : SV_TARGET
{
	float2 uv = input.texCoord;
	
	// Map screen UV to a view direction used for sky-texture sampling
	float2 ndc = uv * 2.0f - 1.0f;
	float4 worldPos = mul( data.viewProjInverse, float4( ndc, 0.0f, 0.0f ) );
	float3 viewDirection = normalize( worldPos.xyz - data.cameraPosition.xyz );
	
	float viewY = viewDirection.y;
	
    return float4( viewDirection, 1.0f );
}
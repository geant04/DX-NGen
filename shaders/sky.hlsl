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
	float2 ndc = input.texCoord * 2.0f - 1.0f;
	ndc.y = 1.0f - ndc.y;

	float4 nearPoint = mul( data.viewProjInverse, float4( ndc, 0.0f, 1.0f ) );
	float4 farPoint = mul( data.viewProjInverse, float4( ndc, 1.0f, 1.0f ) );

	nearPoint /= nearPoint.w;
	farPoint /= farPoint.w;

	float3 viewDirection = normalize( farPoint.xyz - nearPoint.xyz );
	
	float skyLerp = saturate( viewDirection.y * 0.5f + 0.5f );
	float3 horizonColor = float3( 0.796f, 0.894f, 1.0f );
	float3 zenithColor = float3( 0.318f, 0.58f, 0.851f );
	float3 skyColor = lerp( horizonColor, zenithColor, skyLerp );
	
	float3 sunDir = normalize( -data.sunDirection );

	float sunAlignment = saturate( dot( viewDirection, sunDir ) );
	float sunDisc = pow( sunAlignment, 2048.0f );
	float sunGlow = pow( sunAlignment, 32.0f );
	float3 sunColor = float3( 1.0f, 0.75f, 0.35f );

	skyColor += sunColor * sunGlow * 0.25f;
	skyColor += sunColor * sunDisc * 4.0f;

	return float4( skyColor, 1.0f );
}
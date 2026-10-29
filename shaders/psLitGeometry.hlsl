#include "CommonBuffers.hlsli"

struct PSInput
{
	float4 position : SV_POSITION;
	float4 lightPosition : COLOR0;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

Texture2D shadowMap : register(t0);
SamplerState texSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
	// Shadow calculation work
	// this is in light's viewspace
	float4 lightDepthPosition = input.lightPosition;
	float4 ndcLightPosition = ( lightDepthPosition ) / lightDepthPosition.w;
	float ndcLightDepth = ndcLightPosition.z;
	ndcLightPosition.xy = float2( 0.5f, -0.5f ) * ndcLightPosition.xy + float2( 0.5f, 0.5f );
	
	float shadowMapDepth = shadowMap.Sample( texSampler, ndcLightPosition.xy );
	float shadow = ( shadowMapDepth + 0.005f < ndcLightDepth ) ?  0.0f : 1.0f;
	
	float3 outColor = 1.0f;
	float3 normal = normalize( input.normal.rgb );
	float3 sunDir = normalize( data.sunDirection.rgb );
	
	outColor *= dot( normal, -sunDir ) * shadow;
	
	return float4( outColor, 1.0f );
}
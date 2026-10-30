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
SamplerComparisonState shadowSampler : register(s1);

#define TEST_SIMPLE_BOX_TAP 1

float GetShadow( in float4 lightDepthPosition, in float2 screenPos )
{
	// lightDepthPosition is position in light's viewspace
	float4 ndcLightPosition = ( lightDepthPosition ) / lightDepthPosition.w;
	const float ndcLightDepth = ndcLightPosition.z;
	const float2 lightUV = ndcLightPosition.xy = float2( 0.5f, -0.5f ) * ndcLightPosition.xy + float2( 0.5f, 0.5f );
	
	// 1 / 512.0f
	const float invShadowMapSize = 0.00048828125f;
	const float bias = 0.001f;
	float shadowMapDepth = shadowMap.Sample( texSampler, lightUV );
	float shadow = 0.0f;

#if TEST_SIMPLE_BOX_TAP
	if ( lightUV.x >= 0.0f && lightUV.x <= 1.0f && lightUV.y >= 0.0f && lightUV.y <= 1.0f )
	{
		const uint samples = 8;
		const float2 offsets[samples] = 
		{ 
			{ -0.5f,  0.5f },
			{ -0.0f,  0.5f },
			{  0.5f,  0.5f },
			{ -0.5f,  0.0f },
			{  0.5f,  0.0f },
			{ -0.5f, -0.5f },
			{  0.0f, -0.5f },
			{  0.5f, -0.5f }
		};	
		
		const float centerWeight = 0.20418f;
		const uint gaussianSamples = 8;
		const float weights[gaussianSamples] =
		{
			0.07511f,
			0.12384f,
			0.07511f,
			0.12384f,
			0.12384f,
			0.07511f,
			0.12384f,
			0.07511f
		};
			
		shadow = centerWeight * ( 1.0f - shadowMap.SampleCmpLevelZero( shadowSampler, ndcLightPosition.xy, ndcLightDepth - bias ) );
		
		[unroll]
		for ( int i = 0; i < samples; i++ )
		{
			float shadowSample = shadowMap.SampleCmpLevelZero( 
				shadowSampler, ndcLightPosition.xy + offsets[i] * 2.0f * invShadowMapSize, ndcLightDepth - bias );
			float weight = weights[i];
			shadow += weight * ( 1.0f - shadowSample );
		}
	}
#endif
	
	return shadow;
}

float4 PSMain(PSInput input) : SV_TARGET
{	
	float shadow = GetShadow( input.lightPosition, input.position.xy );
	
	float3 outColor = 0.85f;
	float3 normal = normalize( input.normal.rgb );
	float3 sunDir = normalize( data.sunDirection.rgb );
	
	outColor *= ( max( dot( normal, -sunDir ), 0.0f ) * ( 1.0f - shadow + 0.25f ) );
	
	return float4( outColor, 1.0f );
}
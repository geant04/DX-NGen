#include "CommonBuffers.hlsli"

struct PSInput
{
	float4 position : SV_POSITION;
	float4 lightPosition : COLOR0;
	float4 worldPosition : COLOR1;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	nointerpolation uint limitCulled : TEXCOORD1;
};

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

Texture2D shadowMap : register(t0);
SamplerState texSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

// t0, t1 binded in vsIndirectGeometry? I'm not sure how this works
StructuredBuffer<PointLight> pointLights : register(t2, space1);

#define RENDER_SHADOWS 0
#define RENDER_FUN_RED_LIGHT_SCENE 0
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

float3 IntToRandomColor( int seed )
{
	uint x = asuint( seed );
	
	x = ( ( x >> 16 ) ^ x ) * 0x45d9f3b;
	x = ( ( x >> 16 ) ^ x ) * 0x45d9f3b;
	x = ( x >> 16 ) ^ x;
	
	float r = float( x & 0xFF ) / 255.0;
	float g = float( ( x >> 8 ) & 0xFF ) / 255.0;
	float b = float( ( x >> 16 ) & 0xFF ) / 255.0;
	
	return float3( r, g, b );
}

float4 PSMain( PSInput input ) : SV_TARGET
{	
	float3 outColor = 0.0f;
	float3 albedo = lerp( IntToRandomColor( input.limitCulled + 1 ), 1.0f,  0.8f );
	float3 normal = normalize( input.normal.rgb );
	
#if RENDER_SHADOWS
	float shadow = GetShadow( input.lightPosition, input.position.xy );
	float3 sunDir = normalize( data.sunDirection.rgb );
	float sunLightLambert =  max( dot( normal, normalize( -sunDir ) ), 0.0f ) 

	outColor *= sunLightLambert * ( 1.0f - shadow + 0.25f ) );
#endif
	
#if RENDER_FUN_RED_LIGHT_SCENE
	// Will replace this with the actual pointLight calculation
	Light defaultTestLight;
	defaultTestLight.worldPosition = float3( 0.0f, 6.0f, 0.0f );
	defaultTestLight.squaredRadius = 40.0f;
	defaultTestLight.color = float3( 1.0f, 0.0f, 0.0f );
	
	float3 worldPosToLightPos = defaultTestLight.worldPosition - input.worldPosition.xyz;
	float distToLight = max( dot( worldPosToLightPos, worldPosToLightPos ), 0.0001f );
	float pointLightLambert = max( dot( normal, normalize( worldPosToLightPos ) ), 0.0f );
	
	outColor *= lerp( pointLightLambert, 1.0f, 0.5f );
	outColor *= ( defaultTestLight.color ) * ( defaultTestLight.squaredRadius / distToLight );
#endif
#if 1
	uint numPointLights = data.numPointLights;
	
	for ( uint i = 0; i < numPointLights; i++ )
	{
		PointLight pointLight = pointLights[i];
		
		float3 worldPosToLightPos = pointLight.worldPosition - input.worldPosition.xyz;
		float distToLight = max( dot( worldPosToLightPos, worldPosToLightPos ), 0.0001f );
		float pointLightLambert = max( dot( normal, normalize( worldPosToLightPos ) ), 0.0f );
	
		float3 radiance = albedo * lerp( pointLightLambert, 1.0f, 0.5f );
		radiance *= ( pointLight.color ) * ( pointLight.squaredRadius / distToLight );
		outColor += radiance;
	}
#endif
	
	return float4( outColor, 1.0f );
}
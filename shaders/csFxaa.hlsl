#include "CommonBuffers.hlsli"

RWTexture2D<float4> outputTexture : register(u0);
Texture2D<float4> inputTexture : register(t0);
SamplerState texSampler : register(s0);

#define CS_FXAA_WIDTH 8
#define CS_FXAA_HEIGHT 8
#define EDGE_THRESHOLD_MIN 0.0312f
#define EDGE_THRESHOLD_MAX 0.125f

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

float GetLinearLuma( float3 linearRGB )
{
	// encode luma based on Rec.709 primaries
	return dot( linearRGB, float3( 0.2126, 0.7152, 0.0722 ) );
}
float GetPerceptualLuma( float3 linearRGB )
{
	return sqrt( GetLinearLuma( linearRGB ) );
}


void FxaaInternal( int2 dtid )
{	
	// https://blog.simonrodriguez.fr/articles/2016/07/implementing_fxaa.html
	float centerLuma = GetPerceptualLuma( inputTexture[dtid].rgb );
	float upLuma = GetPerceptualLuma( inputTexture[dtid + int2(0, -1)].rgb );
	float downLuma = GetPerceptualLuma( inputTexture[dtid + int2(0, 1)].rgb );
	float leftLuma = GetPerceptualLuma( inputTexture[dtid + int2(-1, 0)].rgb );
	float rightLuma = GetPerceptualLuma( inputTexture[dtid + int2(1, 0)].rgb );
	
	float maxLuma = max( rightLuma, max( leftLuma, max ( upLuma, downLuma ) ) );
	float minLuma = min( rightLuma, min( leftLuma, min ( upLuma, downLuma ) ) );
	
	float lumaRange = maxLuma - minLuma;
	
	if ( lumaRange < max( EDGE_THRESHOLD_MIN, maxLuma * EDGE_THRESHOLD_MAX ) )
	{
		outputTexture[dtid] = float4( inputTexture[dtid].xyz, 1.0f );
		return;
	}
	
	float upLeft = GetPerceptualLuma( inputTexture[dtid + int2( -1, -1 )].rgb );
	float upRight = GetPerceptualLuma( inputTexture[dtid + int2( 1, -1 )].rgb );
	float boLeft = GetPerceptualLuma( inputTexture[dtid + int2( -1, 1 )].rgb );
	float boRight = GetPerceptualLuma( inputTexture[dtid + int2( 1, 1 )].rgb );
	
	float leftCorners = upLeft + boLeft;
	float upDown = upLuma + downLuma;
	float rightCorners = upRight + boRight;
	
	float upCorners = upLeft + upRight;
	float leftRight = leftLuma + rightLuma;
	float downCorners = boLeft + boRight;
	
	float horizontalEdgeWeight = abs( leftCorners - 2.0f * leftLuma ) 
		+ 2.0f * abs( upDown - 2.0f * centerLuma ) + abs( rightCorners - 2.0f * rightLuma );
	
	float verticalEdgeWeight = abs( upCorners - 2.0f * upLuma ) 
		+ 2.0f * abs( leftRight - 2.0f * centerLuma ) + abs( downCorners - 2.0f * downLuma );
	
	bool isHorizontal = horizontalEdgeWeight > verticalEdgeWeight ? 1 : 0;
	const float3 debugColor0 = float3( isHorizontal, 0.0f, 1.0f - isHorizontal );
	
	// Check for orthogonal neighbors to determine true orientation if not on edge
	// Checks for area of highest luma difference, we will move towards that for AA
	float neighbor1 = isHorizontal ? upLuma : leftLuma; // if vertical, choose left orthogonal
	float neighbor2 = isHorizontal ? downLuma : rightLuma;
	
	float gradient1 = neighbor1 - centerLuma;
	float gradient2 = neighbor2 - centerLuma;
	
	bool isGrad1Steepest = abs( gradient1 ) >= abs( gradient2 );
	float gradScaled = 0.25f * max( abs( gradient1 ), abs( gradient2 ) );
	
	float avgLumaInSteepDir = isGrad1Steepest ? 0.5f * ( neighbor1 + centerLuma ) 
											  : 0.5f * ( neighbor2 + centerLuma );
	
	float2 uv = ( float2( dtid ) + 0.5f ) * float2( data.invScreenWidth, data.invScreenHeight );
	float2 untouchedUV = uv;
	float2 offset = float2( !isHorizontal * data.invScreenWidth, isHorizontal * data.invScreenHeight );
	float steepDir = isGrad1Steepest ? -1.0f : 1.0f;
	uv += 0.5f * steepDir * offset;
	
	// Grab neighbors from our half-pixel offsetted uv
	offset = float2( isHorizontal * data.invScreenWidth, !isHorizontal * data.invScreenHeight );
	float2 uv1 = uv - offset;
	float2 uv2 = uv + offset;
	float lumaEnd1 = GetPerceptualLuma( inputTexture.SampleLevel( texSampler, uv1, 0 ).rgb );
	float lumaEnd2 = GetPerceptualLuma( inputTexture.SampleLevel( texSampler, uv2, 0 ).rgb );
	lumaEnd1 -= avgLumaInSteepDir;
	lumaEnd2 -= avgLumaInSteepDir;
	
	bool reached1 = abs( lumaEnd1 ) >= gradScaled;
	bool reached2 = abs( lumaEnd2 ) >= gradScaled;
	bool reachedBoth = reached1 && reached2;
	
	if ( !reached1 )
	{
		uv1 -= offset;
	}
	if ( !reached2 )
	{
		uv2 += offset;
	}
	
	if ( !reachedBoth )
	{
		const uint ITERATIONS = 12;
		for ( uint i = 2; i < ITERATIONS; i++ )
		{
			if ( !reached1 )
			{
				lumaEnd1 = GetPerceptualLuma( inputTexture.SampleLevel( texSampler, uv1, 0 ).rgb );
				lumaEnd1 -= avgLumaInSteepDir;
			}
			if ( !reached2 )
			{
				lumaEnd2 = GetPerceptualLuma( inputTexture.SampleLevel( texSampler, uv2, 0 ).rgb );
				lumaEnd2 -= avgLumaInSteepDir;
			}
			
			reached1 = abs( lumaEnd1 ) >= gradScaled;
			reached2 = abs( lumaEnd2 ) >= gradScaled;
			reachedBoth = reached1 && reached2;
	
			if ( !reached1 )
			{
				uv1 -= offset;
			}
			if ( !reached2 )
			{
				uv2 += offset;
			}
			
			if ( reachedBoth )
			{
				break;
			}
		}
	}

	float distance1 = isHorizontal ? ( uv.x - uv1.x ) : ( uv.y - uv1.y );
	float distance2 = isHorizontal ? ( uv2.x - uv.x ) : ( uv2.y - uv.y );
	
	bool isDirection1 = distance1 < distance2;
	float distanceFinal = min( distance1, distance2 );
	float edgeThickness = ( distance1 + distance2 );
	float pixelOffset = -distanceFinal / edgeThickness + 0.5;
	
	bool isLumaCenterSmaller = centerLuma < avgLumaInSteepDir;
	bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
	float finalOffset = correctVariation ? pixelOffset : 0.0;
	
	const float SUBPIXEL_QUALITY = 0.75f;
	float lumaAverage = (1.0/12.0) * (2.0 * (upDown + leftRight) + leftCorners + rightCorners);

	float subPixelOffset1 = clamp( abs( lumaAverage - centerLuma ) / lumaRange, 0.0, 1.0 );
	float subPixelOffset2 = ( -2.0 * subPixelOffset1 + 3.0 ) * subPixelOffset1 * subPixelOffset1;
	float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;
	finalOffset = max( finalOffset,subPixelOffsetFinal );
	
	float2 finalUv = untouchedUV;
	if ( isHorizontal )
	{
		finalUv.y += finalOffset * steepDir * data.invScreenHeight;
	} else {
		finalUv.x += finalOffset * steepDir * data.invScreenWidth;
	}
	
	float3 finalColor = inputTexture.SampleLevel( texSampler, finalUv, 0 ).rgb;
	outputTexture[dtid] = float4( finalColor, 1.0f );
}

[numthreads( CS_FXAA_WIDTH, CS_FXAA_HEIGHT, 1 )]
void main( uint3 dtid : SV_DispatchThreadID )
{
	FxaaInternal( dtid.xy );
}
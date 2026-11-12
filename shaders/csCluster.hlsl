#include "CommonBuffers.hlsli"

StructuredBuffer<PointLight> pointLights : register(t0, space1);
RWStructuredBuffer<Cluster> clusters : register(u0, space1);
RWStructuredBuffer<ClusterBBOX> clusterBBOX : register(u1, space1);

cbuffer CameraConstants : register(b0)
{
	CameraConstantsData data;
}

#define CS_CLUSTER_PROCESS_X 16
#define CS_CLUSTER_PROCESS_Y 8
#define CS_CLUSTER_PROCESS_Z 2

struct ClusterData
{
	float2 minUV;
	float2 maxUV;
};

// 2.0f * (1.0f - y) - 1.0f
// 2.0f - 2.0f * y - 1.0f
// 1.0f - 2.0f * y

float normalizedDepthToViewZ( float value )
{
	return ( 1000.0f - 0.1f ) * value + 0.1f;
}

float3 uvToViewRay( float2 uv )
{
	float3 ndc = float3( 2.0f * uv.x - 1.0f, 1.0f - 2.0f * uv.y, 1.0f );
	float4 viewPos = mul( data.projInverse, float4( ndc, 1.0f ) );
	viewPos.xyz /= viewPos.w;
	viewPos.xyz /= viewPos.z; // convert to -tan(fovY/2) * aspect, +tan(fovY/2) * aspect in X at (0,0), (1, 0)?
	return viewPos.xyz;
}

bool isLightInCluster( in ClusterBBOX bbox, in PointLight pointLight )
{
	float3 lightViewPosition = mul( data.view, float4( pointLight.worldPosition, 1.0f ) ).xyz;
	float lightRadius = sqrt( pointLight.squaredRadius ); // Oh well!
	
	bool isInX = ( ( bbox.min.x - lightRadius ) < lightViewPosition.x ) 
				&& ( lightViewPosition.x < ( bbox.max.x + lightRadius ) );
	bool isInY = ( ( bbox.min.y - lightRadius ) < lightViewPosition.y ) 
				&& ( lightViewPosition.y < ( bbox.max.y + lightRadius ) );
	bool isInZ = ( ( bbox.min.z - lightRadius ) < lightViewPosition.z ) 
				&& ( lightViewPosition.z < ( bbox.max.z + lightRadius ) );
	
	return isInX && isInY && isInZ;
}

[numthreads( CS_CLUSTER_PROCESS_X, CS_CLUSTER_PROCESS_Y, CS_CLUSTER_PROCESS_Z )]
void cs_main( uint3 dtid : SV_DispatchThreadID )
{
	// Working ex: ( dims: 2 x 2 x 2 )
	// min/max id: [0, 7]
	// 0, 1 - z : 0
	// 2, 3 - z : 0
	// 4, 5 - z : 1
	// 6, 7 - z : 1
	
	// Group pointLights and assign them to clusters
	// Interesting idea: processs by threads, then assign numLights by atomics? hmm...
	uint numClustersX = data.clusterDims.x;
	uint numClustersY = data.clusterDims.y;
	uint numClustersZ = data.clusterDims.z;
	uint clusterId = (dtid.x + dtid.y * numClustersX ) + dtid.z * ( numClustersX * numClustersY );
	
	if ( dtid.x >= numClustersX || dtid.y >= numClustersY || dtid.z >= numClustersZ )
	{
		return;
	}
	
	clusters[clusterId].numLights = 0;
	uint numLights = 0;
	
	float invClusterDimsX = 1.0f / float( numClustersX );
	float invClusterDimsY = 1.0f / float( numClustersY );
	float invClusterDimsZ = 1.0f / float( numClustersZ );
		
	float2 uvMin = float2( dtid.x, dtid.y ) / float2( numClustersX, numClustersY );
	float2 uvMax = ( float2( dtid.x, dtid.y ) + 1.0f ) / float2( numClustersX, numClustersY );
	
	float3 viewRays[4];
	viewRays[0] = uvToViewRay( float2( uvMin.x, uvMin.y ) );
	viewRays[1] = uvToViewRay( float2( uvMax.x, uvMin.y ) );
	viewRays[2] = uvToViewRay( float2( uvMin.x, uvMax.y ) );
	viewRays[3] = uvToViewRay( float2( uvMax.x, uvMax.y ) );
	
	// https://www.aortiz.me/2018/12/21/CG.html
	float slicePortionNear = float( dtid.z ) * invClusterDimsZ;
	float slicePortionFar = float( dtid.z + 1 ) * invClusterDimsZ;
	const float near = 0.1f;
	const float far = 1000.0f;
	
	float nearZ = near * pow( far / near, slicePortionNear );
	float farZ = near * pow( far / near, slicePortionFar );
	
	ClusterBBOX bbox;
	bbox.max = float3( -1e30, -1e30, -1e30 );
	bbox.min = -bbox.max;
	
	for ( uint i = 0; i < 4; i++ )
	{
		float3 minPoint = viewRays[i] * nearZ; // 0.01f * -tan(fovY/2) * aspect at uv(0, 0)
		float3 maxPoint = viewRays[i] * farZ; // 1000.0f * -tan(fovY/2) * aspect at uv(0, 0)
		
		bbox.min = min( bbox.min, min( minPoint, maxPoint ) );
		bbox.max = max( bbox.max, max( minPoint, maxPoint ) );
	}
	
	for ( uint i = 0 ; i < data.numPointLights && numLights < MAX_LIGHTS_IN_CLUSTER; i++ )
	{	
		if ( isLightInCluster( bbox, pointLights[i] ) )
		{
			clusters[clusterId].pointLightIndices[numLights++] = i;
		}
	}
	
	clusterBBOX[clusterId] = bbox;
	
	clusters[clusterId].numLights = numLights;
}
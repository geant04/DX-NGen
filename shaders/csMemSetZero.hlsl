#include "CommonBuffers.hlsli"

// For now, we'll simply assume the buffer is zero. We can always rewrite this later to be more generalized!
RWStructuredBuffer<IndirectCommand> buffer : register(u0, space1);

cbuffer RootConstants : register(b0)
{
    uint numElements;
};

#define CS_MEMSET_ZERO 8

[numthreads( CS_MEMSET_ZERO, 1, 1 )]
void main( uint3 dtid : SV_DispatchThreadID )
{
	if ( dtid.x >= numElements )
	{
		return;
	}
	
	buffer[dtid.x].instanceCount = 0;
}
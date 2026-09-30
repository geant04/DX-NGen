struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

float4 PSMain(PSInput input) : SV_TARGET
{
	return float4( input.normal, 1.0f );
}
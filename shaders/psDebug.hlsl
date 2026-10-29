Texture2D inputTexture : register(t0);
SamplerState texSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

float4 PSMain( PSInput input ) : SV_TARGET
{
	float depthValue = inputTexture.Sample( texSampler, input.texCoord ).r;
	// depthValue = pow(depthValue, 4.0f); // arbitrary assignment
	
    return float4( depthValue, depthValue, depthValue, 1.0f );
}
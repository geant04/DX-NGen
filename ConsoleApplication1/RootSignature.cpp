#include "RootSignature.h"
#include <iostream>

namespace Gfx 
{

void BuildRootSignature( ComPtr<ID3D12Device> device, Task* task )
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	std::vector<CD3DX12_DESCRIPTOR_RANGE1> descriptorRanges = {};
	std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters = {};
	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplerDescs = {};

	descriptorRanges.reserve(4);
	rootParameters.reserve(4);
	staticSamplerDescs.reserve(16);

	int descriptorRangeCount = 0;
	int descriptorRootParamsCount = 0;
	int readTexturesCount = task->readTextures.size();
	int readWriteTexturesCount = task->readWriteTextures.size();
	int dynamicBuffersCount = task->dynamicBuffers.size();
	int buffersCount = task->buffers.size();
	int staticSamplersCount = task->staticSamplers.size();

	if ( readTexturesCount > 0 )
	{
		// t0
		descriptorRanges.push_back({});
		descriptorRanges[descriptorRangeCount].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, readTexturesCount, 0, 0 );

		rootParameters.push_back({});
		rootParameters[descriptorRootParamsCount++].InitAsDescriptorTable(1, &descriptorRanges[descriptorRangeCount++] );
	}
	if ( buffersCount > 0 )
	{
		// t0, space1
		descriptorRanges.push_back( {} );
		descriptorRanges[descriptorRangeCount].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, buffersCount, 0, 1 );

		rootParameters.push_back( {} );
		rootParameters[descriptorRootParamsCount++].InitAsDescriptorTable( 1, &descriptorRanges[descriptorRangeCount++] );
	}
	if ( readWriteTexturesCount > 0 )
	{
		// u0
		descriptorRanges.push_back({});
		descriptorRanges[descriptorRangeCount].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, readWriteTexturesCount, 0, 0);
		
		rootParameters.push_back({});
		rootParameters[descriptorRootParamsCount++].InitAsDescriptorTable(1, &descriptorRanges[descriptorRangeCount++] );
	}
	// add support for normal buffers
	if ( dynamicBuffersCount > 0 )
	{
		// b0
		descriptorRanges.push_back({});
		descriptorRanges[descriptorRangeCount].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, dynamicBuffersCount, 0, 0);
		
		rootParameters.push_back({});
		rootParameters[descriptorRootParamsCount++].InitAsDescriptorTable(1, &descriptorRanges[descriptorRangeCount++] );
	}
	if ( staticSamplersCount > 0 )
	{
		unsigned int samplerIndex = 0;
		for ( Sampler* sampler : task->staticSamplers )
		{
			D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
			samplerDesc.Filter = sampler->filterMode;
			samplerDesc.AddressU = sampler->addressMode;
			samplerDesc.AddressV = sampler->addressMode;
			samplerDesc.AddressW = sampler->addressMode;
			samplerDesc.ComparisonFunc = sampler->comparisonFunc;;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderRegister = samplerIndex++;
			staticSamplerDescs.push_back(samplerDesc);
		}
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	const D3D12_ROOT_PARAMETER1* rootParameterPtr = ( descriptorRootParamsCount > 0 ) ? &rootParameters[0] : nullptr;
	const D3D12_STATIC_SAMPLER_DESC* staticSamplerDescPtr = ( staticSamplersCount > 0 ) ? &staticSamplerDescs[0] : nullptr;
	D3D12_ROOT_SIGNATURE_FLAGS signatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	if ( task->getType() == Draw )
	{
		signatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	rootSignatureDesc.Init_1_1( descriptorRootParamsCount, rootParameterPtr, staticSamplersCount, staticSamplerDescPtr, signatureFlags );
	

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error);

	if (FAILED(hr)) {
		std::cout << hr << std::endl;
		if ( error )
		{
			OutputDebugStringA((const char*)error->GetBufferPointer());
		}
		return std::perror("[ERROR CS]: Cannot serialize versioned root signature");
	}

	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&task->rootSignature));
	if (FAILED(hr)) {
		return std::perror("[ERROR CS]: Cannot create PT root signature");
	}
}

}
#pragma once

#include "Common.h"

namespace Pipeline {
	
void createPSO(
	const ComPtr<ID3D12Device> &device, 
	ComPtr<ID3D12RootSignature> &rootSignature, 
	ComPtr<ID3D12PipelineState> &outPSO,
	const char* vsAssetName,
	const char* psAssetName
);

void createComputePSO(
	ComPtr<ID3D12Device> &device, 
	ComPtr<ID3D12RootSignature> &rootSignature, 
	ComPtr<ID3D12PipelineState> &outPSO,
	const char* csAssetName,
	const char* entryPoint
);

}

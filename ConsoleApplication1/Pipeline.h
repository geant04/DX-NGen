#pragma once

#include "Common.h"
#include "Task.h"

namespace Gfx {
	
void CreatePSO( const ComPtr<ID3D12Device> &device, DrawTask* drawTask );

void CreateComputePSO(
	ComPtr<ID3D12Device> &device, 
	ComPtr<ID3D12RootSignature> &rootSignature, 
	ComPtr<ID3D12PipelineState> &outPSO,
	const char* csAssetName,
	const char* entryPoint
);

}

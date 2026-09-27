#pragma once

#include "Common.h"
#include "Task.h"

namespace Gfx
{
	
void createRootSignature(
	ComPtr<ID3D12Device> &mDevice,
	ComPtr<ID3D12RootSignature> &outRootSignature
);

void createPathTracingRootSignature(
	ComPtr<ID3D12Device> &mDevice,
	ComPtr<ID3D12RootSignature> &outRootSignature
);

void createBlitRootSignature(
	const ComPtr<ID3D12Device> &mDevice,
	ComPtr<ID3D12RootSignature> &outRootSignature
);

void BuildComputeRootSignature( ComPtr<ID3D12Device> device, ComputeTask* task );

};


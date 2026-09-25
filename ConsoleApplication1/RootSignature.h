#pragma once

#include "Common.h"

namespace RootSignature
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

};


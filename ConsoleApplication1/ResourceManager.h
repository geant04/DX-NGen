#pragma once

#include "Common.h"

// Backend resource manager setup
namespace ResourceManager
{
	// DX12 resource management scheme, handles everything from an arbitrary amount of RTs, SRV/UAV/CBVs, etc.

	struct DescriptorHeap
	{
		ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
		uint32_t descriptorSize = 0;
		uint32_t handleIndex = 0;

		// We can probably create a ring-buffer of some sort for easy insert/free
		uint32_t capacity;
	};

	struct GlobalDescriptorHeaps
	{
		DescriptorHeap rtvGlobalHeap = {};
		DescriptorHeap shaderResourceGlobalHeap = {};
		DescriptorHeap samplerGlobalHeap = {};
	};

	struct Resources
	{
		GlobalDescriptorHeaps globalDescriptorHeap;
	};


	// Build global descriptor heaps
	// - Allocate a global amount of resources that we can re-use
	// - global samplers
	// - global SRV texture slots
	// - global CBV texture slots
	void CreateGlobalDescriptorHeaps( ComPtr<ID3D12Device> device, GlobalDescriptorHeaps &globalDescriptorHeaps );
	
	// Will automatically handle finding the optimal rtv descriptor handle
	bool AllocateRenderTargetView( ComPtr<ID3D12Device> device, const DescriptorHeap *rtvGlobalHeap, const ComPtr<ID3D12Resource> renderTarget );


	struct Texture
	{
	
	};

	struct Buffer
	{

	};


} // namespace ResourceManager
#pragma once

#include "Common.h"
#include "ResourceHandles.h"

#define SWAPCHAIN_COUNT 2

namespace Gfx
{
	// DX12 resource management scheme, handles everything from an arbitrary amount of RTs, SRV/UAV/CBVs, etc.

	enum HeapType
	{
		RTV_HEAP, SHADER_RESOURCE_HEAP, SAMPLER_HEAP
	};

	struct DescriptorHeap
	{
		ComPtr<ID3D12DescriptorHeap> descriptorHeapPtr = nullptr;
		uint32_t descriptorSize = 0;
		uint32_t handleIndex = 0;

		// We can probably create a ring-buffer of some sort for easy insert/free
		uint32_t capacity;

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle()
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = descriptorHeapPtr->GetCPUDescriptorHandleForHeapStart();
			cpuDescriptorHandle.ptr += handleIndex * descriptorSize;
		
			return cpuDescriptorHandle;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle()
		{
			D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle = descriptorHeapPtr->GetGPUDescriptorHandleForHeapStart();
			gpuDescriptorHandle.ptr += handleIndex * descriptorSize;

			return gpuDescriptorHandle;
		}
	};

	struct GlobalDescriptorHeaps
	{
		DescriptorHeap rtvGlobalHeap = {};
		DescriptorHeap shaderResourceGlobalHeap = {};
		DescriptorHeap samplerGlobalHeap = {};
	};

	struct Texture
	{
		ComPtr<ID3D12Resource> resource;

		DXGI_FORMAT format;
		uint32_t mips;

		DescriptorIndex srv;
		DescriptorIndex uav;
		DescriptorIndex rtv;
	};

	struct Buffer
	{
		ComPtr<ID3D12Resource> resource;

		UINT sizeInBytes;
		UINT cpuDataSize;

		UINT8* mappedBufferData;
		void* cpuBufferData;

		DescriptorIndex srv;
		DescriptorIndex uav;
		DescriptorIndex cbv;
	};

	struct Sampler
	{
		D3D12_FILTER filterMode;
		D3D12_TEXTURE_ADDRESS_MODE addressMode;
	};

	struct Resources
	{
		GlobalDescriptorHeaps globalDescriptorHeap;

		DescriptorHeap* GetDescriptorFromType( HeapType type )
		{
			switch( type )
			{
				case RTV_HEAP:
					return &globalDescriptorHeap.rtvGlobalHeap;
				case SHADER_RESOURCE_HEAP:
					return &globalDescriptorHeap.shaderResourceGlobalHeap;
				case SAMPLER_HEAP:
					return &globalDescriptorHeap.samplerGlobalHeap;
				default:
					return nullptr;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle( const DescriptorIndex& descriptorIndex, HeapType type )
		{
			DescriptorHeap* descriptorHeap = GetDescriptorFromType( type );
			if ( descriptorHeap == nullptr )
			{
				return {};
			}

			D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = 
				descriptorHeap->descriptorHeapPtr->GetCPUDescriptorHandleForHeapStart();
		
			cpuDescriptorHandle.ptr += descriptorIndex.index * descriptorHeap->descriptorSize;
		
			return cpuDescriptorHandle;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle( const DescriptorIndex& descriptorIndex, HeapType type )
		{
			DescriptorHeap* descriptorHeap = GetDescriptorFromType( type );
			if ( descriptorHeap == nullptr )
			{
				return {};
			}

			D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle =
				descriptorHeap->descriptorHeapPtr->GetGPUDescriptorHandleForHeapStart();

			gpuDescriptorHandle.ptr += descriptorIndex.index * descriptorHeap->descriptorSize;

			return gpuDescriptorHandle;
		}

		// Global resources
		Texture swapChainTextures[SWAPCHAIN_COUNT];
	};


	// Build global descriptor heaps
	// - Allocate a global amount of resources that we can re-use
	// - global samplers
	// - global SRV texture slots
	// - global CBV texture slots
	void CreateGlobalDescriptorHeaps( ComPtr<ID3D12Device> device, GlobalDescriptorHeaps &globalDescriptorHeaps );
	
	// Allocate funcs
	bool AllocateRenderTargetView( ComPtr<ID3D12Device> device, const DescriptorHeap *rtvGlobalHeap, Texture* texture );
	bool AllocateShaderResourceView( ComPtr<ID3D12Device> device, const DescriptorHeap *shaderResourceHeap, Texture* texture );

	// General purpose resource creation functions
	void CreateSwapChainRTVs( ComPtr<ID3D12Device> device, ComPtr<IDXGISwapChain4> swapChain, Resources* resources, uint32_t frameCount );

	bool CreateReadTexture( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips );
	
	bool CreateRWTexture( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips );
	
	bool CreateConstantBuffer( ComPtr<ID3D12Device> device, Buffer* buffer, Resources* resources, void* data, const UINT sizeInBytes, const UINT cpuDataSize );
} // namespace ResourceManager
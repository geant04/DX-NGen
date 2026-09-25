#include "ResourceManager.h"

namespace ResourceManager
{
	const uint32_t MAX_RTV_DESCRIPTORS				= 8;
	const uint32_t MAX_SHADER_RESOURCE_DESCRIPTORS	= 64;
	const uint32_t MAX_SAMPLER_DESCRIPTORS			= 4;

	void CreateGlobalDescriptorHeaps( ComPtr<ID3D12Device> device, GlobalDescriptorHeaps &globalDescriptorHeaps )
	{
		// RTVs
		{
			D3D12_DESCRIPTOR_HEAP_DESC RTVDescriptorHeapDesc = {};
			RTVDescriptorHeapDesc.NumDescriptors = MAX_RTV_DESCRIPTORS;
			RTVDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			RTVDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			DescriptorHeap *rtvGlobalHeap = &globalDescriptorHeaps.rtvGlobalHeap;

			device->CreateDescriptorHeap(&RTVDescriptorHeapDesc, IID_PPV_ARGS(&rtvGlobalHeap->descriptorHeap));
			rtvGlobalHeap->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// SRVs/UAVs/CBVs
		{
			D3D12_DESCRIPTOR_HEAP_DESC shaderResourceDescriptorHeapDesc = {};
			shaderResourceDescriptorHeapDesc.NumDescriptors = MAX_SHADER_RESOURCE_DESCRIPTORS;
			shaderResourceDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			shaderResourceDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			
			DescriptorHeap *shaderResourceGlobalHeap = &globalDescriptorHeaps.shaderResourceGlobalHeap;

			device->CreateDescriptorHeap(&shaderResourceDescriptorHeapDesc, IID_PPV_ARGS(&shaderResourceGlobalHeap->descriptorHeap));
			shaderResourceGlobalHeap->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		// Static samplers
		{
			D3D12_DESCRIPTOR_HEAP_DESC samplerDescriptorHeapDesc = {};
			samplerDescriptorHeapDesc.NumDescriptors = MAX_SAMPLER_DESCRIPTORS;
			samplerDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			samplerDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			DescriptorHeap *samplerGlobalHeap = &globalDescriptorHeaps.samplerGlobalHeap;
			
			device->CreateDescriptorHeap(&samplerDescriptorHeapDesc, IID_PPV_ARGS(&samplerGlobalHeap->descriptorHeap));
			samplerGlobalHeap->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
	}


	bool AllocateRenderTargetView( ComPtr<ID3D12Device> device, DescriptorHeap &rtvGlobalHeap, const ComPtr<ID3D12Resource> renderTarget )
	{
		uint32_t *handleIndex = &rtvGlobalHeap.handleIndex;
		
		if ( *handleIndex >= MAX_RTV_DESCRIPTORS )
		{
			// log error: too full
			return false;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvGlobalHeap.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += *handleIndex * rtvGlobalHeap.descriptorSize;

		device->CreateRenderTargetView(renderTarget.Get(), nullptr, rtvHandle);
		handleIndex += rtvGlobalHeap.descriptorSize;

		return true;
	}

}
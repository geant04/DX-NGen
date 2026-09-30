#include "ResourceManager.h"

namespace Gfx
{
	// Global resource count! You should never touch this unless you have to.
	const uint32_t MAX_RTV_DESCRIPTORS				= 8;
	const uint32_t MAX_DSV_DESCRIPTORS				= 4;
	const uint32_t MAX_SHADER_RESOURCE_DESCRIPTORS	= 64;
	const uint32_t MAX_SAMPLER_DESCRIPTORS			= 4;

	void CreateGlobalDescriptorHeaps( ComPtr<ID3D12Device> device, GlobalDescriptorHeaps &globalDescriptorHeaps )
	{
		// RTVs
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
			rtvDescriptorHeapDesc.NumDescriptors = MAX_RTV_DESCRIPTORS;
			rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			DescriptorHeap *rtvGlobalHeap = &globalDescriptorHeaps.rtvGlobalHeap;

			device->CreateDescriptorHeap( &rtvDescriptorHeapDesc, IID_PPV_ARGS( &rtvGlobalHeap->descriptorHeapPtr ) );
			rtvGlobalHeap->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			
			rtvGlobalHeap->capacity = MAX_RTV_DESCRIPTORS;
		}

		// DSVs
		{
			D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
			dsvDescriptorHeapDesc.NumDescriptors = MAX_DSV_DESCRIPTORS;
			dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			DescriptorHeap *dsvGlobalHeap = &globalDescriptorHeaps.dsvGlobalHeap;

			device->CreateDescriptorHeap( &dsvDescriptorHeapDesc, IID_PPV_ARGS( &dsvGlobalHeap->descriptorHeapPtr ) );
			dsvGlobalHeap->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			
			dsvGlobalHeap->capacity = MAX_DSV_DESCRIPTORS;
		}

		// SRVs/UAVs/CBVs
		{
			D3D12_DESCRIPTOR_HEAP_DESC shaderResourceDescriptorHeapDesc = {};
			shaderResourceDescriptorHeapDesc.NumDescriptors = MAX_SHADER_RESOURCE_DESCRIPTORS;
			shaderResourceDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			shaderResourceDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			
			DescriptorHeap *shaderResourceGlobalHeap = &globalDescriptorHeaps.shaderResourceGlobalHeap;

			device->CreateDescriptorHeap(&shaderResourceDescriptorHeapDesc, IID_PPV_ARGS(&shaderResourceGlobalHeap->descriptorHeapPtr));
			shaderResourceGlobalHeap->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			
			shaderResourceGlobalHeap->capacity = MAX_SHADER_RESOURCE_DESCRIPTORS;
		}

		// Static samplers
		{
			D3D12_DESCRIPTOR_HEAP_DESC samplerDescriptorHeapDesc = {};
			samplerDescriptorHeapDesc.NumDescriptors = MAX_SAMPLER_DESCRIPTORS;
			samplerDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			samplerDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			DescriptorHeap *samplerGlobalHeap = &globalDescriptorHeaps.samplerGlobalHeap;
			
			device->CreateDescriptorHeap(&samplerDescriptorHeapDesc, IID_PPV_ARGS(&samplerGlobalHeap->descriptorHeapPtr));
			samplerGlobalHeap->descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			
			samplerGlobalHeap->capacity = MAX_SAMPLER_DESCRIPTORS;
		}
	}

	bool AllocateRenderTargetView( ComPtr<ID3D12Device> device, DescriptorHeap *rtvGlobalHeap, Texture* texture )
	{
		device->CreateRenderTargetView( texture->resource.Get(), nullptr, rtvGlobalHeap->GetCpuHandle() );

		texture->rtv.index = rtvGlobalHeap->handleIndex;
		rtvGlobalHeap->handleIndex++;

		return true;
	}

	bool AllocateDepthStencilView( ComPtr<ID3D12Device> device, DescriptorHeap *dsvGlobalheap, Texture* texture )
	{
		device->CreateDepthStencilView( texture->resource.Get(), nullptr, dsvGlobalheap->GetCpuHandle() );

		texture->dsv.index = dsvGlobalheap->handleIndex;
		dsvGlobalheap->handleIndex++;

		return true;
	}

	bool AllocateShaderResourceView( ComPtr<ID3D12Device> device, DescriptorHeap *shaderResourceHeap, Texture* texture )
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texture->format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = texture->mips;

		device->CreateShaderResourceView( texture->resource.Get(), &srvDesc, shaderResourceHeap->GetCpuHandle());

		texture->srv.index = shaderResourceHeap->handleIndex;
		shaderResourceHeap->handleIndex++;

		return true;
	}
	
	bool AllocateUnorderedAccessView( ComPtr<ID3D12Device> device, DescriptorHeap *shaderResourceHeap, Texture* texture )
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = texture->format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		device->CreateUnorderedAccessView( texture->resource.Get(), nullptr, &uavDesc, shaderResourceHeap->GetCpuHandle());

		texture->uav.index = shaderResourceHeap->handleIndex;
		shaderResourceHeap->handleIndex++;

		return true;
	}

	bool AllocateConstantBufferView( ComPtr<ID3D12Device> device, DescriptorHeap *shaderResourceHeap, Buffer* buffer )
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = buffer->resource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = buffer->sizeInBytes;

		device->CreateConstantBufferView(&cbvDesc, shaderResourceHeap->GetCpuHandle());

		buffer->cbv.index = shaderResourceHeap->handleIndex;
		shaderResourceHeap->handleIndex++;

		return true;
	}

	void CreateSwapChainRTVs( ComPtr<ID3D12Device> device, ComPtr<IDXGISwapChain4> swapChain, Resources* resources, uint32_t frameCount )
	{
		DescriptorHeap* rtvDescriptorHeap = &resources->globalDescriptorHeap.rtvGlobalHeap;

		for (uint32_t i = 0; i < frameCount; i++)
		{
			Texture* swapChainFrameRTV = &resources->swapChainTextures[i];
			swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainFrameRTV->resource));

			AllocateRenderTargetView( device, rtvDescriptorHeap, swapChainFrameRTV );
		}
	}

	// Creates a 2D texture by default
	static Texture CreateTextureInternal( ComPtr<ID3D12Device> device, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips )
	{
		Texture texture;
		texture.format = format;
		texture.mips = mips;
		 
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Format = format;
		textureDesc.MipLevels = mips;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // need to change this
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		
		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = format;
		clearValue.Color[0] = 0.0f;
		clearValue.Color[1] = 0.2f;
		clearValue.Color[2] = 0.4f;
		clearValue.Color[3] = 1.0f;

		// Temporarily stage memory CPU/GPU
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(&texture.resource)
		);

		// Microsoft keeps using this weird wchar_t stuff and it makes it a lot harder than it should be
		texture.resource->SetName(L"Texture");

		return texture;
	}

	static Texture CreateDepthStencilTextureInternal( ComPtr<ID3D12Device> device, Resources* resources, bool isDepth, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips )
	{
		Texture texture;
		texture.format = format;
		texture.mips = mips;

		if ( !isDepth )
		{
			// TODO: need to check if the format is correct for stencil!
		}

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Format = format;
		textureDesc.MipLevels = mips;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = format;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(&texture.resource)
		);

		texture.resource->SetName(L"Depth/Stencil Texture");

		return texture;
	}

	static Buffer CreateBufferInternal( ComPtr<ID3D12Device> device, void* data, const UINT sizeInBytes, const UINT cpuDataSize )
	{
		Buffer buffer = {};
		buffer.sizeInBytes = sizeInBytes;
		buffer.cpuDataSize = cpuDataSize;
		buffer.cpuBufferData = data;

		CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC cameraCBDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeInBytes);
		device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&cameraCBDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer.resource)
		);

		buffer.resource->SetName(L"Buffer");

		D3D12_RANGE constantBufferReadRange = { 0, 0 };
		buffer.resource->Map( 0, &constantBufferReadRange, reinterpret_cast<void**>(&buffer.mappedBufferData ) );
		memcpy( buffer.mappedBufferData, data, cpuDataSize );

		return buffer;
	}

	bool CreateReadTexture( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips )
	{
		if ( texture == nullptr )
		{
			return false;
		}

		if ( texture->resource == nullptr )
		{
			*texture = CreateTextureInternal( device, resources, width, height, format, mips );
		}

		return AllocateShaderResourceView( device, &resources->globalDescriptorHeap.shaderResourceGlobalHeap, texture );
	}

	bool CreateRWTexture( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips )
	{
		if ( texture == nullptr )
		{
			return false;
		}

		if ( texture->resource == nullptr )
		{
			*texture = CreateTextureInternal( device, resources, width, height, format, mips );
		}

		return AllocateUnorderedAccessView( device, &resources->globalDescriptorHeap.shaderResourceGlobalHeap, texture );
	}

	bool CreateRTV( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips )
	{
		if ( texture == nullptr )
		{
			return false;
		}

		if ( texture->resource == nullptr )
		{
			*texture = CreateTextureInternal( device, resources, width, height, format, mips );
		}

		return AllocateRenderTargetView( device, &resources->globalDescriptorHeap.rtvGlobalHeap, texture );
	}

	bool CreateConstantBuffer( ComPtr<ID3D12Device> device, Buffer* buffer, Resources* resources, void* data, const UINT sizeInBytes, const UINT cpuDataSize )
	{
		if ( buffer == nullptr )
		{
			return false;
		}

		if ( buffer->resource == nullptr )
		{
			*buffer = CreateBufferInternal( device, data, sizeInBytes, cpuDataSize );
		}

		return AllocateConstantBufferView( device, &resources->globalDescriptorHeap.shaderResourceGlobalHeap, buffer );
	}

	bool CreateRenderTargetFromTexture( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources )
	{
		if ( texture == nullptr )
		{
			return false;
		}

		return AllocateRenderTargetView( device, &resources->globalDescriptorHeap.rtvGlobalHeap, texture );
	}

	static bool CreateDepthStencilTexture( bool isDepth, ComPtr<ID3D12Device> device, Texture* texture, uint32_t width, uint32_t height, Resources* resources, DXGI_FORMAT format, uint32_t mips )
	{
		if ( texture == nullptr )
		{
			return false;
		}

		if ( texture->resource == nullptr )
		{
			*texture = CreateDepthStencilTextureInternal( device, resources, isDepth, width, height, format, mips );
		}

		return AllocateDepthStencilView( device, &resources->globalDescriptorHeap.dsvGlobalHeap, texture );
	}

	bool CreateDepthTexture( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips )
	{
		return CreateDepthStencilTexture( true, device, texture, width, height, resources, format, mips );
	}

	bool CreateStencilTexture( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t mips )
	{
		return CreateDepthStencilTexture( false, device, texture, width, height, resources, format, mips );
	}

	bool CreateTextureFromImage( ComPtr<ID3D12Device> device, Texture* texture, Resources* resources, const char* file )
	{
		if ( texture != nullptr )
		{
			return false;
		}

		// fresh texture, by default should be SRV
		

		return true;
	}


	Texture* AddTexture( Resources* resources )
	{
		return &resources->textures[resources->textureCount++];
	}

	Buffer* AddBuffer( Resources* resources )
	{
		return &resources->buffers[resources->bufferCount++];
	}

	Sampler* AddSampler( Resources* resources )
	{
		return &resources->samplers[resources->samplerCount++];
	}

	Mesh* AddMesh( Resources* resources )
	{
		return &resources->meshes[resources->meshCount++];
	}

	Model* AddModel( Resources* resources )
	{
		return &resources->models[resources->modelCount++];
	}
}
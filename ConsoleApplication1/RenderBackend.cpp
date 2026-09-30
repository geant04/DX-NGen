#include "RenderBackend.h"

#include "Common.h"
#include "RootSignature.h"
#include "Pipeline.h"

namespace Gfx
{
	static void UploadMeshBuffer( MinimalDXApp* app, DrawTask* drawTask )
	{
		const Mesh* mesh = drawTask->model->mesh;
		const ComPtr<ID3D12Device> device = app->device;

		// Upload CPU to GPU memory using defaultHeap
		// Should we share the same heap when uploading info?
		CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(mesh->vertexBufferSize);

		// Vertex buffer will reside on the GPU as a default heap
		device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&drawTask->vertexBuffer)
		);
		
		// Upload heap on CPU, sends data to vertex buffer on GPU
		// The purpose for this is to prepare our buffer data on system RAM to be uploaded to the GPU later
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		
		device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&drawTask->vertexBufferUpload)
		);

		// Copy memory to upload heap
		// We're strictly writing, so no need to define a read range.
		UINT8* pVertexDataBegin;
		D3D12_RANGE readRange = {0, 0};
		drawTask->vertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
		memcpy(pVertexDataBegin, mesh->vertices.data(), mesh->vertexBufferSize);
		drawTask->vertexBufferUpload->Unmap(0, nullptr);
		drawTask->vertexBufferView.BufferLocation = drawTask->vertexBuffer->GetGPUVirtualAddress();
		drawTask->vertexBufferView.StrideInBytes = sizeof(Vertex);
		drawTask->vertexBufferView.SizeInBytes = mesh->vertexBufferSize;

		// We need to make the command list that'll execute based off of the PSO we made
		app->device->CreateCommandList(
			0, 
			D3D12_COMMAND_LIST_TYPE_DIRECT, 
			app->commandAllocator.Get(), 
			drawTask->pipelineState.Get(), 
			IID_PPV_ARGS(&app->commandList)
		);
		
		app->commandList->Close();

		// Execute the buffer upload
		app->commandAllocator->Reset();
		app->commandList->Reset(app->commandAllocator.Get(), drawTask->pipelineState.Get());

		app->commandList->CopyBufferRegion(
			drawTask->vertexBuffer.Get(), 0,
			drawTask->vertexBufferUpload.Get(), 0,
			mesh->vertexBufferSize
		);
		
		// We were originally D3D12_RESOURCE_STATE_COPY_DEST,
		// need to transition this to a vertex buffer so the shader can interpret the resource properly I assume
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			drawTask->vertexBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		);

		app->commandList->ResourceBarrier(1, &barrier);

		app->commandList->Close();

		ID3D12CommandList* ppCommandList[] = { app->commandList.Get() };
		app->commandQueue->ExecuteCommandLists(_countof(ppCommandList), ppCommandList);
	}

	static bool PrepareDescriptorTable( 
		ComPtr<ID3D12Device> device,
		Resources* resources,
		const std::vector<DescriptorIndex>& sourceDescriptors,
		PreparedDescriptorTable* outTable
	)
	{
		if ( device == nullptr || resources == nullptr || outTable == nullptr )
		{
			return false;
		}

		*outTable = {};

		if ( sourceDescriptors.empty() )
		{
			return true;
		}

		Gfx::DescriptorHeap* heap = &resources->globalDescriptorHeap.shaderResourceGlobalHeap;

		if ( heap->descriptorHeapPtr == nullptr )
		{
			return false;
		}

		const uint32_t descriptorCount = static_cast<uint32_t>( sourceDescriptors.size() );

		if ( heap->handleIndex + descriptorCount > heap->capacity )
		{
			return false;
		}

		const uint32_t destinationIndex = heap->handleIndex;

		DescriptorIndex destinationDescriptor;
		destinationDescriptor.index = destinationIndex;

		D3D12_CPU_DESCRIPTOR_HANDLE destinationStart = resources->GetCpuHandle( destinationDescriptor, Gfx::SHADER_RESOURCE_HEAP );

		for ( uint32_t i = 0; i < descriptorCount; i++ )
		{
			if ( !sourceDescriptors[i].IsValid() )
			{
				return false;
			}
		}

		heap->handleIndex += descriptorCount;
		
		for ( uint32_t i = 0; i < descriptorCount; i++ )
		{
			D3D12_CPU_DESCRIPTOR_HANDLE sourceHandle = resources->GetCpuHandle( sourceDescriptors[i], Gfx::SHADER_RESOURCE_HEAP );
			D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle = destinationStart;
			destinationHandle.ptr += static_cast<SIZE_T>(i) * heap->descriptorSize;

			device->CopyDescriptorsSimple( 1, destinationHandle, sourceHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
		}

		outTable->gpuHandle = resources->GetGpuHandle( destinationDescriptor, Gfx::SHADER_RESOURCE_HEAP );
		outTable->descriptorCount = descriptorCount;

		return true;
	}

	static bool PrepareBindings( MinimalDXApp* app, Task* task )
	{
		if ( app != nullptr && task != nullptr )
		{
			std::vector<DescriptorIndex> readDescriptorIndices;
			std::vector<DescriptorIndex> readWriteDescriptorIndices;
			std::vector<DescriptorIndex> bufferDescriptorIndices;
			std::vector<DescriptorIndex> dynamicBufferDescriptorIndices;

			for ( Texture* readTexture : task->readTextures )
			{
				if ( readTexture == nullptr || !readTexture->srv.IsValid() )
				{
					return false;
				}
				readDescriptorIndices.push_back( { readTexture->srv.index } );
			}

			for ( Texture* readWriteTexture : task->readWriteTextures )
			{
				if ( readWriteTexture == nullptr || !readWriteTexture->uav.IsValid() )
				{
					return false;
				}
				readWriteDescriptorIndices.push_back( { readWriteTexture->uav.index } );
			}
			
			for ( Gfx::Buffer* buffer : task->buffers )
			{
				if ( buffer == nullptr || !buffer->srv.IsValid() )
				{
					return false;

				}
				bufferDescriptorIndices.push_back( { buffer->srv.index } );
			}

			for ( Gfx::Buffer* dynamicBuffer : task->dynamicBuffers )
			{
				if ( dynamicBuffer == nullptr || !dynamicBuffer->cbv.IsValid() )
				{
					return false;
				}
				dynamicBufferDescriptorIndices.push_back( { dynamicBuffer->cbv.index } );
			}

			if ( !PrepareDescriptorTable( app->device, &app->resources, readDescriptorIndices,			&task->bindings.readTextures ) )		return false;
			if ( !PrepareDescriptorTable( app->device, &app->resources, readWriteDescriptorIndices,		&task->bindings.readWriteTextures ) )	return false;
			if ( !PrepareDescriptorTable( app->device, &app->resources, bufferDescriptorIndices,		&task->bindings.buffers ) )				return false;
			if ( !PrepareDescriptorTable( app->device, &app->resources, dynamicBufferDescriptorIndices,	&task->bindings.dynamicBuffers ) )		return false;
		}
		else
		{
			return false;
		}

		return true;
	}

	bool CreateDrawTask( MinimalDXApp* app, DrawTask* drawTask )
	{
		if ( drawTask == nullptr )
		{
			return false;
		}

		drawTask->descriptorHeap = &app->resources.globalDescriptorHeap.shaderResourceGlobalHeap;

		if ( !PrepareBindings( app, drawTask ) )
		{
			return false;
		}

		Gfx::BuildRootSignature( app->device, drawTask );

		Gfx::CreatePSO( app->device, drawTask );

		{
			Gfx::UploadMeshBuffer( app, drawTask );
		
			app->WaitForPreviousFrame();

			// Reset this until we need to do another vertex buffer upload, not needed anymore since we copied relevant info over
			drawTask->vertexBufferUpload.Reset();
		}

		return true;
	}

	bool CreateComputeTask( MinimalDXApp* app, ComputeTask* computeTask )
	{
		if ( computeTask == nullptr )
		{
			return false;
		}

		computeTask->descriptorHeap = &app->resources.globalDescriptorHeap.shaderResourceGlobalHeap;

		if ( !PrepareBindings( app, computeTask ) )
		{
			return false;
		}

		Gfx::BuildRootSignature( app->device, computeTask );

		Gfx::CreateComputePSO(
			app->device,
			computeTask->rootSignature,
			computeTask->pipelineState,
			computeTask->initResources.csAssetName,
			computeTask->initResources.entryPoint
		);

		return true;
	}
}
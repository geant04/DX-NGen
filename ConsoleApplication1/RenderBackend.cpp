#include "RenderBackend.h"

#include "Common.h"
#include "RootSignature.h"
#include "Pipeline.h"

namespace Gfx
{
	static void UploadMeshBuffer( MinimalDXApp* app, DrawTask* drawTask )
	{
		const Mesh* mesh = drawTask->mesh;
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

	bool CreateDrawTask( MinimalDXApp* app, DrawTask* drawTask )
	{
		if ( drawTask == nullptr )
		{
			return false;
		}

		RootSignature::createBlitRootSignature(
			app->device,
			drawTask->rootSignature
		);

		Pipeline::createPSO(
			app->device,
			drawTask->rootSignature,
			drawTask->pipelineState,
			drawTask->initResources.vertexShaderName, //L"vsTriangle.hlsl",
			drawTask->initResources.pixelShaderName //L"psBlit.hlsl"
		);

		// We need to make the command list that'll execute based off of the PSO we made
		app->device->CreateCommandList(
			0, 
			D3D12_COMMAND_LIST_TYPE_DIRECT, 
			app->commandAllocator.Get(), 
			drawTask->pipelineState.Get(), 
			IID_PPV_ARGS(&app->commandList)
		);

		{
			Gfx::UploadMeshBuffer( app, drawTask );
		
			app->WaitForPreviousFrame();

			// Reset this until we need to do another vertex buffer upload, not needed anymore since we copied relevant info over
			drawTask->vertexBufferUpload.Reset();
		}
	}
}
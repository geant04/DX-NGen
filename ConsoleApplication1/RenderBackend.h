#pragma once

#include "MinimalDXApp.h"
#include "Task.h"
#include "ResourceManager.h"

namespace Gfx
{
	bool CreateDrawTask( MinimalDXApp* app, DrawTask* drawTask );
	bool CreateComputeTask( MinimalDXApp* app, ComputeTask* computeTask );
	
	struct UploadMeshBufferInfo
	{
		Vertex* vertices;
		uint32_t vertexBufferSize;

		ComPtr<ID3D12Resource> vertexBuffer = nullptr;
		ComPtr<ID3D12Resource> vertexBufferUpload = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	};

	template<typename T>
	struct UploadBufferInfo
	{
		T* objects;
		uint32_t objectBufferSize;
		
		ComPtr<ID3D12Resource> buffer = nullptr;
		ComPtr<ID3D12Resource> bufferUpload = nullptr;
	};

	template<typename T>
	void UploadBuffer( MinimalDXApp* app, UploadBufferInfo<T>* info, D3D12_RESOURCE_STATES finalState )
	{
		const ComPtr<ID3D12Device> device = app->device;

		CD3DX12_HEAP_PROPERTIES defaultHeapProps( D3D12_HEAP_TYPE_DEFAULT );
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD); // intermediate heap
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer( info->objectBufferSize );

		// Buffer will reside on the GPU as a default heap
		device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS( &info->buffer )
		);

		device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &info->bufferUpload )
		);

		D3D12_SUBRESOURCE_DATA objectData = {};
		objectData.pData = reinterpret_cast<UINT8*>( &info->objects[0] );
		objectData.RowPitch = info->objectBufferSize;
		objectData.SlicePitch = objectData.RowPitch;

		UpdateSubresources<1>( app->uploadCommandList.Get(), info->buffer.Get(), info->bufferUpload.Get(), 0, 0, 1, &objectData );

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			info->buffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			finalState
		);
		app->uploadCommandList->ResourceBarrier(1, &barrier);
	}


	void UploadMeshBuffer( MinimalDXApp* app, UploadMeshBufferInfo* info );

	inline std::shared_ptr<BarrierTask> CreateBarrierTask(
		Gfx::Texture* screenTexture,
		D3D12_RESOURCE_STATES stateBefore,
		D3D12_RESOURCE_STATES stateAfter
	)
	{
		std::shared_ptr<BarrierTask> task = std::make_shared<BarrierTask>();
		task->resource = screenTexture->resource.Get();
		task->stateBefore = stateBefore;
		task->stateAfter = stateAfter;
		return task;
	};
}
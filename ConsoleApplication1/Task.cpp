#include "Task.h"

void DrawTask::execute(
	ComPtr<ID3D12GraphicsCommandList> commandList, 
	ComPtr<ID3D12CommandAllocator> commandAllocator,
    D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
)
{
	// Bind the root signature and PSO before binding any descriptor tables against them
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetPipelineState(pipelineState.Get());

    if ( descriptorHeap != nullptr && descriptorHeap->descriptorHeapPtr != nullptr )
    {
        int bindingSlot = 0;
        if ( bindings.readTextures.descriptorCount > 0 )
        {
             commandList->SetGraphicsRootDescriptorTable(bindingSlot++, bindings.readTextures.gpuHandle);
        }
        if ( bindings.readWriteTextures.descriptorCount > 0 )
        {
             commandList->SetGraphicsRootDescriptorTable(bindingSlot++, bindings.readWriteTextures.gpuHandle);
        }
        if ( bindings.buffers.descriptorCount > 0 )
        {
             commandList->SetGraphicsRootDescriptorTable(bindingSlot++, bindings.buffers.gpuHandle);
        }      
        if ( bindings.dynamicBuffers.descriptorCount > 0 )
        {
             commandList->SetGraphicsRootDescriptorTable(bindingSlot++, bindings.dynamicBuffers.gpuHandle);
        }
    }

    // Bind RTV
    commandList->OMSetRenderTargets(1, rtvHandle, FALSE, nullptr);

    // Bind vertex buffer and draw
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);
}

void BarrierTask::execute(
	ComPtr<ID3D12GraphicsCommandList> commandList, 
	ComPtr<ID3D12CommandAllocator> commandAllocator,
    D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
)
{
    // Barrier transition from PS to UA
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		resource,
		stateBefore,
		stateAfter
	);
    commandList->ResourceBarrier(1, &barrier);
}

void ComputeTask::execute(
	ComPtr<ID3D12GraphicsCommandList> commandList, 
	ComPtr<ID3D12CommandAllocator> commandAllocator,
    D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
)
{
    // Memcpy update info from CPU to GPU?
    for ( Gfx::Buffer* buffer : dynamicBuffers )
    {
        memcpy( buffer->mappedBufferData, buffer->cpuBufferData, buffer->cpuDataSize );
    }

    commandList->SetComputeRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());

    // PSO and root signature setup
    if ( descriptorHeap != nullptr && descriptorHeap->descriptorHeapPtr != nullptr )
    {
        int bindingSlot = 0;
        if ( bindings.readTextures.descriptorCount > 0 )
        {
             commandList->SetComputeRootDescriptorTable(bindingSlot++, bindings.readTextures.gpuHandle);
        }
        if ( bindings.readWriteTextures.descriptorCount > 0 )
        {
             commandList->SetComputeRootDescriptorTable(bindingSlot++, bindings.readWriteTextures.gpuHandle);
        }
        if ( bindings.buffers.descriptorCount > 0 )
        {
             commandList->SetComputeRootDescriptorTable(bindingSlot++, bindings.buffers.gpuHandle);
        }      
        if ( bindings.dynamicBuffers.descriptorCount > 0 )
        {
             commandList->SetComputeRootDescriptorTable(bindingSlot++, bindings.dynamicBuffers.gpuHandle);
        }
    }

    commandList->Dispatch( initResources.dispatchWidth, initResources.dispatchHeight, 1 );
}
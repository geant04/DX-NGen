#include "Task.h"
#include <iostream>

void DrawTask::execute(
	ComPtr<ID3D12GraphicsCommandList> commandList, 
	ComPtr<ID3D12CommandAllocator> commandAllocator,
    Gfx::Resources *resources
)
{
	// Bind the root signature and PSO before binding any descriptor tables against them
	commandList->SetGraphicsRootSignature( rootSignature.Get() );
	commandList->SetPipelineState( pipelineState.Get() );

	for ( Gfx::Buffer* buffer : dynamicBuffers )
	{
		memcpy( buffer->mappedBufferData, buffer->cpuBufferData, buffer->cpuDataSize );
	}

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

	if ( renderTargetHandles.empty() )
    {
		std::cout << "ERROR: empty render targets!" << std::endl;
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE swapChainHandle = resources->GetCpuHandle( resources->swapChainOutputTexture->rtv, Gfx::RTV_HEAP );
	D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandlePtr = outputToBackBuffer ? &swapChainHandle : &renderTargetHandles[0];
	D3D12_CPU_DESCRIPTOR_HANDLE* depthStencilHandlePtr = nullptr;

	if ( clearRenderTarget )
	{
		FLOAT clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		if ( outputToBackBuffer )
		{
			commandList->ClearRenderTargetView( *rtvHandlePtr, clearColor, 0, nullptr);
		}
		else
		{
			for ( D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle : renderTargetHandles )
			{
				commandList->ClearRenderTargetView( rtvHandle, clearColor, 0, nullptr);
			}
		}
	}

	if ( depthStencilTarget != nullptr )
	{
		depthStencilHandlePtr = &depthStencilHandle;
	}

	if ( ( clearDepth || clearStencil ) && ( depthTest || stencilTest ) )
	{
		float depthClear = 1.0f;
		UINT8 stencilClear = 0;

		D3D12_CLEAR_FLAGS clearFlags = ( clearDepth ) ? D3D12_CLEAR_FLAG_DEPTH : D3D12_CLEAR_FLAG_STENCIL;
		if ( clearDepth && clearStencil )
		{
			clearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
		}

		commandList->ClearDepthStencilView( 
			depthStencilHandle, clearFlags, depthClear, stencilClear, 0, nullptr );
	}

	int numRenderTargets = renderTargetHandles.size();
	
	if ( outputToBackBuffer )
	{
		numRenderTargets = 1;
	}

	commandList->OMSetRenderTargets( numRenderTargets, rtvHandlePtr, FALSE, depthStencilHandlePtr );

	// Bind vertex buffer and draw
	commandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	commandList->IASetVertexBuffers( 0, 1, &vertexBufferView );
	commandList->DrawInstanced( model->mesh->vertexCount, 1, 0, 0 );
}

bool DrawTask::BindRenderTargets( std::vector<Gfx::Texture*> renderTargets, Gfx::Texture* depthStencilTarget, Gfx::Resources* resources )
{
	if ( renderTargets.empty() )
	{
		// Need at least one render target!
		return false;
	}

	this->renderTargets = renderTargets;

	for ( Gfx::Texture* rtvTexture : renderTargets )
	{
		renderTargetHandles.push_back( resources->GetCpuHandle( rtvTexture->rtv, Gfx::RTV_HEAP ) );
	}
	
	if ( depthStencilTarget != nullptr )
	{
		this->depthStencilTarget = depthStencilTarget;
		this->depthStencilHandle = resources->GetCpuHandle( depthStencilTarget->dsv, Gfx::DSV_HEAP );
	}

	return true;
}

void BarrierTask::execute(
	ComPtr<ID3D12GraphicsCommandList> commandList, 
	ComPtr<ID3D12CommandAllocator> commandAllocator,
	Gfx::Resources *resources
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
	Gfx::Resources *resources
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
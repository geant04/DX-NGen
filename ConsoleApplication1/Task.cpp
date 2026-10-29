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

	if ( renderTargetHandles.empty() && !depthStencilTarget )
    {
		std::cout << "ERROR: empty render and depth targets!" << std::endl;
		return;
	}

	// Set viewport and scissor rect
	const unsigned int numRenderTargetHandles = renderTargetHandles.size();
	const unsigned int numRenderTargetTextures = renderTargets.size();
	
	if ( numRenderTargetHandles != numRenderTargetTextures )
	{
		std::cout << "ERROR: mismatch numRenderTargetHandles and numRenderTargetTexture!";
		return;
	}

	std::vector<D3D12_VIEWPORT> viewports( numRenderTargetTextures );
	std::vector<D3D12_RECT> scissorRects( numRenderTargetTextures );

	for ( unsigned int i = 0; i < numRenderTargetTextures; i++ )
	{
		Gfx::RenderTarget* renderTarget = renderTargets[i];

		viewports[i] = { 0.0f, 0.0f, static_cast<float>( renderTarget->viewportWidth ), static_cast<float>( renderTarget->viewportHeight ), 0.0f, 1.0f };
		scissorRects[i] = { 0, 0, renderTarget->scissorWidth, renderTarget->scissorHeight };
	}

	if ( numRenderTargetTextures == 0 )
	{
		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>( viewportWidth ), static_cast<float>( viewportHeight ), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>( viewportWidth ), static_cast<LONG>( viewportHeight ) };

		commandList->RSSetViewports( 1, &viewport );
		commandList->RSSetScissorRects( 1, &scissorRect );
	}
	else
	{
		commandList->RSSetViewports( numRenderTargetTextures, &viewports[0] );
		commandList->RSSetScissorRects( numRenderTargetTextures, &scissorRects[0] );
	}

	D3D12_CPU_DESCRIPTOR_HANDLE swapChainHandle = resources->GetCpuHandle( resources->swapChainOutputTexture->rtv, Gfx::RTV_HEAP );
	D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandlePtr = ( numRenderTargetTextures > 0 ) ? &renderTargetHandles[0] : nullptr;
	rtvHandlePtr = outputToBackBuffer ? &swapChainHandle : rtvHandlePtr;
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

bool DrawTask::BindRenderTargets( std::vector<Gfx::RenderTarget*> renderTargets, Gfx::Texture* depthStencilTarget, Gfx::Resources* resources )
{
	if ( renderTargets.empty() && !depthStencilTarget )
	{
		// Need at least one render/depth target!
		std::cout << "ERROR: no render target or depth target!" << std::endl;
		return false;
	}

	this->renderTargets = renderTargets;

	for ( Gfx::RenderTarget* renderTarget : renderTargets )
	{
		if ( !renderTarget )
		{
			std::cout << "ERROR: encountered a null renderTarrget in BindRenderTargets!";
			return false;
		}

		renderTargetHandles.push_back( resources->GetCpuHandle( renderTarget->rtvTexture->rtv, Gfx::RTV_HEAP ) );
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
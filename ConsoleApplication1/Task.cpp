#include "Task.h"

void DrawTask::execute(
	ComPtr<ID3D12GraphicsCommandList> &commandList, 
	ComPtr<ID3D12CommandAllocator> &commandAllocator,
    D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
)
{
	// Bind the root signature and PSO before binding any descriptor tables against them
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetPipelineState(pipelineState.Get());

    if (descriptorHeap)
    {
        ID3D12DescriptorHeap* heaps[] = { descriptorHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        commandList->SetGraphicsRootDescriptorTable(0, srvTableHandle);
    }

    // Bind RTV
    commandList->OMSetRenderTargets(1, rtvHandle, FALSE, nullptr);

    // Bind vertex buffer and draw
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);
}

void ComputeTask::execute(
	ComPtr<ID3D12GraphicsCommandList> &commandList, 
	ComPtr<ID3D12CommandAllocator> &commandAllocator,
    D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
)
{
    // Memcpy update info from CPU to GPU?
    memcpy(mCameraConstantBufferData, &mCameraConstants, sizeof(mCameraConstants));

    // PSO and root signature setup
    ID3D12DescriptorHeap* heaps[] = { descriptorHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    commandList->SetComputeRootSignature(rootSignature.Get());
    commandList->SetPipelineState(pipelineState.Get());
    commandList->SetComputeRootDescriptorTable(0, mOutputUAVHandle);
    commandList->SetComputeRootConstantBufferView(1, mCameraConstantBuffer->GetGPUVirtualAddress());

    // Barrier transition from PS to UA
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		mOutputTexture.Get(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
    commandList->ResourceBarrier(1, &barrier);

    commandList->Dispatch(dispatchInfo.width, dispatchInfo.height, 1);

    // Barrier transition back from UA to PS
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		mOutputTexture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
    commandList->ResourceBarrier(1, &barrier);
}
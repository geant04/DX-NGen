#pragma once

#include "Common.h"
#include "Mesh.h"

class Task
{
public:
	
	struct InitResources
	{
		const char* vertexShaderName;
		const char* pixelShaderName;
	};

	virtual ~Task() = default;

	virtual void execute(
		ComPtr<ID3D12GraphicsCommandList> &commandList, 
		ComPtr<ID3D12CommandAllocator> &commandAllocator,
		D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
	) = 0;

	// Shader Resources
	ComPtr<ID3D12RootSignature> rootSignature;

	// Pipelines
	ComPtr<ID3D12PipelineState> pipelineState;

	InitResources initResources;
};

class DrawTask : public Task
{

public:
	// GPU RESOURCES
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> vertexBufferUpload;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap = {};
	D3D12_GPU_DESCRIPTOR_HANDLE srvTableHandle = {};
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

	// CPU RESOURCES
	Mesh* mesh;

	void execute(
		ComPtr<ID3D12GraphicsCommandList> &commandList, 
		ComPtr<ID3D12CommandAllocator> &commandAllocator,
		D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
	) override;
};

// Camera buffer information - this should be separately stored outside of compute task...
struct CameraConstants {
	XMFLOAT4X4 viewProjInverse;
	XMFLOAT4 cameraPosition;
	uint32_t frameNumber;
	uint32_t padding0[3];
};

class ComputeTask : public Task
{
struct DispatchInfo
{
	uint32_t width;
	uint32_t height;
};

public:
	// These resources shouldn't exist for all compute tasks, but alas.
	ComPtr<ID3D12Resource> mOutputTexture;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_GPU_DESCRIPTOR_HANDLE mOutputUAVHandle = {};

	// Camera buffer information - this should be separately stored outside of compute task...
	ComPtr<ID3D12Resource> mCameraConstantBuffer;
	UINT8* mCameraConstantBufferData;
	CameraConstants mCameraConstants;

	DispatchInfo dispatchInfo;

	void execute(
		ComPtr<ID3D12GraphicsCommandList> &commandList, 
		ComPtr<ID3D12CommandAllocator> &commandAllocator,
		D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle = nullptr
	) override;
};


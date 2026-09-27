#pragma once

#include "Common.h"
#include "Mesh.h"
#include "ResourceManager.h"

struct PreparedDescriptorTable
{
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
	uint32_t descriptorCount = 0;
};

struct PreparedBindings
{
	PreparedDescriptorTable readTextures;
	PreparedDescriptorTable readWriteTextures;
	PreparedDescriptorTable buffers;
	PreparedDescriptorTable dynamicBuffers;
};

enum TaskType
{
	None = 0,
	Draw = 1,
	Compute = 2
};

class Task
{
public:
	
	struct InitResources{};

	virtual ~Task() = default;

	virtual void execute(
		ComPtr<ID3D12GraphicsCommandList> commandList, 
		ComPtr<ID3D12CommandAllocator> commandAllocator,
		D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
	) = 0;

	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipelineState;

	PreparedDescriptorTable descriptorTable;
	PreparedBindings bindings;
	Gfx::DescriptorHeap* descriptorHeap = nullptr;

	std::vector<Gfx::Texture*> readTextures;
	std::vector<Gfx::Texture*> readWriteTextures;
	std::vector<Gfx::Buffer*> buffers;
	std::vector<Gfx::Buffer*> dynamicBuffers;

	std::vector<Gfx::Sampler*> staticSamplers;

	virtual TaskType getType() const { return None; }
};

class DrawTask : public Task
{
struct InitResources
{
	const char* vertexShaderName;
	const char* pixelShaderName;
};

public:
	// GPU RESOURCES
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> vertexBufferUpload;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

	// CPU RESOURCES
	Mesh* mesh;

	void execute(
		ComPtr<ID3D12GraphicsCommandList> commandList, 
		ComPtr<ID3D12CommandAllocator> commandAllocator,
		D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle
	) override;

	InitResources initResources;

	virtual TaskType getType() const override { return Draw; }
};

class ComputeTask : public Task
{
struct InitResources
{
	const char* csAssetName;
	const char* entryPoint;
	uint32_t dispatchWidth;
	uint32_t dispatchHeight;
};

public:
	InitResources initResources;

	void execute(
		ComPtr<ID3D12GraphicsCommandList> commandList, 
		ComPtr<ID3D12CommandAllocator> commandAllocator,
		D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle = nullptr
	) override;

	virtual TaskType getType() const override { return Compute; }
};

class BarrierTask : public Task
{
public:
	ID3D12Resource* resource = nullptr;
    D3D12_RESOURCE_STATES stateBefore = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_COMMON;

	void execute(
		ComPtr<ID3D12GraphicsCommandList> commandList, 
		ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr,
		D3D12_CPU_DESCRIPTOR_HANDLE *rtvHandle = nullptr
	) override;
};
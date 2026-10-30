#pragma once

#include "Common.h"
#include "Mesh.h"
#include "Camera.h"
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
		Gfx::Resources *resources
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
	// We're going to move this vertexBuffer stuff somewhere else so that we can process geometry draws a lot more easily
	// GPU RESOURCES
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> vertexBufferUpload;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

	// GPU/CPU RESOURCES
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetHandles = {};
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle;

	std::vector<Gfx::RenderTarget*> renderTargets;

	Gfx::Texture* depthStencilTarget;

	// I don't really like this!!
	CameraConstants drawConstants;

	// CPU RESOURCES
	int viewportWidth;
	int viewportHeight;

	Model* model;

	bool clearRenderTarget = false;
	bool clearDepth = false;
	bool clearStencil = false;
	bool depthTest = false;
	bool stencilTest = false;

	// special hack case because I want the thing to work
	bool outputToBackBuffer = false;

	bool BindRenderTargets( std::vector<Gfx::RenderTarget*> renderTargets, Gfx::Texture* depthStencilTarget, Gfx::Resources* resources );

	void execute(
		ComPtr<ID3D12GraphicsCommandList> commandList, 
		ComPtr<ID3D12CommandAllocator> commandAllocator,
		Gfx::Resources *resources
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
		Gfx::Resources *resources
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
		Gfx::Resources *resources = nullptr
	) override;
};
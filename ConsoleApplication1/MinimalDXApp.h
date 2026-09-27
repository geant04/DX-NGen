#pragma once

#include "Common.h"
#include "Task.h"
#include "Settings.h"
#include "ResourceManager.h"

struct WindowProperties
{
	HWND g_hWnd;
	int mWidth;
	int mHeight;
	float aspectRatio;
};

struct MinimalDXApp
{
	// Baseline boiler plate information storing everything needed for a simple DX12 app.
	// This includes the following:
	//  - DXGI Factory + Adapter + Device setup to allow our program to communicate to the GPU + locate GPU to work on
	//  - Command Queue + Command List to execute commands, in this case we only care about a command queue to draw
	//  - Command Allocator, ?? not sure what this does yet, but it's important
	//  - Swap Chain, this stores our framebuffers that we will query and draw to per frame?
	//  - Descriptor Heaps + RTVs, this stores a table on the GPU to access Render Target Views/Views, allowing us to see GPU memory
	//  - PSO... stores vertex/pixel shader information, providing proper layouts for the respective shaders
	//  - Fences/Synchronization to ensure we can execute commands on the GPU and run CPU code subsequently
	void InitializeDX( const GlobalSettings& settings );
	void BuildTaskList();
	void WaitForPreviousFrame();
	void CreateDisplayWindow(HINSTANCE hInstnace, int nCmdShow);

	void ProcessGameState();
	void Render();
	void Run();
	void AddTask(std::unique_ptr<Task> task);

	// SOON TO BE DEPRECATED FRAMECOUNT NUMBER!
	static const uint32_t frameCount = 2;
	WindowProperties windowProperties = {};

	// Backend context should be abstracted away in a separate level
	ComPtr<IDXGIFactory7> factory;
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<ID3D12Device> device;

	// Buffer writing stuff
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<IDXGISwapChain4> swapChain;

	// Resources
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> shaderResourceHeap;
	ComPtr<ID3D12Resource> renderTargets[frameCount];
	ComPtr<ID3D12Resource> screenTextureUAV;

	Gfx::Resources resources;

	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 fenceValue = 0;

	// Miscellanious things
	std::vector<std::unique_ptr<Task>> tasks;

	uint32_t frameIndex;
	uint32_t rtvDescriptorSize;
	uint32_t shaderResourceDescriptorSize;
	uint32_t* frameNumber;

	// PERSISTENT RESOURCES
	CameraConstants cameraConstants = {};
	Gfx::Texture screenTexture;
	Gfx::Sampler bilinearSamplerClamp;
	Gfx::Buffer cameraDataBuffer;
};


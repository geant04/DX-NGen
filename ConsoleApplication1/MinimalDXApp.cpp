#include "MinimalDXApp.h"
#include "Pipeline.h"
#include "RootSignature.h"
#include "Mesh.h"
#include "RenderBackend.h"
#include "Settings.h"
#include <iostream>

#define SIMPLE_TEST false

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_DESTROY: PostQuitMessage(0); return 0;
	default: return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

void MinimalDXApp::CreateDisplayWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Populate the WindowProperties struct with stuff copied from GPT.
	// Not gonna bother figuring out how the window is made. Not the priority here.
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"DX12WindowClass";
	RegisterClassEx(&wc);

	windowProperties.mHeight = 720;
	windowProperties.mWidth = 1280;
	windowProperties.aspectRatio = static_cast<float>(windowProperties.mWidth) / static_cast<float>(windowProperties.mHeight);

	RECT rect = {0, 0, (LONG)windowProperties.mWidth, (LONG)windowProperties.mHeight};
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

	windowProperties.g_hWnd = CreateWindow(
		wc.lpszClassName,
		L"DirectX 12 Triangle",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr, nullptr, hInstance, nullptr
	);

	ShowWindow(windowProperties.g_hWnd, nCmdShow);
}


/*
Very straightforward program, goes over the esssential basics to making sure our DX12 program can run smoothly.
The code works in a very sequential order, initializing everything important we need.
I think there should be a destructor of some sort implemented later on, but we'll worry about it later.
*/
void MinimalDXApp::InitializeDX( const GlobalSettings& settings )
{
	UINT factoryFlags = 0;

#if defined(_DEBUG)
	// Turn on the D3D12 debug layer so resource-state, descriptor, and PSO mismatches
	// show up as messages in the Output window instead of silently producing garbage/white frames.
	{
		ComPtr<ID3D12Debug1> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(TRUE);
		}

		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	// Begin with the DXGI Factory.
	// DXGI is the bridge between GPU/driver and the code
	// Not sure what IID_PPV_ARGS does, but returns interface pointer... whatever that means
	CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory));

	// I do need to check that this isn't my integrated graphics, I might have multiple adapters
	factory->EnumAdapters1(0, &adapter);
	
	// Create the device
	D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

	{
		// Create Command Queue, requires a description. This is pretty similar to Vulkan!
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};


		// D3D12 offers several different command list types:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles
		// - Direct: Capable of handling everything apaprently 3D (graphics, compute, copy)
		// - Compute: I assume this handles compute shaders, similar to a compute queue.
		// - Bundle: ? Not sure - consists of multiple API calls... 
		// - Copy: ? Not sure, just repsonsible for copying memory between resources on GPU
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	}

	// Create swap chain
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = frameCount;
		swapChainDesc.Width = windowProperties.mWidth;
		swapChainDesc.Height = windowProperties.mHeight;
		swapChainDesc.Format = settings.isHDR ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;

		// The frames in this swap chain have render target writing capability
		// Retrieved buffers are called back-buffers, written to and then later drawn to the screen
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		// This has to do with multisampling I think?
		swapChainDesc.SampleDesc.Count = 1;
	
		ComPtr<IDXGISwapChain1> swapChainPtr;
		factory->CreateSwapChainForHwnd(
			commandQueue.Get(), 
			windowProperties.g_hWnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChainPtr
		);

		swapChainPtr.As(&swapChain);
	}

	frameIndex = swapChain->GetCurrentBackBufferIndex();

	ResourceManager::CreateGlobalDescriptorHeaps( device, globalDescriptorHeaps );

	// todo: replace all of this descriptor junk with the global data
	// Create RTV Descriptor Heap + Texture Heap for our awesome texture
	{
		// I'm afraid this gets a bit complicated when we want to do stuff like
		// add textures, requiring SRVs and UAVs
		// SRV - Shader Resource View
		// UAV - Unordered Resource View
		// RTV - Render Target View
		D3D12_DESCRIPTOR_HEAP_DESC RTVDescriptorHeapDesc = {};
		RTVDescriptorHeapDesc.NumDescriptors = frameCount;
		RTVDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		RTVDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		device->CreateDescriptorHeap(&RTVDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
		
		rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// So yes, I was correct that this part gets complicated when we add SRV heaps. My question, though, is that if we 
		// want to bind multiple textures, does it just require one heap, or many?
		D3D12_DESCRIPTOR_HEAP_DESC shaderResourceHeapDesc = {};

		// In theory, if I wanted multiple textures, then I'd adjust the number of descriptors.
		// This is overly simplistic, though! It does not scale at all.
		// To scale well, we'd instead need a bigger descriptor heap with, say, like 10000 descriptors.
		// I think this is pretty similar to the bind group in terms of defining visibility as well.

		// Descriptor is a metadata record. Stored in the descriptor heap
		// This should be called the textureHeap or something in the future, should I add more textures to it
		// Updating SRVHeapDesc to contain 3 now for our simple PathTracing app.
		// Slot 0: UAV, screenTexture
		// Slot 1: SRV, for the scene geo and camera info
		// Slot 2: SRV, view for the screenTexture
		shaderResourceHeapDesc.NumDescriptors = 3;
		shaderResourceHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		shaderResourceHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		device->CreateDescriptorHeap(&shaderResourceHeapDesc, IID_PPV_ARGS(&shaderResourceHeap));
		shaderResourceDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	}

	// Create frame resources in the swap chain, can we think of this as populating frame data per backbuffer in the swapchain?
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		for (uint32_t i = 0; i < frameCount; i++)
		{
			swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
			device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize;
		}
	}

	// Might need a reminder of commandAllocator's purpose?
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));

	// Create synchronization stage
	{
		device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		fenceValue = 1;
		fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			// TODO: Need some error handling/validation
		}
	}
}


// Responsible for PSO creation and compiling vert/pixel shaders
void MinimalDXApp::BuildTaskList()
{
	std::unique_ptr<DrawTask> drawScreen = std::make_unique<DrawTask>();
	{
		drawScreen->mesh = &Mesh::GenerateFullScreenTriangle();
		drawScreen->initResources.vertexShaderName = "vsTriangle.hlsl";
		drawScreen->initResources.pixelShaderName = "psBlit.hlsl";
	}
	Gfx::CreateDrawTask( this, drawScreen.get() );
	
	// ? why is this here and not at the top of BuildTaskList?
	commandList->Close();

	{
		// TODO:
		// My goal is to do some fun compute shader dispatch work that modifies our "screen", having a thread modify a UAV.
		// This is our ad-hoc path-tracer, which simulates path-tracing on a per-thread level.
		// We will then do some copy-to-RTV operation, such that we draw to the swap-chain using the results
		// from our compute pass.
		// Overall, this requires:
		// 1. Path-tracing target UAV
		// 2. Transition from UAV to SRV after the compute work is done (requires a barrier)
		// 3. Root signature using our SRV, and final draw to RT using our transitioned SRV.

		std::unique_ptr<ComputeTask> pathTraceTask = std::make_unique<ComputeTask>();

		RootSignature::createPathTracingRootSignature(
			device,
			pathTraceTask->rootSignature
		);

		Pipeline::createComputePSO(
			device,
			pathTraceTask->rootSignature,
			pathTraceTask->pipelineState,
			"csPathTracer.hlsl",
			"CSMain"
		);
		
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.MipLevels = 1;
		textureDesc.Width = windowProperties.mWidth;
		textureDesc.Height = windowProperties.mHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		// Resource state initial at PS_Resource, s.t we go PS -> UA -> PS per frame
		CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&screenTexture)
		);

		screenTexture->SetName(L"Screen Texture");

		CameraConstants cameraConstants = {};
		pathTraceTask->mCameraConstants = cameraConstants;

		// Camera constant buffer: upload heap, persistently mapped so we can memcpy fresh data each frame
		const UINT cameraCBSize = (sizeof(CameraConstants) + 255) & ~255;
		CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC cameraCBDesc = CD3DX12_RESOURCE_DESC::Buffer(cameraCBSize);
		device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&cameraCBDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pathTraceTask->mCameraConstantBuffer)
		);

		pathTraceTask->mCameraConstantBuffer->SetName(L"Camera Constant Buffer");

		D3D12_RANGE cameraCBReadRange = { 0, 0 };
		pathTraceTask->mCameraConstantBuffer->Map(0, &cameraCBReadRange, reinterpret_cast<void**>(&pathTraceTask->mCameraConstantBufferData));
		memcpy(pathTraceTask->mCameraConstantBufferData, &pathTraceTask->mCameraConstants, sizeof(CameraConstants));

		// Walk the shader-visible heap, building a view at each reserved slot:
		// Slot 0: UAV of screenTexture (compute output, u0)
		// Slot 1: reserved for scene-geo / future texture SRV (t0 in the path tracing root signature)
		// Slot 2: SRV of screenTexture (sampled by drawScreen's pixel shader, t0)
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = shaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = shaderResourceHeap->GetGPUDescriptorHandleForHeapStart();

		// Populate slot 0
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		device->CreateUnorderedAccessView(screenTexture.Get(), nullptr, &uavDesc, cpuHandle);
		pathTraceTask->mOutputUAVHandle = gpuHandle;

		cpuHandle.ptr += shaderResourceDescriptorSize;
		gpuHandle.ptr += shaderResourceDescriptorSize;
		// Slot 1 stays unpopulated until there's actual scene/texture data to view

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = pathTraceTask->mCameraConstantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = cameraCBSize;
		device->CreateConstantBufferView(&cbvDesc, cpuHandle);

		cpuHandle.ptr += shaderResourceDescriptorSize;
		gpuHandle.ptr += shaderResourceDescriptorSize;
		// Populate slot 2

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(screenTexture.Get(), &srvDesc, cpuHandle);

		drawScreen->descriptorHeap = shaderResourceHeap;
		drawScreen->srvTableHandle = gpuHandle;

		pathTraceTask->mOutputTexture = screenTexture;
		pathTraceTask->descriptorHeap = shaderResourceHeap;
		pathTraceTask->dispatchInfo.width = (windowProperties.mWidth + 7) / 8; // hardcoded (8, 8, 1) dims
		pathTraceTask->dispatchInfo.height = (windowProperties.mHeight + 7) / 8;

		frameNumber = &pathTraceTask->mCameraConstants.frameNumber;

		tasks.push_back(std::move(pathTraceTask));
	}

	// Sync
	{
		WaitForPreviousFrame();
	}

	tasks.push_back(std::move(drawScreen));
}

void MinimalDXApp::addTask(std::unique_ptr<Task> task)
{
	tasks.push_back(std::move(task));
}

void MinimalDXApp::Render()
{
    // Get the current back buffer handle
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += frameIndex * rtvDescriptorSize;

	// Reset command allocator & list
	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);

    // Set viewport and scissor rect
    D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(windowProperties.mWidth), static_cast<float>(windowProperties.mHeight), 0.0f, 1.0f};
    D3D12_RECT scissorRect = {0, 0, windowProperties.mWidth, windowProperties.mHeight};
	commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

	// Transition the back buffer to render target
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		renderTargets[frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);
    commandList->ResourceBarrier(1, &barrier);

    // Clear the render target
    FLOAT clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// Update frameNumber cbuffer value
	*frameNumber += 1;

	for (auto& task : tasks)
	{	
		task->execute(commandList, commandAllocator, &rtvHandle);
	}
	
    // Transition back buffer to present
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargets[frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );

    commandList->ResourceBarrier(1, &barrier);

    // Close and execute
    commandList->Close();

	// ACTUAL rendering part
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present
    swapChain->Present(1, 0);

    // Wait for GPU to finish
    WaitForPreviousFrame();
}

void MinimalDXApp::Run()
{
	// When render is done, we call it like a helper function.
	MSG msg = {};

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}
}

void MinimalDXApp::WaitForPreviousFrame()
{
	const UINT64 fenceCheck = fenceValue;
	commandQueue->Signal(fence.Get(), fenceCheck);
	fenceValue++;

	if ( fence->GetCompletedValue() < fenceCheck)
	{
		fence->SetEventOnCompletion(fenceCheck, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	frameIndex = swapChain->GetCurrentBackBufferIndex();
}
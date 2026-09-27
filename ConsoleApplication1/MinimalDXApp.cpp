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

	{
		Gfx::CreateGlobalDescriptorHeaps( device, resources.globalDescriptorHeap );
		Gfx::CreateSwapChainRTVs( device, swapChain, &resources, SWAPCHAIN_COUNT );
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

void MinimalDXApp::BuildTaskList()
{
	const uint32_t screenTextureMips = 1;
	const uint32_t screenTextureSamples = 1;
	const uint32_t screenWidth = windowProperties.mWidth;
	const uint32_t screenHeight = windowProperties.mHeight;
	
	std::unique_ptr<ComputeTask> pathTraceTask = std::make_unique<ComputeTask>();
	{
		pathTraceTask->readWriteTextures.push_back( &screenTexture );
		pathTraceTask->dynamicBuffers.push_back( &cameraDataBuffer );

		Gfx::CreateRWTexture( device, &screenTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, screenTextureMips );
		Gfx::CreateReadTexture( device, &screenTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, screenTextureMips );
		Gfx::CreateConstantBuffer( device, &cameraDataBuffer, &resources, &cameraConstants, (sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );

		pathTraceTask->initResources.csAssetName = "csPathTracer.hlsl";
		pathTraceTask->initResources.entryPoint = "CSMain";
		pathTraceTask->initResources.dispatchWidth = (windowProperties.mWidth + 7) / 8; // hardcoded (8, 8, 1) dims
		pathTraceTask->initResources.dispatchHeight = (windowProperties.mHeight + 7) / 8;
	}

	if ( !Gfx::CreateComputeTask( this, pathTraceTask.get() ) )
	{
		return;
	}

	std::unique_ptr<DrawTask> drawScreen = std::make_unique<DrawTask>();
	{
		drawScreen->mesh = &Mesh::GenerateFullScreenTriangle();
		drawScreen->initResources.vertexShaderName = "vsTriangle.hlsl";
		drawScreen->initResources.pixelShaderName = "psBlit.hlsl";
		drawScreen->readTextures.push_back( &screenTexture );
		drawScreen->staticSamplers.push_back( &bilinearSamplerClamp );
		{
			bilinearSamplerClamp.filterMode = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			bilinearSamplerClamp.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}
	}

	if ( !Gfx::CreateDrawTask( this, drawScreen.get() ) )
	{
		return;
	}

	// Sync
	{
		WaitForPreviousFrame();
	}

	std::unique_ptr<BarrierTask> screenSrvToUav = std::make_unique<BarrierTask>();
	{
		screenSrvToUav->resource = screenTexture.resource.Get();
		screenSrvToUav->stateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		screenSrvToUav->stateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	std::unique_ptr<BarrierTask> screenUavToSrv = std::make_unique<BarrierTask>();
	{	
		screenUavToSrv->resource = screenTexture.resource.Get();
		screenUavToSrv->stateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		screenUavToSrv->stateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	tasks.push_back( std::move( screenSrvToUav ) );
	tasks.push_back( std::move( pathTraceTask ) );
	tasks.push_back( std::move( screenUavToSrv ) );
	tasks.push_back( std::move( drawScreen ) );
}

void MinimalDXApp::AddTask( std::unique_ptr<Task> task )
{
	tasks.push_back( std::move( task ) );
}

void MinimalDXApp::ProcessGameState()
{
	// func responsible for stuff like getting mouse I/O or toggling settings and what not.
	// basically processing changes and stuff from the client

	{
		cameraConstants.frameNumber++;
	}
}

// TOOD: need to move this to render backend, there is too much backend work done here
void MinimalDXApp::Render()
{
    // Get the current back buffer handle
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = resources.GetCpuHandle( resources.swapChainTextures[frameIndex].rtv, Gfx::RTV_HEAP );

	// Reset command allocator & list
	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);

	// Bind, global descriptor heap setup
	ID3D12DescriptorHeap* descriptorHeaps[] =
	{
		resources.globalDescriptorHeap.shaderResourceGlobalHeap.descriptorHeapPtr.Get(),
		resources.globalDescriptorHeap.samplerGlobalHeap.descriptorHeapPtr.Get()
	};

	commandList->SetDescriptorHeaps( _countof(descriptorHeaps), descriptorHeaps );

    // Set viewport and scissor rect
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>( windowProperties.mWidth ), static_cast<float>( windowProperties.mHeight ), 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, windowProperties.mWidth, windowProperties.mHeight };
	commandList->RSSetViewports( 1, &viewport );
    commandList->RSSetScissorRects( 1, &scissorRect );

	// I can't quite abstract this away into tasks because this changes dynamically, though it would be
	// interesting to see if we can support dynamic tasks? probably not a good idea though
	// Transition the back buffer to render target
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		resources.swapChainTextures[frameIndex].resource.Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);
    commandList->ResourceBarrier(1, &barrier);

    // Clear the render target
    FLOAT clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	for ( auto& task : tasks )
	{	
		task->execute( commandList, commandAllocator, &rtvHandle );
	}
	
    // Transition back buffer to present
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resources.swapChainTextures[frameIndex].resource.Get(),
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
			ProcessGameState();
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
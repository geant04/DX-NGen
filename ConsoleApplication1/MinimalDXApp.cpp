#include "MinimalDXApp.h"
#include "Pipeline.h"
#include "RootSignature.h"
#include "Mesh.h"
#include "RenderBackend.h"
#include "Settings.h"
#include <iostream>
#include <windowsx.h>

#define SIMPLE_TEST false

// GLOBAL VARS
bool usePathTracingMode = false;
bool resetPathTracer = true;

bool isLeftMouseDown = false;
int lastMouseX = 0;
int lastMouseY = 0;
int deltaMouseX = 0;
int deltaMouseY = 0;

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
	case WM_LBUTTONDOWN:
		std::cout << "isMoving"  << std::endl;

		isLeftMouseDown = true;
		lastMouseX = LOWORD( lParam );
		lastMouseY = HIWORD( lParam );
		SetCapture( hWnd );
		return 0;

	case WM_LBUTTONUP:
		isLeftMouseDown = false;
		deltaMouseX = 0;
		deltaMouseY = 0;
		ReleaseCapture();
		return 0;

	case WM_MOUSEMOVE:
		if ( isLeftMouseDown )
		{
			int mouseX = LOWORD( lParam );
			int mouseY = HIWORD( lParam );
			deltaMouseX += mouseX - lastMouseX;
			deltaMouseY += mouseY - lastMouseY;
			lastMouseX = mouseX;
			lastMouseY = mouseY;
		}
		return 0;

	case WM_KEYDOWN:
		if ( wParam == 'M' && !( lParam & ( 1 << 30 ) ) )
		{
			if ( !usePathTracingMode )
			{
				resetPathTracer = true;
			}
			usePathTracingMode = !usePathTracingMode;
		}
		return 0;
	
	case WM_DESTROY:
		PostQuitMessage( 0 );
		return 0;
	
	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}
}

void MinimalDXApp::CreateDisplayWindow( HINSTANCE hInstance, int nCmdShow )
{
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
		resources.swapChainOutputTexture = &resources.swapChainTextures[frameIndex];
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

	{
		// random stuff
		resources.globalFullScreenTriangleMesh = &Mesh::GenerateFullScreenTriangle();
		resources.globalTestTriangle.mesh = resources.globalFullScreenTriangleMesh;

		resources.globalTestCubeMesh = &Mesh::GenerateCube( 1.0f );
		resources.globalTestCube.mesh = resources.globalTestCubeMesh;

		// set sun camera view proj matrix
		const XMVECTOR target = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
		const float lightDistance = 20.0f;
		const XMVECTOR eye = XMVectorSubtract( target, XMVectorScale( sunDirection, lightDistance ) );
		const XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
		const XMMATRIX view = XMMatrixLookAtLH( eye, target, up );
		const XMMATRIX projection = XMMatrixOrthographicLH( 24.0f, 24.0f, 1.0f, 50.0f );

		shadowCameraData.viewProj = view * projection;
	}
}

void MinimalDXApp::BuildTaskList()
{
	const uint32_t screenTextureMips = 1;
	const uint32_t screenTextureSamples = 1;
	const uint32_t screenWidth = windowProperties.mWidth;
	const uint32_t screenHeight = windowProperties.mHeight;

	Gfx::Texture* screenTexture = Gfx::AddTexture( &resources );
	Gfx::Texture* sceneDepthTexture = Gfx::AddTexture( &resources );
	Gfx::Buffer* cameraDataBuffer = Gfx::AddBuffer( &resources );
	Gfx::Sampler* bilinearSamplerClamp = Gfx::AddSampler( &resources );
	Gfx::RenderTarget* screenRenderTarget = Gfx::AddRenderTarget( &resources );
	
	// Model* testCubeModel = Gfx::AddModel( &resources );
	//testCubeModel = &resources.globalTestCube;
	resources.globalTestCube.mesh = resources.globalTestCubeMesh;

	Model* testFloorModel = Gfx::AddModel( &resources );
	testFloorModel->mesh = resources.globalTestCube.mesh;

	Model* fullScreenTriangle = Gfx::AddModel( &resources );
	fullScreenTriangle = &resources.globalTestTriangle;

	Gfx::CreateRWTexture( device, screenTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, screenTextureMips );
	Gfx::CreateReadTexture( device, screenTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, screenTextureMips );
	Gfx::CreateConstantBuffer( device, cameraDataBuffer, &resources, &cameraConstants, (sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
	Gfx::CreateRTV( device, screenTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_R8G8B8A8_UNORM, screenTextureMips );

	// Shadow draw tasks... Need to refactor this in such a way that:
	// 1. let every object have a shadowCaster flag or something
	// 2. process all geometry separately and automatically generate draw tasks from there
	// 3. if above works, then the same logic maps for shadows
	// This is a temporary hack because I want to say "I added shadow maps!" to my engine.
	const unsigned int shadowMapSize = 512;
	Gfx::Texture* sunShadowMap = Gfx::AddTexture( &resources );
	Gfx::CreateDepthTexture( device, sunShadowMap, &resources, shadowMapSize, shadowMapSize, DXGI_FORMAT_D16_UNORM, 1 );
	Gfx::CreateReadTexture( device, sunShadowMap, &resources, shadowMapSize, shadowMapSize, DXGI_FORMAT_R16_UNORM, 1 );

	std::shared_ptr<ComputeTask> pathTraceTask = std::make_shared<ComputeTask>();
	{
		pathTraceTask->readWriteTextures.push_back( screenTexture );
		pathTraceTask->dynamicBuffers.push_back( cameraDataBuffer );

		pathTraceTask->initResources.csAssetName = "csPathTracer.hlsl";
		pathTraceTask->initResources.entryPoint = "CSMain";
		pathTraceTask->initResources.dispatchWidth = (windowProperties.mWidth + 7) / 8; // hardcoded (8, 8, 1) dims
		pathTraceTask->initResources.dispatchHeight = (windowProperties.mHeight + 7) / 8;
	}

	if ( !Gfx::CreateComputeTask( this, pathTraceTask.get() ) )
	{
		return;
	}

	std::shared_ptr<DrawTask> drawScreen = std::make_shared<DrawTask>();
	{
		drawScreen->model = fullScreenTriangle;
		drawScreen->initResources.vertexShaderName = "vsTriangle.hlsl";
		drawScreen->initResources.pixelShaderName = "psBlit.hlsl";
		drawScreen->readTextures.push_back( screenTexture );
		drawScreen->staticSamplers.push_back( bilinearSamplerClamp );
		{
			bilinearSamplerClamp->filterMode = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			bilinearSamplerClamp->addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}
		drawScreen->clearRenderTarget = false;
		drawScreen->depthTest = false;
		drawScreen->outputToBackBuffer = true;
		drawScreen->dynamicBuffers.push_back( cameraDataBuffer );

		screenRenderTarget->rtvTexture = screenTexture;
		screenRenderTarget->viewportWidth = screenWidth;
		screenRenderTarget->viewportHeight = screenHeight;
		screenRenderTarget->scissorWidth = screenWidth;
		screenRenderTarget->scissorHeight = screenHeight;

		drawScreen->BindRenderTargets( { screenRenderTarget }, nullptr, &resources );
	}

	if ( !Gfx::CreateDrawTask( this, drawScreen.get() ) )
	{
		return;
	}

	// Sync
	{
		// These wait for previous frame calls don't really matter in-practice.
		// This is because we aren't dynamically changing resources. This call isn't ran every frame.
		WaitForPreviousFrame();
	}

	std::shared_ptr<BarrierTask> barrierScreenSrvToUav = Gfx::CreateBarrierTask( 
		screenTexture, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS );

	std::shared_ptr<BarrierTask> barrierScreenSrvToRtv = std::make_shared<BarrierTask>();
	{
		barrierScreenSrvToRtv->resource = screenTexture->resource.Get();
		barrierScreenSrvToRtv->stateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrierScreenSrvToRtv->stateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	std::shared_ptr<BarrierTask> barrierScreenRtvToSrv = std::make_shared<BarrierTask>();
	{
		barrierScreenRtvToSrv->resource = screenTexture->resource.Get();
		barrierScreenRtvToSrv->stateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrierScreenRtvToSrv->stateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	std::shared_ptr<BarrierTask> barrierScreenUavToSrv = std::make_shared<BarrierTask>();
	{	
		barrierScreenUavToSrv->resource = screenTexture->resource.Get();
		barrierScreenUavToSrv->stateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrierScreenUavToSrv->stateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	// Temp disabling these tasks and only pushing drawScreen, which now does "draw cube" actually
	pathTracingTasks.push_back( barrierScreenSrvToUav );
	pathTracingTasks.push_back( pathTraceTask );
	pathTracingTasks.push_back( barrierScreenUavToSrv );
	pathTracingTasks.push_back( drawScreen );

	std::shared_ptr<DrawTask> drawSky = std::make_shared<DrawTask>();
	{
		drawSky->model = fullScreenTriangle;
		drawSky->dynamicBuffers.push_back( cameraDataBuffer );
		drawSky->initResources.vertexShaderName = "vsTriangle.hlsl";
		drawSky->initResources.pixelShaderName = "sky.hlsl";

		drawSky->clearRenderTarget = false;
		drawSky->depthTest = false;
		drawSky->outputToBackBuffer = false;
		drawSky->BindRenderTargets( { screenRenderTarget }, nullptr, &resources );
	}

	if ( !Gfx::CreateDrawTask( this, drawSky.get() ) )
	{
		return;
	}

	std::shared_ptr<DrawTask> drawTestCube = std::make_shared<DrawTask>();
	{
		Gfx::CreateDepthTexture( device, sceneDepthTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_D16_UNORM, 1 );
		
		drawTestCube->model = &resources.globalTestCube;
		drawTestCube->model->transform.Translate( { 0.0f, 0.0f, 0.0f } );
		{
			Gfx::Buffer* cubeConstantBuffer = Gfx::AddBuffer( &resources );
			Gfx::CreateConstantBuffer( device, cubeConstantBuffer, &resources, &drawTestCube->drawConstants, 
				(sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
			drawTestCube->dynamicBuffers.push_back( cubeConstantBuffer );
		}
		drawTestCube->readTextures.push_back( sunShadowMap );
		drawTestCube->staticSamplers.push_back( bilinearSamplerClamp );
		drawTestCube->initResources.vertexShaderName = "vsGeometry.hlsl";
		drawTestCube->initResources.pixelShaderName = "psLitGeometry.hlsl";
		drawTestCube->clearRenderTarget = false;
		drawTestCube->clearDepth = true;
		drawTestCube->depthTest = true;
		drawTestCube->outputToBackBuffer = false;
		drawTestCube->BindRenderTargets( { screenRenderTarget }, sceneDepthTexture, &resources );
	}
	
	if ( !Gfx::CreateDrawTask( this, drawTestCube.get() ) )
	{
		return;
	}

	std::shared_ptr<DrawTask> drawTestFloor = std::make_shared<DrawTask>();
	{
		drawTestFloor->model = testFloorModel;
		drawTestFloor->model->transform.Scale( { 10.0f, 0.5f, 10.0f } );
		drawTestFloor->model->transform.Translate( { 0.0f, -0.75f, 0.0f } );
		{
			Gfx::Buffer* floorConstantBuffer = Gfx::AddBuffer( &resources );
			Gfx::CreateConstantBuffer( device, floorConstantBuffer, &resources, &drawTestFloor->drawConstants, 
				(sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
			drawTestFloor->dynamicBuffers.push_back( floorConstantBuffer );
		}
		drawTestFloor->readTextures.push_back( sunShadowMap );
		drawTestFloor->staticSamplers.push_back( bilinearSamplerClamp );
		drawTestFloor->initResources.vertexShaderName = "vsGeometry.hlsl";
		drawTestFloor->initResources.pixelShaderName = "psLitGeometry.hlsl";
		drawTestFloor->clearRenderTarget = false;
		drawTestFloor->clearDepth = false;
		drawTestFloor->depthTest = true;
		drawTestFloor->outputToBackBuffer = false;
		drawTestFloor->BindRenderTargets( { screenRenderTarget }, sceneDepthTexture, &resources );
	}
	
	if ( !Gfx::CreateDrawTask( this, drawTestFloor.get() ) )
	{
		return;
	}

	std::shared_ptr<DrawTask> drawCubeShadow = std::make_shared<DrawTask>();
	std::shared_ptr<DrawTask> drawFloorShadow = std::make_shared<DrawTask>();
	{
		{
			Gfx::Buffer* shadowCubeConstantBuffer = Gfx::AddBuffer( &resources );
			Gfx::CreateConstantBuffer( device, shadowCubeConstantBuffer, &resources, &drawCubeShadow->drawConstants, 
				(sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
			drawCubeShadow->dynamicBuffers.push_back( shadowCubeConstantBuffer );

			Gfx::Buffer* shadowFloorConstantBuffer = Gfx::AddBuffer( &resources );
			Gfx::CreateConstantBuffer( device, shadowFloorConstantBuffer, &resources, &drawFloorShadow->drawConstants, 
				(sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
			drawFloorShadow->dynamicBuffers.push_back( shadowFloorConstantBuffer );
		}

		drawCubeShadow->model = drawTestCube->model;
		drawFloorShadow->model = drawTestFloor->model;
		
		drawCubeShadow->initResources.vertexShaderName = "vsGeometry.hlsl";
		drawCubeShadow->initResources.pixelShaderName = nullptr;
		drawCubeShadow->clearDepth = true;
		drawCubeShadow->depthTest = true;
		drawCubeShadow->viewportWidth = shadowMapSize;
		drawCubeShadow->viewportHeight = shadowMapSize;
		drawCubeShadow->BindRenderTargets( {}, sunShadowMap, &resources );
		
		drawFloorShadow->initResources.vertexShaderName = "vsGeometry.hlsl";
		drawFloorShadow->initResources.pixelShaderName = nullptr;
		drawFloorShadow->clearDepth = false;
		drawFloorShadow->depthTest = true;
		drawFloorShadow->viewportWidth = shadowMapSize;
		drawFloorShadow->viewportHeight = shadowMapSize;
		drawFloorShadow->BindRenderTargets( {}, sunShadowMap, &resources );
	}

	if ( !Gfx::CreateDrawTask( this, drawCubeShadow.get() ) )
	{
		return;
	}
	if ( !Gfx::CreateDrawTask( this, drawFloorShadow.get() ) )
	{
		return;
	}

	shadowTasks.push_back( drawCubeShadow );
	shadowTasks.push_back( drawFloorShadow );

	Gfx::RenderTarget* debugRenderTarget = Gfx::AddRenderTarget( &resources );
	// Gfx::Texture* scratchBuffer = Gfx::AddTexture( &resources );
	std::shared_ptr<DrawTask> drawDebugView = std::make_shared<DrawTask>();
	{
		// Gfx::CreateReadTexture( device, scratchBuffer, &resources, screenWidth, screenHeight, DXGI_FORMAT_R8_UNORM, 1 );
		drawDebugView->model = fullScreenTriangle;
		drawDebugView->initResources.vertexShaderName = "vsTriangle.hlsl";
		drawDebugView->initResources.pixelShaderName = "psDebug.hlsl";

		drawDebugView->clearRenderTarget = false;
		drawDebugView->clearDepth = false;
		drawDebugView->depthTest = false;
		drawDebugView->outputToBackBuffer = true;

		Gfx::CreateReadTexture( device, sunShadowMap, &resources, screenWidth, screenHeight, DXGI_FORMAT_R16_UNORM, 1 );
		drawDebugView->readTextures.push_back( sunShadowMap ); // this should not work because I didn't make a dedicated SRV for this? we'll find out
		drawDebugView->staticSamplers.push_back( bilinearSamplerClamp );

		debugRenderTarget->rtvTexture = screenTexture;
		debugRenderTarget->viewportWidth = static_cast<unsigned int>( 256 );
		debugRenderTarget->viewportHeight = static_cast<unsigned int>( 256 );
		debugRenderTarget->scissorWidth = static_cast<unsigned int>( debugRenderTarget->viewportWidth );
		debugRenderTarget->scissorHeight = static_cast<unsigned int>( debugRenderTarget->viewportHeight );

		drawDebugView->BindRenderTargets( { debugRenderTarget }, nullptr, &resources );
	}
		
	if ( !Gfx::CreateDrawTask( this, drawDebugView.get() ) )
	{
		return;
	}

	
	std::shared_ptr<BarrierTask> barrierDepthToSrv = Gfx::CreateBarrierTask( 
		sunShadowMap, 
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
	std::shared_ptr<BarrierTask> barrierSrvToDepth = Gfx::CreateBarrierTask( 
		sunShadowMap, 
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE );

	tasks.push_back( barrierDepthToSrv );
	tasks.push_back( barrierScreenSrvToRtv );
	tasks.push_back( drawSky );
	tasks.push_back( drawTestCube );
	tasks.push_back( drawTestFloor );
	tasks.push_back( barrierScreenRtvToSrv );
	tasks.push_back( drawScreen );
	{
		tasks.push_back( drawDebugView );
		tasks.push_back( barrierSrvToDepth );
	}
}

void MinimalDXApp::ProcessGameState()
{
	// func responsible for stuff like getting mouse I/O or toggling settings and what not.
	// basically processing changes and stuff from the client

	{
		// need to implement/generate camera matrix stuff here
		// we are rebuilding the camera every frame! gosh this is horrible...
		float fovY = cameraData.fov;
		float aspectRatio = static_cast<float>( windowProperties.mWidth ) / static_cast<float>( windowProperties.mHeight );
		float nearZ = cameraData.nearZ;
		float farZ = cameraData.farZ;
		
		// Camera update code
		{
			if ( deltaMouseX != 0 || deltaMouseY != 0 )
			{
				cameraData.yaw	 -= static_cast<float>( deltaMouseX ) * cameraData.sensitivity;
				cameraData.pitch += static_cast<float>( deltaMouseY ) * cameraData.sensitivity;

				deltaMouseX = 0;
				deltaMouseY = 0;
			}

			const float pitchLimit = XM_PIDIV2 - 0.05f;
			cameraData.pitch = std::max( -pitchLimit, std::min( pitchLimit, cameraData.pitch ) );

			float cosPitch = cosf( cameraData.pitch );
			float sinPitch = sinf( cameraData.pitch );
			float cosYaw = cosf( cameraData.yaw );
			float sinYaw = sinf( cameraData.yaw );

			XMVECTOR target = XMLoadFloat4( &cameraData.at );
			XMVECTOR offset = XMVectorSet(
				cameraData.orbitRadius * cosPitch * sinYaw,
				cameraData.orbitRadius * sinPitch,
				cameraData.orbitRadius * cosPitch * cosYaw,
				0.0f
			);

			XMStoreFloat4( &cameraData.eye, XMVectorAdd( target, offset ) );
		}

		cameraConstants.cameraPosition = cameraData.eye;

		const XMVECTOR& eye = XMLoadFloat4( &cameraData.eye );
		const XMVECTOR& at  = XMLoadFloat4( &cameraData.at );
		const XMVECTOR& up  = XMLoadFloat4( &cameraData.up );
		XMMATRIX view = XMMatrixLookAtLH( eye, at, up );
		XMMATRIX projection = XMMatrixPerspectiveFovLH( fovY, aspectRatio, nearZ, farZ );
		cameraData.viewProj = view * projection;

		cameraConstants.resetPathTracer = resetPathTracer;

		if ( resetPathTracer )
		{
			resetPathTracer = false;
			cameraConstants.frameNumber = 1;
		}
		else
		{
			cameraConstants.frameNumber++;
		}
	}
}

// TOOD: need to move this to render backend, there is too much backend work done here
void MinimalDXApp::Render()
{
    // Get the current back buffer handle
    resources.swapChainOutputTexture = &resources.swapChainTextures[frameIndex];

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

	if ( usePathTracingMode )
	{
		for ( auto& task : pathTracingTasks )
		{	
			task->execute( commandList, commandAllocator, &resources );
		}
	}
	else
	{
		for ( auto& task : shadowTasks )
		{	
			if ( auto drawTask = std::dynamic_pointer_cast<DrawTask>( task ) )
			{
				if ( !drawTask->dynamicBuffers.empty() )
				{
					// update model matrix hack
					CameraConstants* cb = reinterpret_cast<CameraConstants*>( drawTask->dynamicBuffers[0]->cpuBufferData );
					*cb = cameraConstants;	// camera position, frame number, etc.
					
					XMMATRIX model = XMLoadFloat4x4( &drawTask->model->transform.model );
					const XMMATRIX& viewProj = shadowCameraData.viewProj;
					const XMMATRIX modelViewProj = model * viewProj;
					const XMMATRIX viewProjInverse = ( XMMatrixInverse( nullptr, viewProj ) );

					XMStoreFloat4x4( &cb->modelViewProj, modelViewProj );
					XMStoreFloat4x4( &cb->viewProjInverse,  viewProjInverse );
				}
			}
			task->execute( commandList, commandAllocator, &resources );
		}
		for ( auto& task : tasks )
		{	
			if ( auto drawTask = std::dynamic_pointer_cast<DrawTask>( task ) )
			{
				if ( !drawTask->dynamicBuffers.empty() )
				{
					// update model matrix hack
					CameraConstants* cb = reinterpret_cast<CameraConstants*>( drawTask->dynamicBuffers[0]->cpuBufferData );
					*cb = cameraConstants;	// camera position, frame number, etc.
					
					XMMATRIX model = XMLoadFloat4x4( &drawTask->model->transform.model );
					const XMMATRIX& viewProj = cameraData.viewProj;
					const XMMATRIX modelViewProj = model * viewProj;
					const XMMATRIX lightModelViewProj = model * shadowCameraData.viewProj;
					const XMMATRIX viewProjInverse = ( XMMatrixInverse( nullptr, viewProj ) );

					XMStoreFloat4x4( &cb->modelViewProj, modelViewProj );
					XMStoreFloat4x4( &cb->viewProjInverse, viewProjInverse );
					XMStoreFloat4x4( &cb->lightViewProj, lightModelViewProj );
					XMStoreFloat3( &cb->sunDirection, sunDirection );
				}
			}
			task->execute( commandList, commandAllocator, &resources );
		}
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
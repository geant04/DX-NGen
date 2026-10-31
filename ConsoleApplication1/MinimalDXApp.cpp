#include "MinimalDXApp.h"
#include "Pipeline.h"
#include "RootSignature.h"
#include "Mesh.h"
#include "RenderBackend.h"
#include "Settings.h"
#include <iostream>
#include <windowsx.h>
#include "TaskSystem.h"

#define SIMPLE_TEST false

// GLOBAL VARS
bool usePathTracingMode = false;
bool resetPathTracer = true;

struct PanInfo
{
	int mouseX = 0;
	int mouseY = 0;
	int pendingMouseX = 0;
	int pendingMouseY = 0;
	bool isMiddleMouseDown = false;
};

bool isLeftMouseDown = false;
int mouseWheelDelta = 0;
int lastMouseX = 0;
int lastMouseY = 0;
int deltaMouseX = 0;
int deltaMouseY = 0;

PanInfo panInfo = {};

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
		{
			int mouseX = GET_X_LPARAM( lParam );
			int mouseY = GET_Y_LPARAM( lParam );

			if ( isLeftMouseDown )
			{
				deltaMouseX += mouseX - lastMouseX;
				deltaMouseY += mouseY - lastMouseY;
				lastMouseX = mouseX;
				lastMouseY = mouseY;
			}
			else if ( panInfo.isMiddleMouseDown )
			{
				panInfo.pendingMouseX += mouseX - panInfo.mouseX;
				panInfo.pendingMouseY += mouseY - panInfo.mouseY;

				panInfo.mouseX = mouseX;
				panInfo.mouseY = mouseY;
			}

			return 0;
		}

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

	case WM_MOUSEWHEEL:
		mouseWheelDelta += 
			GET_WHEEL_DELTA_WPARAM( wParam );
		return 0;

	case WM_MBUTTONDOWN:
		panInfo.isMiddleMouseDown = true;
		panInfo.mouseX = GET_X_LPARAM( lParam );
		panInfo.mouseY = GET_Y_LPARAM( lParam );
		SetCapture( hWnd );
		return 0;

	case WM_MBUTTONUP:
		panInfo.isMiddleMouseDown = false;
		panInfo.pendingMouseX = 0;
		panInfo.pendingMouseY = 0;
		ReleaseCapture();
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

	{
		device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &uploadCommandAllocator ) );
		device->CreateCommandList(
			0, 
			D3D12_COMMAND_LIST_TYPE_DIRECT, 
			uploadCommandAllocator.Get(), 
			nullptr,
			IID_PPV_ARGS( &uploadCommandList )
		);
		uploadCommandList->Close();
		device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &uploadFence ) );
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
		const float lightDistance = 60.0f;
		const XMVECTOR eye = XMVectorSubtract( target, XMVectorScale( sunDirection, lightDistance ) );
		const XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
		const XMMATRIX view = XMMatrixLookAtLH( eye, target, up );

		const float shadowWorldWidth = 80.0f;
		const float shadowWorldHeight = 80.0f;
		const float shadowNearPlane = 0.1f;
		const float shadowFarPlane = 150.0f;

		const XMMATRIX projection = XMMatrixOrthographicLH(
			shadowWorldWidth,
			shadowWorldHeight,
			shadowNearPlane,
			shadowFarPlane
		);

		shadowCameraData.viewProj = view * projection;

		PrepareScene( this );
	}
}

void MinimalDXApp::BuildTaskList()
{
	BuildTaskSystem( this );
}

// THIS FUNC IS RAN EVERY FRAME!
// func responsible for stuff like getting mouse I/O or toggling settings and what not.
// basically processing changes and stuff from the client
void MinimalDXApp::ProcessGameState()
{
	{
		// need to implement/generate camera matrix stuff here
		// we are rebuilding the camera every frame! gosh this is horrible...
		float fovY = cameraData.fov;
		float aspectRatio = static_cast<float>( windowProperties.mWidth ) / static_cast<float>( windowProperties.mHeight );
		float nearZ = cameraData.nearZ;
		float farZ = cameraData.farZ;
		
		// Camera update code
		{
			if ( mouseWheelDelta != 0 )
			{
				const float wheelSteps = static_cast<float>( mouseWheelDelta ) / static_cast<float>( WHEEL_DELTA );
				const float zoomFactor = pow( 0.85f, wheelSteps );

				cameraData.orbitRadius *= zoomFactor;
				mouseWheelDelta = 0.0f;
			}

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

			const float movementSpeed = 0.1f;
			const float panSensitivity = 0.02f;
			
			const XMVECTOR forward = XMVector3Normalize( XMVectorSet( -sinYaw, 0.0f, -cosYaw, 0.0f ) );
			const XMVECTOR right = XMVector3Normalize( XMVectorSet( -cosYaw, 0.0f, sinYaw, 0.0f ) );
			const XMVECTOR worldUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

			XMVECTOR target = XMLoadFloat4( &cameraData.at );
			
			if ( GetAsyncKeyState( 'W' ) & 0x8000 )
			{
				target = XMVectorAdd( target, XMVectorScale( forward, movementSpeed ) );
			}
			if ( GetAsyncKeyState( 'A' ) & 0x8000 )
			{
				target = XMVectorAdd( target, XMVectorScale( -right, movementSpeed ) );
			}
			if ( GetAsyncKeyState( 'S' ) & 0x8000 )
			{
				target = XMVectorAdd( target, XMVectorScale( -forward, movementSpeed ) );
			}
			if ( GetAsyncKeyState( 'D' ) & 0x8000 )
			{
				target = XMVectorAdd( target, XMVectorScale( right, movementSpeed ) );
			}

			if ( panInfo.pendingMouseX != 0 || panInfo.pendingMouseY != 0 )
			{
				const float horizontalPan = static_cast<float>( panInfo.pendingMouseX ) * panSensitivity;
				const float verticalPan = static_cast<float>( panInfo.pendingMouseY ) * panSensitivity;

				// -right to move scene to the left
				target = XMVectorAdd( target, XMVectorScale( -right, horizontalPan ) );
				target = XMVectorAdd( target, XMVectorScale( worldUp, verticalPan ) );

				panInfo.pendingMouseX = 0;
				panInfo.pendingMouseY = 0;
			}

			XMStoreFloat4( &cameraData.at, target );


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
					
					XMMATRIX model = drawTask->pass ? XMMatrixIdentity() 
													: XMLoadFloat4x4( &drawTask->model->transform.model );

					const XMMATRIX& lightViewProj = shadowCameraData.viewProj;
					const XMMATRIX lightModelViewProj = model * lightViewProj;
					const XMMATRIX lightViewProjInverse = ( XMMatrixInverse( nullptr, lightViewProj ) );

					XMStoreFloat4x4( &cb->modelViewProj, lightModelViewProj );
					XMStoreFloat4x4( &cb->viewProjInverse,  lightViewProjInverse );
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
					
					
					XMMATRIX model = drawTask->pass ? XMMatrixIdentity() 
													: XMLoadFloat4x4( &drawTask->model->transform.model );

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
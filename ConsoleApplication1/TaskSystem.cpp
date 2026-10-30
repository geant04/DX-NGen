#include "TaskSystem.h"
#include "Pipeline.h"
#include "RootSignature.h"
#include "Mesh.h"
#include "RenderBackend.h"
#include "Settings.h"
#include <iostream>
#include <windowsx.h>

static void BuildPathtracerTaskSystemInternal( MinimalDXApp* app, Gfx::Texture* inOutTexture, 
	Gfx::Sampler* bilinearSamplerClamp, Gfx::RenderTarget* screenRenderTarget, Gfx::Buffer* cameraDataBuffer, 
	unsigned int screenWidth, unsigned int screenHeight )
{
	std::shared_ptr<ComputeTask> pathTraceTask = std::make_shared<ComputeTask>();
	{
		pathTraceTask->readWriteTextures.push_back( inOutTexture );
		pathTraceTask->dynamicBuffers.push_back( cameraDataBuffer );

		pathTraceTask->initResources.csAssetName = "csPathTracer.hlsl";
		pathTraceTask->initResources.entryPoint = "CSMain";
		pathTraceTask->initResources.dispatchWidth = ( screenWidth + 7) / 8; // hardcoded (8, 8, 1) dims
		pathTraceTask->initResources.dispatchHeight = ( screenHeight + 7) / 8;
	}

	if ( !Gfx::CreateComputeTask( app, pathTraceTask.get() ) )
	{
		return;
	}

	// Temp disabling these tasks and only pushing drawScreen, which now does "draw cube" actually
	app->pathTracingTasks.push_back( pathTraceTask );
	app->pathTracingTasks.push_back( UavToSrv(  inOutTexture ) );
	app->pathTracingTasks.push_back( drawToScreen( app, inOutTexture, bilinearSamplerClamp, cameraDataBuffer, screenRenderTarget ) );
	app->pathTracingTasks.push_back( SrvToUav(  inOutTexture ) );
}

static void BuildTaskSystemInternal( MinimalDXApp* app )
{
	ComPtr<ID3D12Device>& device = app->device;
	Gfx::Resources& resources = app->resources;

	const uint32_t screenTextureMips = 1;
	const uint32_t screenTextureSamples = 1;
	const uint32_t screenWidth = app->windowProperties.mWidth;
	const uint32_t screenHeight = app->windowProperties.mHeight;

	{
		// This is some incredibly dangerous code... that shouldn't really be called here
		app->cameraConstants.invScreenWidth = 1.0f / static_cast<float>( screenWidth );
		app->cameraConstants.invScreenHeight = 1.0f / static_cast<float>( screenHeight );
	}

	const DXGI_FORMAT screenFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	Gfx::Texture* screenTexture = Gfx::AddTexture( &resources );
	Gfx::Texture* sceneDepthTexture = Gfx::AddTexture( &resources );
	Gfx::Buffer* cameraDataBuffer = Gfx::AddBuffer( &resources );
	Gfx::Sampler* bilinearSamplerClamp = Gfx::AddSampler( &resources );
	Gfx::Sampler* pcfShadowSampler = Gfx::AddSampler( &resources );
	Gfx::RenderTarget* screenRenderTarget = Gfx::AddRenderTarget( &resources );

	{
		// BindRenderTargets should be able to handle outputToBackbuffer, so this stuff below should be unnecessary
		screenRenderTarget->rtvTexture = screenTexture;
		screenRenderTarget->viewportWidth = screenWidth;
		screenRenderTarget->viewportHeight = screenHeight;
		screenRenderTarget->scissorWidth = screenWidth;
		screenRenderTarget->scissorHeight = screenHeight;
	}
	{
		bilinearSamplerClamp->filterMode = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		bilinearSamplerClamp->addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

		pcfShadowSampler->filterMode = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		pcfShadowSampler->comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		pcfShadowSampler->addressMode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	}
	
	resources.globalTestCube.mesh = resources.globalTestCubeMesh;

	Model* testFloorModel = Gfx::AddModel( &resources );
	testFloorModel->mesh = resources.globalTestCube.mesh;

	Model* fullScreenTriangle = Gfx::AddModel( &resources );
	fullScreenTriangle = &resources.globalTestTriangle;

	Gfx::CreateRWTexture( device, screenTexture, &resources, screenWidth, screenHeight, screenFormat, screenTextureMips );
	Gfx::CreateReadTexture( device, screenTexture, &resources, screenWidth, screenHeight, screenFormat, screenTextureMips );
	Gfx::CreateConstantBuffer( device, cameraDataBuffer, &resources, &app->cameraConstants, (sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
	Gfx::CreateRTV( device, screenTexture, &resources, screenWidth, screenHeight, screenFormat, screenTextureMips );

	BuildPathtracerTaskSystemInternal( app, 
		screenTexture, 
		bilinearSamplerClamp, 
		screenRenderTarget, 
		cameraDataBuffer, 
		screenWidth, screenHeight );

	// Shadow draw tasks... Need to refactor this in such a way that:
	// 1. let every object have a shadowCaster flag or something
	// 2. process all geometry separately and automatically generate draw tasks from there
	// 3. if above works, then the same logic maps for shadows
	// This is a temporary hack because I want to say "I added shadow maps!" to my engine.
	const unsigned int shadowMapSize = 2048;
	Gfx::Texture* sunShadowMap = Gfx::AddTexture( &resources );
	Gfx::CreateDepthTexture( device, sunShadowMap, &resources, shadowMapSize, shadowMapSize, DXGI_FORMAT_D16_UNORM, 1 );
	Gfx::CreateReadTexture( device, sunShadowMap, &resources, shadowMapSize, shadowMapSize, DXGI_FORMAT_R16_UNORM, 1 );

	// Sync
	{
		// These wait for previous frame calls don't really matter in-practice.
		// This is because we aren't dynamically changing resources. This call isn't ran every frame.
		app->WaitForPreviousFrame();
	}

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

	if ( !Gfx::CreateDrawTask( app, drawSky.get() ) )
	{
		return;
	}

	std::shared_ptr<DrawTask> drawTestCube = std::make_shared<DrawTask>();
	{
		Gfx::CreateDepthTexture( device, sceneDepthTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_D16_UNORM, 1 );
		
		drawTestCube->model = &resources.globalTestCube;
		drawTestCube->model->transform.Scale( { 2.0f, 2.5f, 2.0f } );
		drawTestCube->model->transform.Translate( { 0.0f, 1.5f, 0.0f } );
		{
			Gfx::Buffer* cubeConstantBuffer = Gfx::AddBuffer( &resources );
			Gfx::CreateConstantBuffer( device, cubeConstantBuffer, &resources, &drawTestCube->drawConstants, 
				(sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
			drawTestCube->dynamicBuffers.push_back( cubeConstantBuffer );
		}
		drawTestCube->readTextures.push_back( sunShadowMap );
		drawTestCube->staticSamplers.push_back( bilinearSamplerClamp );
		drawTestCube->staticSamplers.push_back( pcfShadowSampler );
		drawTestCube->initResources.vertexShaderName = "vsGeometry.hlsl";
		drawTestCube->initResources.pixelShaderName = "psLitGeometry.hlsl";
		drawTestCube->clearRenderTarget = false;
		drawTestCube->clearDepth = true;
		drawTestCube->depthTest = true;
		drawTestCube->outputToBackBuffer = false;
		drawTestCube->BindRenderTargets( { screenRenderTarget }, sceneDepthTexture, &resources );
	}
	
	if ( !Gfx::CreateDrawTask( app, drawTestCube.get() ) )
	{
		return;
	}

	std::shared_ptr<DrawTask> drawTestFloor = std::make_shared<DrawTask>();
	{
		drawTestFloor->model = testFloorModel;
		drawTestFloor->model->transform.Scale( { 20.0f, 0.5f, 20.0f } );
		drawTestFloor->model->transform.Translate( { 0.0f, -0.75f, 0.0f } );
		{
			Gfx::Buffer* floorConstantBuffer = Gfx::AddBuffer( &resources );
			Gfx::CreateConstantBuffer( device, floorConstantBuffer, &resources, &drawTestFloor->drawConstants, 
				(sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
			drawTestFloor->dynamicBuffers.push_back( floorConstantBuffer );
		}
		drawTestFloor->readTextures.push_back( sunShadowMap );
		drawTestFloor->staticSamplers.push_back( bilinearSamplerClamp );
		drawTestFloor->staticSamplers.push_back( pcfShadowSampler );
		drawTestFloor->initResources.vertexShaderName = "vsGeometry.hlsl";
		drawTestFloor->initResources.pixelShaderName = "psLitGeometry.hlsl";
		drawTestFloor->clearRenderTarget = false;
		drawTestFloor->clearDepth = false;
		drawTestFloor->depthTest = true;
		drawTestFloor->outputToBackBuffer = false;
		drawTestFloor->BindRenderTargets( { screenRenderTarget }, sceneDepthTexture, &resources );
	}
	
	if ( !Gfx::CreateDrawTask( app, drawTestFloor.get() ) )
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

	if ( !Gfx::CreateDrawTask( app, drawCubeShadow.get() ) )
	{
		return;
	}
	if ( !Gfx::CreateDrawTask( app, drawFloorShadow.get() ) )
	{
		return;
	}

	app->shadowTasks.push_back( drawCubeShadow );
	app->shadowTasks.push_back( drawFloorShadow );

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
		drawDebugView->readTextures.push_back( sunShadowMap );
		drawDebugView->staticSamplers.push_back( bilinearSamplerClamp );

		debugRenderTarget->rtvTexture = screenTexture;
		debugRenderTarget->viewportWidth = static_cast<unsigned int>( 256 );
		debugRenderTarget->viewportHeight = static_cast<unsigned int>( 256 );
		debugRenderTarget->scissorWidth = static_cast<unsigned int>( debugRenderTarget->viewportWidth );
		debugRenderTarget->scissorHeight = static_cast<unsigned int>( debugRenderTarget->viewportHeight );

		drawDebugView->BindRenderTargets( { debugRenderTarget }, nullptr, &resources );
	}
		
	if ( !Gfx::CreateDrawTask( app, drawDebugView.get() ) )
	{
		return;
	}

	Gfx::Texture* outBuffer = Gfx::AddTexture( &resources );
	Gfx::CreateRWTexture( device, outBuffer, &resources, screenWidth, screenHeight, screenFormat, 1 );
	Gfx::CreateReadTexture( device, outBuffer, &resources, screenWidth, screenHeight, screenFormat, 1 );
	std::shared_ptr<ComputeTask> fxaa = std::make_shared<ComputeTask>();
	{
		fxaa->readWriteTextures.push_back( outBuffer );
		fxaa->readTextures.push_back( screenTexture );
		fxaa->dynamicBuffers.push_back( cameraDataBuffer );
		fxaa->staticSamplers.push_back( bilinearSamplerClamp );

		fxaa->initResources.csAssetName = "csFxaa.hlsl";
		fxaa->initResources.entryPoint = "main";
		fxaa->initResources.dispatchWidth = ( screenWidth + 7) / 8;
		fxaa->initResources.dispatchHeight = ( screenHeight + 7) / 8;
	}



	if ( !Gfx::CreateComputeTask( app, fxaa.get() ) )
	{
		return;
	}

	{
		// Geometry draw
		app->tasks.push_back( DepthToSrv( sunShadowMap ) );
		
		app->tasks.push_back( SrvToRtv( screenTexture ) );
		app->tasks.push_back( drawSky );
		app->tasks.push_back( drawTestCube );
		app->tasks.push_back( drawTestFloor );
		app->tasks.push_back( RtvToSrv( screenTexture ) );
	}
	{
		// Post-processing
		app->tasks.push_back( SrvToUav( outBuffer ) );
		app->tasks.push_back( fxaa );
		app->tasks.push_back( UavToSrv( outBuffer ) );
	}
	app->tasks.push_back( drawToScreen( app, outBuffer, bilinearSamplerClamp, cameraDataBuffer, screenRenderTarget ) );
	{
		app->tasks.push_back( drawDebugView );
		app->tasks.push_back( SrvToDepth( sunShadowMap ) );
	}
}

void BuildTaskSystem( MinimalDXApp* app )
{
	BuildTaskSystemInternal( app );
}
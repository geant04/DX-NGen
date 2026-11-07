#include "TaskSystem.h"
#include "Pipeline.h"
#include "RootSignature.h"
#include "Mesh.h"
#include "RenderBackend.h"
#include "Settings.h"
#include <iostream>
#include <windowsx.h>

const bool testIndirectDraw = true;

static void BuildPathtracerTaskSystemInternal( MinimalDXApp* app, Gfx::Texture* inOutTexture, 
	Gfx::Sampler* bilinearSamplerClamp, Gfx::RenderTarget* screenRenderTarget, Gfx::Buffer* cameraDataBuffer, 
	unsigned int screenWidth, unsigned int screenHeight )
{
	std::shared_ptr<ComputeTask> pathTraceTask = std::make_shared<ComputeTask>();
	{
		pathTraceTask->readWriteTextures.push_back( inOutTexture );
		pathTraceTask->constantBuffers.push_back( cameraDataBuffer );

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

	Gfx::Sampler* bilinearSamplerClamp = Gfx::AddSampler( &resources );
	Gfx::Sampler* pcfShadowSampler = Gfx::AddSampler( &resources );
	Gfx::RenderTarget* screenRenderTarget = Gfx::AddRenderTarget( &resources );

	Gfx::Buffer* cameraDataBuffer = Gfx::AddBuffer( &resources );

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
	Gfx::CreateDepthTexture( device, sceneDepthTexture, &resources, screenWidth, screenHeight, DXGI_FORMAT_D32_FLOAT, 1 );
	Gfx::CreateConstantBuffer( device, cameraDataBuffer, &resources, &app->cameraConstants, (sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
	Gfx::CreateRTV( device, screenTexture, &resources, screenWidth, screenHeight, screenFormat, screenTextureMips );
	
	cameraDataBuffer->isDynamic = true;

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
	Gfx::CreateDepthTexture( device, sunShadowMap, &resources, shadowMapSize, shadowMapSize, DXGI_FORMAT_D32_FLOAT, 1 );
	Gfx::CreateReadTexture( device, sunShadowMap, &resources, shadowMapSize, shadowMapSize, DXGI_FORMAT_R32_FLOAT, 1 );

	// Sync
	{
		// These wait for previous frame calls don't really matter in-practice.
		// This is because we aren't dynamically changing resources. This call isn't ran every frame.
		app->WaitForPreviousFrame();
	}

	std::shared_ptr<DrawTask> drawSky = std::make_shared<DrawTask>();
	{
		drawSky->model = fullScreenTriangle;
		drawSky->constantBuffers.push_back( cameraDataBuffer );
		drawSky->initResources.vertexShaderName = "vsTriangle.hlsl";
		drawSky->initResources.pixelShaderName = "sky.hlsl";

		drawSky->drawTaskID = 420;

		drawSky->clearRenderTarget = false;
		drawSky->depthTest = false;
		drawSky->outputToBackBuffer = false;
		drawSky->BindRenderTargets( { screenRenderTarget }, nullptr, &resources );
	}

	if ( !Gfx::CreateDrawTask( app, drawSky.get() ) )
	{
		return;
	}

	{
		// Geometry draw
		app->tasks.push_back( DepthToSrv( sunShadowMap ) );
		app->tasks.push_back( SrvToRtv( screenTexture ) );
		app->tasks.push_back( drawSky );
	}

	// Indirect draws for our world-geometry
	{
		Gfx::Buffer* transformBuffer = Gfx::AddBuffer( &resources );
		transformBuffer->resource = app->scene.transformBuffer;
		transformBuffer->numElements = app->scene.transformCount;
		transformBuffer->structuredByteStride = sizeof( Transform );
		Gfx::CreateSRVBuffer( device, transformBuffer, &resources );

		Gfx::Buffer* transformIndexOffsetBuffer = app->scene.opaquePass.transformIndexOffsets;
		Gfx::Buffer* indirectDrawArgsBuffer = app->scene.opaquePass.indirectCommandBuffer;
		Gfx::Buffer* transformBatchIdsBuffer = app->scene.opaquePass.transformBatchIds;
		unsigned int numBatches = app->scene.opaquePass.commands.size();

		// All of this below should not exist, they should've been built in Scene already, or something else
		{
			transformIndexOffsetBuffer->numElements = app->scene.transformCount;
			transformIndexOffsetBuffer->structuredByteStride = sizeof( int );
			transformIndexOffsetBuffer->isDynamic = false;
			Gfx::CreateUAVBuffer( device, transformIndexOffsetBuffer, &resources );
			Gfx::CreateSRVBuffer( device, transformIndexOffsetBuffer, &resources );

			indirectDrawArgsBuffer->numElements = numBatches;
			indirectDrawArgsBuffer->structuredByteStride = sizeof( IndirectCommand );
			indirectDrawArgsBuffer->isDynamic = false;
			Gfx::CreateUAVBuffer( device, indirectDrawArgsBuffer, &resources );

			transformBatchIdsBuffer->numElements = app->scene.transformCount;
			transformBatchIdsBuffer->structuredByteStride = sizeof( int );
			transformBatchIdsBuffer->isDynamic = false;
			Gfx::CreateSRVBuffer( device, transformBatchIdsBuffer, &resources );
		}

		std::shared_ptr<DrawTask> drawShadows = std::make_shared<DrawTask>();
		{
			Gfx::Buffer* shadowCameraData = Gfx::AddBuffer( &resources );
			Gfx::CreateConstantBuffer( device, shadowCameraData, &resources, 
				&drawShadows->drawConstants, (sizeof(CameraConstants) + 255 & ~255), sizeof( CameraConstants ) );
			shadowCameraData->isDynamic = true;

			drawShadows->pass = &app->scene.shadowPass;
			drawShadows->constantBuffers.push_back( shadowCameraData );
			drawShadows->readBuffers.push_back( transformBuffer );
			drawShadows->readBuffers.push_back( transformIndexOffsetBuffer );
			drawShadows->initResources.vertexShaderName = "vsIndirectGeometry.hlsl";
			drawShadows->initResources.pixelShaderName = nullptr;
			drawShadows->clearDepth = true;
			drawShadows->depthTest = true;
			drawShadows->viewportWidth = shadowMapSize;
			drawShadows->viewportHeight = shadowMapSize;
			drawShadows->BindRenderTargets( {}, sunShadowMap, &resources );
		}

		if ( !Gfx::CreateDrawTask( app, drawShadows.get() ) )
		{
			return;
		}
		// app->shadowTasks.push_back( drawShadows );

		std::shared_ptr<ComputeTask> drawOpaqueCull = std::make_shared<ComputeTask>();
		{
			drawOpaqueCull->readBuffers.push_back( transformBuffer );
			drawOpaqueCull->readBuffers.push_back( transformBatchIdsBuffer );
			drawOpaqueCull->writeBuffers.push_back( transformIndexOffsetBuffer );
			drawOpaqueCull->writeBuffers.push_back( indirectDrawArgsBuffer );
			drawOpaqueCull->constantBuffers.push_back( cameraDataBuffer );
			
			unsigned int numElements = app->scene.opaquePass.batchIds.size();
			drawOpaqueCull->initResources.csAssetName = "csCull.hlsl";
			drawOpaqueCull->initResources.entryPoint = "main";
			drawOpaqueCull->initResources.dispatchWidth = ( numElements + 63 ) / 64;
			drawOpaqueCull->initResources.dispatchHeight = 1;
		}

		if ( !Gfx::CreateComputeTask( app, drawOpaqueCull.get() ) )
		{
			return;
		}

		
		std::shared_ptr<DrawTask> drawOpaque = std::make_shared<DrawTask>();
		{
			drawOpaque->pass = &app->scene.opaquePass;
			drawOpaque->constantBuffers.push_back( cameraDataBuffer );
			drawOpaque->readBuffers.push_back( transformBuffer );
			drawOpaque->readBuffers.push_back( transformIndexOffsetBuffer );
			drawOpaque->readTextures.push_back( sunShadowMap );
			drawOpaque->staticSamplers.push_back( bilinearSamplerClamp );
			drawOpaque->staticSamplers.push_back( pcfShadowSampler );
			drawOpaque->initResources.vertexShaderName = "vsIndirectGeometry.hlsl";
			drawOpaque->initResources.pixelShaderName = "psLitGeometry.hlsl";
			drawOpaque->clearRenderTarget = false;
			drawOpaque->clearDepth = true;
			drawOpaque->depthTest = true;
			drawOpaque->outputToBackBuffer = false;
			drawOpaque->BindRenderTargets( { screenRenderTarget }, sceneDepthTexture, &resources );
		}

		if ( !Gfx::CreateDrawTask( app, drawOpaque.get() ) )
		{
			return;
		}

		{
			app->tasks.push_back( SrvToUav( transformIndexOffsetBuffer ) );
			app->tasks.push_back( IndirectToUav( indirectDrawArgsBuffer ) );
			app->tasks.push_back( clearDrawArgs( app, 
				app->scene.opaquePass.indirectCommandBuffer, numBatches ) );
			app->tasks.push_back( UavBarrier( indirectDrawArgsBuffer->resource ) );
			app->tasks.push_back( drawOpaqueCull );
			app->tasks.push_back( UavToIndirect( indirectDrawArgsBuffer ) );
			app->tasks.push_back( UavToSrv( transformIndexOffsetBuffer ) );
		}
		app->tasks.push_back( drawShadows );
		app->tasks.push_back( drawOpaque );
	}

	app->tasks.push_back( RtvToSrv( screenTexture ) );

	Gfx::RenderTarget* debugRenderTarget = Gfx::AddRenderTarget( &resources );
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

		Gfx::CreateReadTexture( device, sunShadowMap, &resources, screenWidth, screenHeight, DXGI_FORMAT_R32_FLOAT, 1 );
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
		fxaa->constantBuffers.push_back( cameraDataBuffer );
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

	if ( true )
	{
		app->tasks.push_back( SrvToUav( outBuffer ) );
		app->tasks.push_back( fxaa );
		app->tasks.push_back( UavToSrv( outBuffer ) );
	}
	else
	{
		outBuffer = screenTexture;
	}
	app->tasks.push_back( drawToScreen( app, outBuffer, bilinearSamplerClamp, cameraDataBuffer, screenRenderTarget ) );
	if ( true )
	{
		app->tasks.push_back( drawDebugView );
		app->tasks.push_back( SrvToDepth( sunShadowMap ) );
	}
}

void BuildTaskSystem( MinimalDXApp* app )
{
	BuildTaskSystemInternal( app );
}
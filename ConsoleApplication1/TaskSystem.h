#pragma once

#include "RenderBackend.h"
#include "ResourceManager.h"

// In the future, we can make some sort of dynamic optimization that 
// caches the task but only changes the resource or something... idk
inline std::shared_ptr<BarrierTask> UavToSrv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture->resource, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
}

inline std::shared_ptr<BarrierTask> UavToSrv( Gfx::Buffer* buffer )
{
	return Gfx::CreateBarrierTask( 
		buffer->resource, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
}

inline std::shared_ptr<BarrierTask> DepthToSrv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture->resource, 
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
}

inline std::shared_ptr<BarrierTask> SrvToDepth( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture->resource, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE );
}

inline std::shared_ptr<BarrierTask> SrvToUav( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture->resource, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
}

inline std::shared_ptr<BarrierTask> SrvToUav( Gfx::Buffer* buffer )
{
	return Gfx::CreateBarrierTask( 
		buffer->resource, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
}

inline std::shared_ptr<BarrierTask> SrvToRtv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture->resource, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET );
}

inline std::shared_ptr<BarrierTask> RtvToSrv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture->resource, 
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
}

inline std::shared_ptr<BarrierTask> IndirectToUav( Gfx::Buffer* buffer )
{
	return Gfx::CreateBarrierTask( 
		buffer->resource, 
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
}

inline std::shared_ptr<BarrierTask> UavToIndirect( Gfx::Buffer* buffer )
{
	return Gfx::CreateBarrierTask( 
		buffer->resource, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT );
}

inline std::shared_ptr<DrawTask> drawToScreen( MinimalDXApp* app, 
	Gfx::Texture* input, 
	Gfx::Sampler* bilinearSamplerClamp, 
	Gfx::Buffer* cameraDataBuffer, 
	Gfx::RenderTarget* screenRenderTarget )
{
	std::shared_ptr<DrawTask> drawScreen = std::make_shared<DrawTask>();
	{
		drawScreen->model = &app->resources.globalTestTriangle;
		drawScreen->initResources.vertexShaderName = "vsTriangle.hlsl";
		drawScreen->initResources.pixelShaderName = "psBlit.hlsl";
		drawScreen->readTextures.push_back( input );
		drawScreen->staticSamplers.push_back( bilinearSamplerClamp );
		drawScreen->clearRenderTarget = false;
		drawScreen->depthTest = false;
		drawScreen->outputToBackBuffer = true;
		drawScreen->constantBuffers.push_back( cameraDataBuffer );

		drawScreen->drawTaskID = 69;

		drawScreen->BindRenderTargets( { screenRenderTarget }, nullptr, &app->resources );
	}
	
	if ( !Gfx::CreateDrawTask( app, drawScreen.get() ) )
	{
		return nullptr;
	}

	return drawScreen;
}

inline std::shared_ptr<ComputeTask> clearDrawArgs( MinimalDXApp* app, Gfx::Buffer* buffer, unsigned int numElements )
{
	Gfx::Buffer* dataBuffer = Gfx::AddBuffer( &app->resources );
	Gfx::CreateConstantBuffer( app->device, dataBuffer, &app->resources, 
		&numElements, (sizeof(int) + 255 & ~255), sizeof(int) );

	std::shared_ptr<ComputeTask> clearBuffer = std::make_shared<ComputeTask>();
	{
		clearBuffer->constantBuffers.push_back( dataBuffer );
		clearBuffer->writeBuffers.push_back( buffer );
		clearBuffer->initResources.csAssetName = "csMemSetZero.hlsl";
		clearBuffer->initResources.entryPoint = "main";
		clearBuffer->initResources.dispatchWidth = ( numElements + 7) / 8;
		clearBuffer->initResources.dispatchHeight = 1;
	}

	if ( !Gfx::CreateComputeTask( app, clearBuffer.get() ) )
	{
		return nullptr;
	}

	return clearBuffer;
}

void BuildTaskSystem( MinimalDXApp* app );
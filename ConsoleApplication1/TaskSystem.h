#pragma once

#include "RenderBackend.h"
#include "ResourceManager.h"

inline std::shared_ptr<BarrierTask> UavToSrv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
}

inline std::shared_ptr<BarrierTask> DepthToSrv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture, 
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
}

inline std::shared_ptr<BarrierTask> SrvToDepth( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE );
}

inline std::shared_ptr<BarrierTask> SrvToUav( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
}

inline std::shared_ptr<BarrierTask> SrvToRtv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET );
}

inline std::shared_ptr<BarrierTask> RtvToSrv( Gfx::Texture* texture )
{
	return Gfx::CreateBarrierTask( 
		texture, 
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
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
		drawScreen->dynamicBuffers.push_back( cameraDataBuffer );

		drawScreen->BindRenderTargets( { screenRenderTarget }, nullptr, &app->resources );
	}
	
	if ( !Gfx::CreateDrawTask( app, drawScreen.get() ) )
	{
		return nullptr;
	}

	return drawScreen;
}

void BuildTaskSystem( MinimalDXApp* app );
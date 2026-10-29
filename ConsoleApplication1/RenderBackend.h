#pragma once

#include "MinimalDXApp.h"
#include "Task.h"
#include "ResourceManager.h"

namespace Gfx
{
	bool CreateDrawTask( MinimalDXApp* app, DrawTask* drawTask );
	bool CreateComputeTask( MinimalDXApp* app, ComputeTask* computeTask );

	inline std::shared_ptr<BarrierTask> CreateBarrierTask(
		Gfx::Texture* screenTexture,
		D3D12_RESOURCE_STATES stateBefore,
		D3D12_RESOURCE_STATES stateAfter
	)
	{
		std::shared_ptr<BarrierTask> task = std::make_shared<BarrierTask>();
		task->resource = screenTexture->resource.Get();
		task->stateBefore = stateBefore;
		task->stateAfter = stateAfter;
		return task;
	};
}
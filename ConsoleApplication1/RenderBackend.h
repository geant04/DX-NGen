#pragma once

#include "MinimalDXApp.h"
#include "Task.h"

namespace Gfx
{
	bool CreateDrawTask( MinimalDXApp* app, DrawTask* drawTask );
	bool CreateComputeTask( MinimalDXApp* app, ComputeTask* computeTask );
}
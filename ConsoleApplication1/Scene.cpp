#include "Scene.h"
#include "RenderBackend.h"
#include <iostream>
#include <random>

void BuildScene( MinimalDXApp* app )
{
	std::vector<RenderableObject*>& sceneObjects = app->scene.sceneObjects;
	Gfx::Resources& resources = app->resources;

	RenderableObject* testCube = Gfx::AddToResourceBuffer( &resources.renderableObjects );

	{
		testCube->mesh = resources.globalTestCubeMesh;
		testCube->transform.Scale( { 2.0f, 2.5f, 2.0f } );
		testCube->transform.Translate( { 0.0f, 1.5f, 0.0f } );
		testCube->isShadowCaster = true;
		sceneObjects.push_back(testCube);
	}

	const unsigned int numTestObjects = 80;
	std::mt19937 randomGenerator( 0xC0FFEEu );
	std::uniform_real_distribution<float> positionDistribution( -20.0f, 20.0f );
	std::uniform_real_distribution<float> positionYDistribution( 0.0f, 5.0f );

	for ( unsigned int i = 0; i < numTestObjects; i++ )
	{
		float randomXPos = positionDistribution( randomGenerator );
		float randomYPos = positionYDistribution( randomGenerator );
		float randomZPos = positionDistribution( randomGenerator );

		if ( testCube == nullptr )
		{
			break;
		}

		RenderableObject* testCube = Gfx::AddToResourceBuffer( &resources.renderableObjects );
		testCube->mesh = resources.globalTestCubeMesh;
		testCube->transform.Scale( { 2.0f, 2.5f, 2.0f } );
		testCube->transform.Translate( { randomXPos, randomYPos, randomZPos } );
		testCube->isShadowCaster = true;
		sceneObjects.push_back(testCube);
	}

	RenderableObject* floor = Gfx::AddToResourceBuffer( &resources.renderableObjects );
	floor->mesh = resources.globalTestCubeMesh;
	floor->transform.Scale( { 100.0f, 0.5f, 100.0f } );
	floor->transform.Translate( { 0.0f, -0.75f, 0.0f } );
	sceneObjects.push_back(floor);
}

void UploadSceneMeshes( MinimalDXApp* app, std::vector<RenderableObject*>& sceneObjects )
{
	// Backend logic should be moved to RenderBackend!
	// Group all mesh data together...
	// This is more of a fundamental design flaw, but every new object should just automatically add to the global vertex buffer
	// rather than just having them be kept under individual mesh objects...
	Scene* scene = &app->scene;
	uint32_t vertexCount = 0;
	uint32_t uniqueMeshCount = 0;

	for ( RenderableObject* renderableObject : sceneObjects )
	{
		Mesh* meshPtr = renderableObject->mesh;

		if ( !meshPtr->meshUploaded )
		{
			meshPtr->vertexOffset = vertexCount;
			scene->vertices.insert( scene->vertices.end(), meshPtr->vertices.begin(), meshPtr->vertices.end() );
			vertexCount += meshPtr->vertexCount;

			meshPtr->meshUploaded = true;
			uniqueMeshCount++;
		}
		// Upload transforms
		{
			scene->transforms.push_back( renderableObject->transform );
			scene->transformCount++;
		}
	}
	
	// Debug messages are helpful for the soul.
	std::cout << "Meshes uploaded: " << uniqueMeshCount << std::endl;
	std::cout << "Transforms: " << scene->transformCount << std::endl;

	if ( scene->vertices.empty() || scene->transforms.empty() )
	{
		std::cout << "Either vertices or transforms are empty!" << std::endl;
		return;
	}

	scene->vertexBufferSize = static_cast<uint32_t>( vertexCount * sizeof( Vertex ) );

	Gfx::UploadBufferInfo<Vertex> vertexBufferInfo = {};
	vertexBufferInfo.objects = &scene->vertices[0];
	vertexBufferInfo.buffer = scene->vertexBuffer;
	vertexBufferInfo.bufferUpload = scene->vertexBufferUpload;
	vertexBufferInfo.objectBufferSize = scene->vertexBufferSize;
	
	Gfx::UploadBuffer( app, &vertexBufferInfo, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER );
	scene->vertexBuffer = vertexBufferInfo.buffer;
	scene->vertexBufferUpload = vertexBufferInfo.bufferUpload;
	scene->vertexBufferView.BufferLocation = scene->vertexBuffer->GetGPUVirtualAddress();
	scene->vertexBufferView.StrideInBytes = sizeof(Vertex);
	scene->vertexBufferView.SizeInBytes = scene->vertexBufferSize;

	scene->transformBufferSize = static_cast<uint32_t>( scene->transformCount * sizeof( Transform ) );

	Gfx::UploadBufferInfo<Transform> transformBufferInfo = {};
	transformBufferInfo.objects = &scene->transforms[0];
	transformBufferInfo.objectBufferSize = scene->transformBufferSize;

	Gfx::UploadBuffer( app, &transformBufferInfo, D3D12_RESOURCE_STATE_GENERIC_READ );
	scene->transformBuffer = transformBufferInfo.buffer;
	scene->transformBufferUpload = transformBufferInfo.bufferUpload;
}

static void PreparePassObjects( Scene* scene, std::vector<RenderableObject*>& sceneObjects )
{
	for ( RenderableObject* renderableObject : sceneObjects )
	{
		scene->opaquePass.objects.push_back( renderableObject );

		if ( renderableObject->isShadowCaster )
		{
			scene->shadowPass.objects.push_back( renderableObject );
		}
	}
}

static void PrepareBatchesForPass( Pass* pass )
{
	// For this paradigm to work, you need to upload all the transform data to the GPU upfront...
	// This way, then, to draw Obj0 and Obj1 in batch 0 (count 2), you'd access by batchBaseIdx + objId, where objId < count
	unsigned int& batchCount = pass->batchCount;
	batchCount = 0;

	// This assumes that meshes and materials are already pre-sorted.
	for ( unsigned int i = 0; i < pass->objects.size(); i++ )
	{
		RenderableObject* obj = pass->objects[i];

		uint32_t materialId = obj->materialId;
		Mesh* meshPtr = obj->mesh;
		bool buildNewBatch = false;

		if ( batchCount == 0 || 
			( meshPtr != pass->batches[batchCount - 1].mesh) ||
			( materialId != pass->batches[batchCount - 1].materialId ) )
		{	
			if ( batchCount == MAX_BATCHES )
			{
				break;
			}

			IndirectBatch& batch = pass->batches[batchCount++];
			batch.mesh = meshPtr;
			batch.materialId = materialId;
			batch.first = i;
			batch.count = 1;
		}
		else
		{
			pass->batches[pass->batchCount - 1].count++;
		}
	}
}

static void PrepareSceneIndirectDraw( MinimalDXApp* app, Pass* pass )
{	
	std::vector<IndirectCommand>& commands = pass->commands;
	commands.resize( pass->batchCount );

	// Each command consists of a DrawInstanced call.
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof(argumentDescs);
	commandSignatureDesc.ByteStride = sizeof( IndirectCommand );

	app->device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&pass->commandSignature) );

	for ( int batchIdx = 0; batchIdx < pass->batchCount; batchIdx++ )
	{
		IndirectBatch* batch = &pass->batches[batchIdx];

		commands[batchIdx].drawArgs.InstanceCount = batch->count;
		commands[batchIdx].drawArgs.StartInstanceLocation = batch->first;
		commands[batchIdx].drawArgs.StartVertexLocation = batch->mesh->vertexOffset;
		commands[batchIdx].drawArgs.VertexCountPerInstance = batch->mesh->vertexCount;
	}

	if ( commands.empty() )
	{
		std::cout << "Empty commands!" << std::endl;
		return;
	}

	Gfx::UploadBufferInfo<IndirectCommand> commandBufferInfo = {};
	commandBufferInfo.buffer = pass->indirectCommandBuffer;
	commandBufferInfo.bufferUpload = pass->indirectCommandUpload;
	commandBufferInfo.objects = &pass->commands[0];
	commandBufferInfo.objectBufferSize = static_cast<uint32_t>( pass->batchCount * sizeof( IndirectCommand ) );

	Gfx::UploadBuffer( app, &commandBufferInfo, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT );
	pass->indirectCommandBuffer = commandBufferInfo.buffer;
	pass->indirectCommandUpload = commandBufferInfo.bufferUpload;
}

void PrepareScene( MinimalDXApp* app )
{
	// Ultimate goal is to have just one dedicated draw call for everything in the scene
	// such that then DrawOpaque, or something like that, draws all opaque objects
	// while then DrawShadow then does the shadow map drawing and what-not... all the fun-jazz
	Scene* scene = &app->scene;
	std::vector<RenderableObject*>& sceneObjects = scene->sceneObjects;

	// Begin upload
	app->uploadCommandAllocator->Reset();
	app->uploadCommandList->Reset( app->uploadCommandAllocator.Get(), nullptr );

	BuildScene( app );
	UploadSceneMeshes( app, sceneObjects );
	PreparePassObjects( scene, sceneObjects );
	PrepareBatchesForPass( &scene->opaquePass );
	PrepareBatchesForPass( &scene->shadowPass );
	PrepareSceneIndirectDraw( app, &scene->opaquePass );
	PrepareSceneIndirectDraw( app, &scene->shadowPass );
	
	// Finish upload
	app->uploadCommandList->Close();
	ID3D12CommandList* lists[] = { app->uploadCommandList.Get() };
	app->commandQueue->ExecuteCommandLists( 1, lists );
}
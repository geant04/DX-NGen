#include "Scene.h"
#include "RenderBackend.h"
#include <iostream>
#include <random>

void BuildScene( MinimalDXApp* app )
{
	std::vector<Gfx::RenderableObject*>& sceneObjects = app->scene.sceneObjects;
	Gfx::Resources& resources = app->resources;

	sceneObjects.reserve( resources.renderableObjects.maxSize );

	Gfx::RenderableObject* floor = Gfx::AddToResourceBuffer( &resources.renderableObjects );
	floor->mesh = resources.globalTestCubeMesh;
	floor->transform.Scale( { 500.0f, 0.5f, 500.0f } );
	floor->transform.Translate( { 0.0f, -0.75f, 0.0f } );
	floor->isShadowCaster = false;
	sceneObjects.push_back(floor);

	Gfx::RenderableObject* testCube = Gfx::AddToResourceBuffer( &resources.renderableObjects );

	{
		testCube->mesh = resources.globalTestCubeMesh;
		testCube->transform.Scale( { 2.0f, 2.5f, 2.0f } );
		testCube->transform.Translate( { 0.0f, 1.5f, 0.0f } );
		testCube->isShadowCaster = true;
		sceneObjects.push_back(testCube);
	}

	const unsigned int numTestObjects = ( resources.renderableObjects.maxSize - 2 > 0 ) ? resources.renderableObjects.maxSize - 2 : 0u;
	std::mt19937 randomGenerator( 0xC0FFEEu );
	std::uniform_real_distribution<float> positionDistribution( -250.0f, 250.0f );
	std::uniform_real_distribution<float> positionYDistribution( -1.0f, 2.0f );

	for ( unsigned int i = 0; i < numTestObjects; i++ )
	{
		float randomXPos = positionDistribution( randomGenerator );
		float randomYPos = positionYDistribution( randomGenerator );
		float randomZPos = positionDistribution( randomGenerator );

		if ( testCube == nullptr )
		{
			break;
		}

		Gfx::RenderableObject* testCube = Gfx::AddToResourceBuffer( &resources.renderableObjects );
		testCube->mesh = resources.globalTestCubeMesh;

		float scaleX = ( i % 1000 == 0 ) ? 1.0f : 0.2f;
		float scaleY = ( i % 1000 == 0 ) ? 16.0f : 0.2f;
		float scaleZ = ( i % 1000 == 0 ) ? 1.0f : 0.2f;

		testCube->transform.Scale( { scaleX, scaleY, scaleZ } );
		testCube->transform.Translate( { randomXPos, randomYPos + 0.5f * scaleY, randomZPos } );
		testCube->isShadowCaster = true;
		sceneObjects.push_back(testCube);
	}
}

void UploadSceneMeshes( MinimalDXApp* app, std::vector<Gfx::RenderableObject*>& sceneObjects )
{
	// Backend logic should be moved to RenderBackend!
	// Group all mesh data together...
	// This is more of a fundamental design flaw, but every new object should just automatically add to the global vertex buffer
	// rather than just having them be kept under individual mesh objects...
	Scene* scene = &app->scene;
	uint32_t vertexCount = 0;
	uint32_t uniqueMeshCount = 0;

	for ( Gfx::RenderableObject* renderableObject : sceneObjects )
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

static void PreparePassObjects( Scene* scene, std::vector<Gfx::RenderableObject*>& sceneObjects )
{
	for ( Gfx::RenderableObject* renderableObject : sceneObjects )
	{
		scene->opaquePass.objects.push_back( renderableObject );

		if ( renderableObject->isShadowCaster )
		{
			scene->shadowPass.objects.push_back( renderableObject );
		}
	}
}

static void PrepareBatchesForPass( Scene* scene, Pass* pass )
{
	// For this paradigm to work, you need to upload all the transform data to the GPU upfront...
	// This way, then, to draw Obj0 and Obj1 in batch 0 (count 2), you'd access by batchBaseIdx + objId, where objId < count
	unsigned int& batchCount = pass->batchCount;
	batchCount = 0;

	// This assumes that meshes and materials are already pre-sorted.
	pass->batchIds.resize( scene->transforms.size(), NULL_BATCH_ID );

	bool isSpecialDebugCase = false;
	if ( pass->objects.size() != scene->transforms.size() )
	{
		isSpecialDebugCase = true;
	}

	for ( unsigned int i = 0; i < pass->objects.size(); i++ )
	{
		Gfx::RenderableObject* obj = pass->objects[i];

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

		pass->batchIds[obj->globalIdx] = ( batchCount - 1 );
	}

	if constexpr ( DEBUG_BATCH_ID_VECTOR )
	{
		std::cout << "DEBUG: Output batchId array passed to the scene cull CS shader: ";
		for ( int i = 0; i < pass->batchIds.size(); i++ )
		{
			std::cout << pass->batchIds[i] << " ";
		}
		std::cout << std::endl;
	}
}

static void PrepareSceneIndirectDraw( MinimalDXApp* app, Pass* pass, bool isShadow )
{	
	std::vector<IndirectCommand>& commands = pass->commands;
	commands.resize( pass->batchCount );

	// Each command consists of a DrawInstanced call.
	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = _countof( argumentDescs );
	commandSignatureDesc.ByteStride = sizeof( IndirectCommand );

	app->device->CreateCommandSignature( &commandSignatureDesc, nullptr, IID_PPV_ARGS( &pass->commandSignature ) );

	for ( unsigned int batchIdx = 0; batchIdx < pass->batchCount; batchIdx++ )
	{
		IndirectBatch* batch = &pass->batches[batchIdx];

		commands[batchIdx].drawArgs.InstanceCount = 0; // Instance count is populated during CS_Cull
		commands[batchIdx].drawArgs.StartInstanceLocation = batch->first;
		commands[batchIdx].drawArgs.StartVertexLocation = batch->mesh->vertexOffset;
		commands[batchIdx].drawArgs.VertexCountPerInstance = batch->mesh->vertexCount;
	}

	if ( commands.empty() )
	{
		std::cout << "Empty commands! Most likely because of 0 batch count." << std::endl;
		return;
	}

	Gfx::Buffer*& indirectCommandBuffer = pass->indirectCommandBuffer;
	indirectCommandBuffer = Gfx::AddBuffer( &app->resources );

	Gfx::UploadBufferInfo<IndirectCommand> commandBufferInfo = {};
	commandBufferInfo.buffer = indirectCommandBuffer->resource;
	commandBufferInfo.bufferUpload = indirectCommandBuffer->upload;
	commandBufferInfo.objects = &pass->commands[0];
	commandBufferInfo.objectBufferSize = static_cast<uint32_t>( pass->batchCount * sizeof( IndirectCommand ) );

	Gfx::UploadBuffer( app, 
		&commandBufferInfo, 
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, 
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
	indirectCommandBuffer->resource = commandBufferInfo.buffer;
	indirectCommandBuffer->upload = commandBufferInfo.bufferUpload;

	// Prepare index offsets used for finding the transform of visible instance geometry
	// This works such that: offsetIdx[0, 1, 2, 4, ... ]
	// Then, access by transforms[offsetIdx[instanceId]]; to get the transform matrix
	// This assumes that everything is properly sorted. and that my logic works. Woop.
	if ( app->scene.transformCount != pass->batchIds.size() )
	{
		std::cout << "ERROR: global transform count != pass's batchID vector size! This won't map properly." << std::endl;
		return;
	}

	const unsigned int numTransforms = app->scene.transformCount;
	Gfx::Buffer*& transformIndexOffsets = pass->transformIndexOffsets;
	Gfx::Buffer*& transformBatchIds = pass->transformBatchIds;
	transformIndexOffsets = Gfx::AddBuffer( &app->resources );
	transformBatchIds = Gfx::AddBuffer( &app->resources );

	Gfx::UploadBufferInfo<uint32_t> transformIndexBufferInfo = {};
	transformIndexBufferInfo.buffer = transformIndexOffsets->resource;
	transformIndexBufferInfo.bufferUpload = transformIndexOffsets->upload;
	transformIndexBufferInfo.objects = &pass->batchIds[0];
	transformIndexBufferInfo.objectBufferSize = static_cast<uint32_t>( numTransforms * sizeof( uint32_t ) );

	Gfx::UploadBuffer( app, 
		&transformIndexBufferInfo, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
	transformIndexOffsets->resource = transformIndexBufferInfo.buffer;
	transformIndexOffsets->upload = transformIndexBufferInfo.bufferUpload;

	Gfx::UploadBufferInfo<uint32_t> batchIdBufferInfo = {};
	batchIdBufferInfo.buffer = transformBatchIds->resource;
	batchIdBufferInfo.bufferUpload = transformBatchIds->upload;
	batchIdBufferInfo.objects = &pass->batchIds[0];
	batchIdBufferInfo.objectBufferSize = static_cast<uint32_t>( numTransforms * sizeof( uint32_t ) );

	Gfx::UploadBuffer( app, &batchIdBufferInfo, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
	transformBatchIds->resource = batchIdBufferInfo.buffer;
	transformBatchIds->upload = batchIdBufferInfo.bufferUpload;

	if ( isShadow )
	{
		// We are also leaking resources, like horribly.
		std::cout << "isShadow CULL HACK is enabled. We are doing this to get performant shadows" << std::endl;

		pass->indirectCommandBuffer = app->scene.opaquePass.indirectCommandBuffer;
	}
}

static void PrepareScenePointLights( MinimalDXApp* app )
{
	Gfx::Resources& resources = app->resources;

	auto& pointLights = app->resources.pointLights;
	unsigned int numLights = pointLights.maxSize;

	std::mt19937 randomGenerator( 0xC0FFEEu );

	float halfScaleX = 200.0f;
	float yMax = 10.0f;

	std::uniform_real_distribution<float> positionDistribution( -halfScaleX, halfScaleX );
	std::uniform_real_distribution<float> positionYDistribution( 0.0f, yMax );
	const float squaredRadius = 400.0f;

	for ( unsigned int i = 0; i < numLights; i++ )
	{
		float randomXPos = positionDistribution( randomGenerator );
		float randomYPos = positionYDistribution( randomGenerator );
		float randomZPos = positionDistribution( randomGenerator );

		float randomR = positionDistribution( randomGenerator );
		float randomG = positionDistribution( randomGenerator );
		float randomB = positionDistribution( randomGenerator );

		float normalizedR = 1.0f * ( ( randomR / halfScaleX ) + 1.0f ) * 0.5f;
		float normalizedG = 1.0f * ( ( randomG / halfScaleX ) + 1.0f ) * 0.5f;
		float normalizedB = 1.0f * ( ( randomB / halfScaleX ) + 1.0f ) * 0.5f;

		Gfx::PointLight* pointLight = Gfx::AddToResourceBuffer( &resources.pointLights );
		XMStoreFloat3( &pointLight->worldPosition, { randomXPos, randomYPos, randomZPos } );
		XMStoreFloat3( &pointLight->color, { normalizedR, normalizedG, normalizedB } );
		pointLight->squaredRadius = squaredRadius;
	}
	
	// TODO: if this upload buffer function works, then it's worth replacing previous upload calls with
	// this dramatically abstracted/simpler function definition
	Gfx::Buffer*& scenePointLights = app->scene.scenePointLights;
	scenePointLights = Gfx::AddBuffer( &resources );
	Gfx::UploadBuffer( 
		app, 
		scenePointLights, 
		&resources.pointLights.buffer, 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
	
	scenePointLights->numElements = resources.pointLights.count;
	scenePointLights->structuredByteStride = sizeof( Gfx::PointLight );
	scenePointLights->isDynamic = false;
	Gfx::CreateSRVBuffer( app->device, scenePointLights, &resources );
	Gfx::CreateUAVBuffer( app->device, scenePointLights, &resources );
}

static void PreparePointLightClusters( MinimalDXApp* app )
{
	ClusterParams& clusterParams = app->scene.clusterParams;

	uint32_t totalClusters = clusterParams.numClustersX * clusterParams.numClustersY * clusterParams.numClustersZ;
	if ( totalClusters > CLUSTERS_LIMIT )
	{
		std::cout << "ERROR: EXCEEDED TOTAL CLUSTERS" << std::endl;
		return;
	}

	clusterParams.totalClusters = totalClusters;

	Gfx::Resources& resources = app->resources;
	for ( unsigned int i = 0; i < totalClusters; i++ )
	{
		Gfx::Cluster* cluster = Gfx::AddToResourceBuffer( &resources.pointLightClusters );
		cluster->numLights = i;

		Gfx::ClusterBBOX* bbox = Gfx::AddToResourceBuffer( &resources.debugClusterBBOX );
	}

	Gfx::Buffer*& pointLightClusters = app->scene.pointLightClusters;
	pointLightClusters = Gfx::AddBuffer( &resources );
	Gfx::UploadBuffer( 
		app, 
		pointLightClusters, 
		&resources.pointLightClusters.buffer, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	pointLightClusters->numElements = totalClusters;
	pointLightClusters->structuredByteStride = sizeof( Gfx::Cluster );
	pointLightClusters->isDynamic = false;
	Gfx::CreateSRVBuffer( app->device, pointLightClusters, &resources );
	Gfx::CreateUAVBuffer( app->device, pointLightClusters, &resources );

	Gfx::Buffer*& debugClusterBBOX = app->scene.debugBboxClusters;
	debugClusterBBOX = Gfx::AddBuffer( &resources );
	Gfx::UploadBuffer( 
		app, 
		debugClusterBBOX, 
		&resources.debugClusterBBOX.buffer, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	debugClusterBBOX->numElements = totalClusters;
	debugClusterBBOX->structuredByteStride = sizeof( Gfx::ClusterBBOX );
	debugClusterBBOX->isDynamic = false;
	Gfx::CreateUAVBuffer( app->device, debugClusterBBOX, &resources );
}


void PrepareScene( MinimalDXApp* app )
{
	// Ultimate goal is to have just one dedicated draw call for everything in the scene
	// such that then DrawOpaque, or something like that, draws all opaque objects
	// while then DrawShadow then does the shadow map drawing and what-not... all the fun-jazz
	Scene* scene = &app->scene;
	std::vector<Gfx::RenderableObject*>& sceneObjects = scene->sceneObjects;

	// Begin upload
	app->uploadCommandAllocator->Reset();
	app->uploadCommandList->Reset( app->uploadCommandAllocator.Get(), nullptr );

	BuildScene( app );
	UploadSceneMeshes( app, sceneObjects );
	PreparePassObjects( scene, sceneObjects );
	PrepareBatchesForPass( scene, &scene->opaquePass );
	PrepareBatchesForPass( scene, &scene->shadowPass );
	PrepareSceneIndirectDraw( app, &scene->opaquePass, false );
	PrepareSceneIndirectDraw( app, &scene->shadowPass, true );

	// Assign final pass details
	{
		scene->opaquePass.camera = &app->cameraData;
		scene->shadowPass.camera = &app->shadowCameraData;
	}

	PrepareScenePointLights( app );
	PreparePointLightClusters( app );
	
	// Finish upload
	app->uploadCommandList->Close();
	ID3D12CommandList* lists[] = { app->uploadCommandList.Get() };
	app->commandQueue->ExecuteCommandLists( 1, lists );
}
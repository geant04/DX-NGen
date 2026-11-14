#pragma once

#include "Common.h"
#include "Model.h"
#include "ResourceManager.h"
#include "Camera.h"

struct MinimalDXApp;

#define MAX_BATCHES 100
#define NULL_BATCH_ID 0xFFFFFFFFu
#define DEBUG_BATCH_ID_VECTOR false

struct IndirectBatch
{
	Mesh* mesh;
	uint32_t materialId;
	uint32_t first;
	uint32_t count;
};

struct IndirectCommand
{
	D3D12_DRAW_ARGUMENTS drawArgs;
};

struct Pass
{
	std::vector<IndirectCommand> commands;
	std::vector<Gfx::RenderableObject*> objects = {};
	IndirectBatch batches[MAX_BATCHES];
	unsigned int batchCount = 0;

	Gfx::Buffer* indirectCommandBuffer = nullptr;
	ComPtr<ID3D12CommandSignature> commandSignature;

	std::vector<uint32_t> batchIds = {};
	Gfx::Buffer* transformIndexOffsets = nullptr;
	Gfx::Buffer* transformBatchIds = nullptr;

	// View information
	Camera* camera;
};

struct ClusterParams
{
	uint32_t numClustersX = 16;
	uint32_t numClustersY = 8;
	uint32_t numClustersZ = 16;
	uint32_t totalClusters = 0;
};

struct Scene
{
	std::vector<Gfx::RenderableObject*> sceneObjects = {};

	Pass opaquePass = {};
	Pass shadowPass = {};

	Gfx::Buffer* scenePointLights;

	// Global stuffs
	ComPtr<ID3D12Resource> vertexBuffer = nullptr;
	ComPtr<ID3D12Resource> vertexBufferUpload = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

	ComPtr<ID3D12Resource> transformBuffer = nullptr;
	ComPtr<ID3D12Resource> transformBufferUpload = nullptr;

	// There is lots of room for optimization here. We're doing some pretty pricy copies
	std::vector<Vertex> vertices = {};
	uint32_t vertexCount = 0;
	uint32_t vertexBufferSize = 0;

	std::vector<Transform> transforms = {};
	uint32_t transformCount = 0;
	uint32_t transformBufferSize = 0;

	ClusterParams clusterParams = {};
	Gfx::Buffer* pointLightClusters;
	Gfx::Buffer* debugBboxClusters;
};

void BuildScene( MinimalDXApp* app );
void PrepareScene( MinimalDXApp* app );
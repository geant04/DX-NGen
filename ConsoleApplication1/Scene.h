#pragma once

#include "Common.h"
#include "Model.h"

struct MinimalDXApp;

#define MAX_BATCHES 100

struct IndirectBatch
{
	Mesh* mesh;
	uint32_t materialId;
	uint32_t first;
	uint32_t count;
};

struct RenderableObject
{
	Transform transform = {};
	Mesh* mesh;
	uint32_t materialId = 0;
	bool isShadowCaster = false;
};

struct IndirectCommand
{
	D3D12_DRAW_ARGUMENTS drawArgs;
};

struct Pass
{
	std::vector<IndirectCommand> commands;
	std::vector<RenderableObject*> objects = {};
	IndirectBatch batches[MAX_BATCHES];
	unsigned int batchCount = 0;

	ComPtr<ID3D12CommandSignature> commandSignature;
	ComPtr<ID3D12Resource> indirectCommandBuffer;
	ComPtr<ID3D12Resource> indirectCommandUpload;
};

struct Scene
{
	std::vector<RenderableObject*> sceneObjects = {};

	Pass opaquePass = {};
	Pass shadowPass = {};

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
};

void BuildScene( MinimalDXApp* app );
void PrepareScene( MinimalDXApp* app );
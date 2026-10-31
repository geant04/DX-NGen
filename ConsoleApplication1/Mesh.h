#pragma once

#include "Common.h"

struct Vertex
{
    XMFLOAT4 position;
    XMFLOAT4 normal;
	XMFLOAT2 uv;
};

class Mesh
{
public:
	std::vector<Vertex> vertices;
	const uint32_t vertexCount;
	const uint32_t vertexBufferSize;

	ComPtr<ID3D12Resource> vertexBuffer = nullptr;
	ComPtr<ID3D12Resource> vertexBufferUpload = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	
	uint32_t vertexOffset = 0;
	bool meshUploaded = false;

	Mesh() : vertices(), vertexCount( 0 ), vertexBufferSize( 0 ) {}

	Mesh(
		std::vector<Vertex> vertices, 
		const uint32_t bufferSize,
		const uint32_t vertexCount
	) : vertices( vertices ), vertexBufferSize( bufferSize ), vertexCount( vertexCount )
	{};

	static Mesh globalFullScreenTriangle;
	static Mesh globalSampleTriangle;
	static Mesh& GenerateFullScreenTriangle();
	static Mesh& GenerateCube( float scale );
};


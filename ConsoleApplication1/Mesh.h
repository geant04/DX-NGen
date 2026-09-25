#pragma once

#include "Common.h"

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
	XMFLOAT2 uv;
};

class Mesh
{
public:
	std::vector<Vertex> vertices;
	const uint32_t vertexBufferSize;

	Mesh(
		std::vector<Vertex> vertices, 
		const uint32_t bufferSize
	) : vertices(vertices), vertexBufferSize(bufferSize)
	{};

	static Mesh globalFullScreenTriangle;
	static Mesh globalSampleTriangle;
	static Mesh& GenerateFullScreenTriangle();
};


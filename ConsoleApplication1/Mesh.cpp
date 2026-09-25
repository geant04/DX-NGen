#include "Mesh.h"

Mesh& Mesh::GenerateFullScreenTriangle()
{
	std::vector<Vertex> fullScreenTriangleVertices =
    {
		{ { -1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		{ { -1.0f, 3.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, -1.0f } },
		{ { 3.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 2.0f, 1.0f } }
    };

	const uint32_t vertexBufferSize = static_cast<uint32_t>(fullScreenTriangleVertices.size() * sizeof(Vertex));

	static Mesh fullScreenTriangle = Mesh(fullScreenTriangleVertices, vertexBufferSize);

	return fullScreenTriangle;
}
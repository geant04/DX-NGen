#include "Mesh.h"

Mesh& Mesh::GenerateFullScreenTriangle()
{
	std::vector<Vertex> fullScreenTriangleVertices =
    {
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		{ { -1.0f, 3.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, -1.0f } },
		{ { 3.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 2.0f, 1.0f } }
    };

	const uint32_t vertexCount = fullScreenTriangleVertices.size();
	const uint32_t vertexBufferSize = static_cast<uint32_t>(fullScreenTriangleVertices.size() * sizeof(Vertex));

	static Mesh fullScreenTriangle = Mesh( fullScreenTriangleVertices, vertexBufferSize, vertexCount );

	return fullScreenTriangle;
}

Mesh& Mesh::GenerateCube( float scale )
{
	float h = scale * 0.5f; // Half size from center

    std::vector<Vertex> cubeVertices =
    {
        // Front Face (Normal: 0, 0, -1)
        { { -h,  h, -h, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
        { {  h,  h, -h, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -h, -h, -h, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -h, -h, -h, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  h,  h, -h, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
        { {  h, -h, -h, 1.0f }, { 0.0f, 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },

        // Back Face (Normal: 0, 0, 1)
        { {  h,  h,  h, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },  { 0.0f, 0.0f } },
        { { -h,  h,  h, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },  { 1.0f, 0.0f } },
        { {  h, -h,  h, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },  { 0.0f, 1.0f } },
        { {  h, -h,  h, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },  { 0.0f, 1.0f } },
        { { -h,  h,  h, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },  { 1.0f, 0.0f } },
        { { -h, -h,  h, 1.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },  { 1.0f, 1.0f } },

        // Top Face (Normal: 0, 1, 0)
        { { -h,  h,  h, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },  { 0.0f, 0.0f } },
        { {  h,  h,  h, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },  { 1.0f, 0.0f } },
        { { -h,  h, -h, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },  { 0.0f, 1.0f } },
        { { -h,  h, -h, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },  { 0.0f, 1.0f } },
        { {  h,  h,  h, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },  { 1.0f, 0.0f } },
        { {  h,  h, -h, 1.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },  { 1.0f, 1.0f } },

        // Bottom Face (Normal: 0, -1, 0)
        { { -h, -h, -h, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        { {  h, -h, -h, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -h, -h,  h, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -h, -h,  h, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  h, -h, -h, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { {  h, -h,  h, 1.0f }, { 0.0f, -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },

        // Left Face (Normal: -1, 0, 0)
        { { -h,  h,  h, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        { { -h,  h, -h, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -h, -h,  h, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -h, -h,  h, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -h,  h, -h, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -h, -h, -h, 1.0f }, { -1.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },

        // Right Face (Normal: 1, 0, 0)
        { {  h,  h, -h, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f },  { 0.0f, 0.0f } },
        { {  h,  h,  h, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f },  { 1.0f, 0.0f } },
        { {  h, -h, -h, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f },  { 0.0f, 1.0f } },
        { {  h, -h, -h, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f },  { 0.0f, 1.0f } },
        { {  h,  h,  h, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f },  { 1.0f, 0.0f } },
        { {  h, -h,  h, 1.0f }, { 1.0f, 0.0f, 0.0f, 0.0f },  { 1.0f, 1.0f } }
    };

    const uint32_t vertexCount = static_cast<uint32_t>( cubeVertices.size() );
    const uint32_t vertexBufferSize = vertexCount * sizeof( Vertex );

    static Mesh cube = Mesh( cubeVertices, vertexBufferSize, vertexCount );
	return cube;
}
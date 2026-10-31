#pragma once

#include "Mesh.h"

struct Transform
{
	XMFLOAT4X4 model;
	XMFLOAT4X4 rotate;
	XMFLOAT4X4 scale;
	XMFLOAT4X4 translate;

	Transform()
	{
		XMStoreFloat4x4( &model, XMMatrixIdentity() );
		XMStoreFloat4x4( &scale, XMMatrixIdentity() );
		XMStoreFloat4x4( &rotate, XMMatrixIdentity() );
		XMStoreFloat4x4( &translate, XMMatrixIdentity() );
	}

	void Scale( const XMFLOAT3& newScale )
	{
		XMMATRIX scaleMat = XMMatrixScaling( newScale.x, newScale.y, newScale.z );
		XMStoreFloat4x4( &scale, scaleMat );

		XMMATRIX rotateMat = XMLoadFloat4x4(&rotate);
		XMMATRIX translateMat = XMLoadFloat4x4(&translate);

		XMMATRIX modelMat = scaleMat * rotateMat * translateMat;
		XMStoreFloat4x4( &model, modelMat );
	}

	void Rotate(float radians, XMFLOAT3 axis)
	{
		XMVECTOR axisVec = XMLoadFloat3( &axis );
		XMMATRIX rotateMat = XMMatrixRotationAxis( axisVec, radians );
		XMStoreFloat4x4( &rotate, rotateMat );

		XMMATRIX scaleMat = XMLoadFloat4x4( &scale );
		XMMATRIX translateMat = XMLoadFloat4x4( &translate );

		XMMATRIX modelMat = scaleMat * rotateMat * translateMat;
		XMStoreFloat4x4( &model, modelMat );
	}

	void Translate(XMFLOAT3 axis)
	{
		XMMATRIX translateMat = XMMatrixTranslation( axis.x, axis.y, axis.z );
		XMStoreFloat4x4( &translate, translateMat );

		XMMATRIX scaleMat = XMLoadFloat4x4( &scale );
		XMMATRIX rotateMat = XMLoadFloat4x4( &rotate );
    
		XMMATRIX modelMat = scaleMat * rotateMat * translateMat;
		XMStoreFloat4x4( &model, modelMat );
	}
};

struct Model
{
	Transform transform = {};

	Mesh* mesh;
};

void LoadModel( const char* filePath, Model* model );
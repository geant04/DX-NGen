#pragma once

#include "Common.h"

struct CameraConstants {
	XMFLOAT4X4 modelViewProj;
	XMFLOAT4X4 viewProjInverse;
	XMFLOAT4X4 lightViewProj;
	XMFLOAT4 cameraPosition;
	XMFLOAT3 sunDirection;
	uint32_t frameNumber;
	bool resetPathTracer;
	float invScreenWidth;
	float invScreenHeight;
	uint32_t padding0[1];
};


struct Camera
{
	XMMATRIX viewProj;
	XMVECTOR initialDirection = XMVector4Normalize( XMVectorSet( 0.0f, 2.0f, -10.0f, 1.0f ) );
	XMFLOAT4 eye = { 0.0f, 0.0f, 0.0f, 0.0f };
	XMFLOAT4 at = { 0.0f, 1.0f, 0.0f, 1.0f };
	XMFLOAT4 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	// in radians
	float yaw = XM_PI;
	float orbitRadius = 12.0f;
	float pitch = asinf( 2.0f / orbitRadius );
	float sensitivity = 0.005f;

	float fov = XM_PIDIV4;
	float nearZ = 0.1f;
	float farZ = 100.0f;

	// Global camera tracking vars
	bool isLeftMouseDown = false;
	int lastMouseX, lastMouseY = 0;
};
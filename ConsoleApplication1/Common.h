#pragma once

// Does this ideally include all of our necessary DirectX libraries?
#include "WinInclude.h" 

#include <wrl/client.h>
#include <windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h> 
#include <string>
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

inline std::wstring Utf8ToWide( const char* value )
{
	if ( value == nullptr )
	{
		return {};
	}

	int requiredLength = MultiByteToWideChar( CP_UTF8, 0, value, -1, nullptr, 0 );
	if ( requiredLength <= 0 )
	{
		return {};
	}

	std::wstring result( requiredLength, L'\0' );

	MultiByteToWideChar( CP_UTF8, 0, value, -1, &result[0], requiredLength );

	result.resize( requiredLength - 1 );
	return result;
}

inline std::wstring GetAssetFullPath( const char* assetName )
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);

	std::wstring exePath(buffer);
	exePath = exePath.substr(0, exePath.find_last_of(L"\\"));

	// Ugly hardcoding for now, but we work with it...
    return exePath + (L"\\..\\..\\shaders\\") + std::wstring(Utf8ToWide(assetName));
}
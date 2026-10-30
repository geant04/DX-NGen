#pragma once
#include "Common.h"

struct Image
{
	// store format
	void* data;

	uint32_t width, height = 0;
	uint32_t byteSizePerPixel;
	uint32_t channels;
};

void LoadImageFromPath( const char* filePath, Image* outImage )
{

}
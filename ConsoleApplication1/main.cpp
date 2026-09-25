#include <iostream>

#include "WinInclude.h"
#include "MinimalDXApp.h"
#include "Settings.h"

int main()
{
	HINSTANCE hInstance = GetModuleHandle( nullptr );
	int nCmdShow = SW_SHOW;

	MinimalDXApp app;
	GlobalSettings globalSettings = {};

	app.CreateDisplayWindow( hInstance, nCmdShow );
	app.InitializeDX( globalSettings );
	app.BuildTaskList();
	app.Run();
}

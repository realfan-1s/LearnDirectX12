// DX12Introduce.cpp : 定义应用程序的入口点。
//
#pragma comment(linker, "/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")
#include "framework.h"
#include <memory>
#include "BoxApp.h"
#include "D3DAPP_Template.h"

using namespace Template;
using namespace DirectX;
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd){
	UNREFERENCED_PARAMETER(prevInstance);
	UNREFERENCED_PARAMETER(cmdLine);
	UNREFERENCED_PARAMETER(showCmd);
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	// 为调试版本开启内存检测，监督内存泄漏
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	try
	{
		std::unique_ptr<D3DApp_Template<BoxApp>> app = std::make_unique<BoxApp>(hInstance, false, Camera::first);
	    if(!app->Init())
	        return 0;
	
	    return app->Run();
	}
	catch(DxException& e)
	{
	    MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
	    return 0;
	}
#else
	std::unique_ptr<D3DApp_Template<BoxApp>> app = std::make_unique<BoxApp>(hInstance, false, Camera::first);
	if(!app->Init())
		return 0;
	
	return app->Run();
#endif
}
// Main.cpp
#include "PCH.h"
#include "D3D12App.h" // 包含我们的新类

// 启用 D3D12 Debug Layer
void EnableDebugLayer()
{
#if defined(_DEBUG)
    ID3D12Debug* pDebugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pDebugController)))
    {
        pDebugController->EnableDebugLayer();
        pDebugController->Release();
    }
#endif
}

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 开启 Debug Layer
    EnableDebugLayer();

    try
    {
        // 1. 创建我们的应用实例
        D3D12App app(1280, 720, L"DX12 Demo (Phase 4: Refactor)");

        // 2. 运行它
        app.Run();
    }
    catch (std::exception& e)
    {
        // 捕获异常
        MessageBoxA(NULL, e.what(), "Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    return 0;
}
// PCH.h
// Precompiled Header
#pragma once
#include <windows.h>
// C++
#include <windows.h>
#include <stdexcept> 
#include <string>    
#include <vector>
#include <algorithm>
#include <wrl.h> 
#include <comdef.h> 

// DirectX
#include <d3d12.h>
#include "d3dx12.h" // 我们的辅助库
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// 链接库
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// 命名空间
using Microsoft::WRL::ComPtr;
using namespace DirectX;
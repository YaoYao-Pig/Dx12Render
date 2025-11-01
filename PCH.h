// PCH.h
// Precompiled Header
#pragma once
#include "Enum.h"

#include <windows.h>
// C++
#include <stdexcept> 
#include <string>    
#include <vector>
#include <algorithm>
#include <wrl.h> 
#include <comdef.h> 



#include<functional>
enum class RenderQueue
{
	Common = 1
};
namespace std {
	template <>
	struct hash<RenderQueue>
	{
		std::size_t operator()(RenderQueue k) const
		{
			// 策略：把 enum class 转换成它底层的整数 (int)
			// 然后调用 int 类型的默认哈希函数
			return std::hash<int>()(static_cast<int>(k));
		}
	};
}

#include<unordered_map>

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

static void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		_com_error err(hr);
		OutputDebugString(err.ErrorMessage());
		throw std::runtime_error("D3D12 Error");
	}
}
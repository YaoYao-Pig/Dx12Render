#pragma once
#include "PCH.h"
class ShadowMap
{
public:
    // 构造函数，需要设备指针和阴影贴图的分辨率
    ShadowMap(ID3D12Device* device, UINT width, UINT height);
    ~ShadowMap();

    // 禁止拷贝构造和赋值
    ShadowMap(const ShadowMap& rhs) = delete;
    ShadowMap& operator=(const ShadowMap& rhs) = delete;

    // 获取阴影贴图的 SRV (用于在主通道中读取)
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
    // 获取阴影贴图的 DSV (用于在阴影通道中写入)
    CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

    // 获取底层的 D3D12 资源
    ID3D12Resource* Resource();

    // 获取匹配阴影贴图分辨率的视口和裁剪矩形
    D3D12_VIEWPORT Viewport() const;
    D3D12_RECT ScissorRect() const;

    // 在 D3D12App 创建堆之后调用此函数，以设置此资源在堆中的句柄
    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, // SRV 在 CPU 描述符堆中的起始句柄
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, // SRV 在 GPU 描述符堆中的起始句柄
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv  // DSV 在 CPU 描述符堆中的起始句柄
    );

    // (可选) 当窗口大小改变时调用
    void OnResize(UINT newWidth, UINT newHeight);

private:
    // 内部函数，用于创建描述符
    void BuildDescriptors();
    // 内部函数，用于创建阴影贴图纹理资源
    void BuildResource();

protected:
    ID3D12Device* m_Device = nullptr;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;

    UINT m_Width = 0;
    UINT m_Height = 0;
    // 我们使用 TYPELESS 格式，因为它需要被 DSV 和 SRV 同时使用
    DXGI_FORMAT m_Format = DXGI_FORMAT_R24G8_TYPELESS;

    // 描述符句柄
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsv;

    // 阴影贴图纹理资源
    ComPtr<ID3D12Resource> m_ShadowMap = nullptr;
};
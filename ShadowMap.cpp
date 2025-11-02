// ShadowMap.cpp
#include "PCH.h"
#include "ShadowMap.h"

ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height) :
    m_Device(device),
    m_Width(width),
    m_Height(height),
    m_Viewport({ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f }),
    m_ScissorRect({ 0, 0, (int)width, (int)height })
{
    // 在构造时立即创建资源
    BuildResource();
}

ShadowMap::~ShadowMap()
{
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv() const
{
    return m_hGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv() const
{
    return m_hCpuDsv;
}

ID3D12Resource* ShadowMap::Resource()
{
    return m_ShadowMap.Get();
}

D3D12_VIEWPORT ShadowMap::Viewport() const
{
    return m_Viewport;
}

D3D12_RECT ShadowMap::ScissorRect() const
{
    return m_ScissorRect;
}

void ShadowMap::BuildDescriptors(
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
    // 保存从 D3D12App 传入的句柄
    m_hCpuSrv = hCpuSrv;
    m_hGpuSrv = hGpuSrv;
    m_hCpuDsv = hCpuDsv;

    // 使用这些句柄创建视图
    BuildDescriptors();
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
    // 阴影贴图的分辨率通常是固定的（例如 1024, 2048, 4096）
    // 并且与屏幕分辨率无关。
    // 如果你需要动态分辨率的阴影，你可以在这里重新构建资源。
    // 目前我们保持不变。
}

void ShadowMap::BuildDescriptors()
{
    //本质是，一个资源要有两套View的映射：在深度缓存阶段，把它作为一个Dsv来写入深度；在Shader阶段，把它作为贴图采样
    // --- 创建 SRV (Shader Resource View) ---
    // 我们需要一个 SRV，以便在主渲染通道中
    // 能够从阴影贴图（作为纹理）中读取数据
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    // 格式必须是 SRV 兼容的（读取深度）
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0; // 深度平面
    m_Device->CreateShaderResourceView(m_ShadowMap.Get(), &srvDesc, m_hCpuSrv);

    // --- 创建 DSV (Depth Stencil View) ---
    // 我们需要一个 DSV，以便在阴影通道中
    // 能够渲染（写入）深度信息到阴影贴图
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    // 格式必须是 DSV 兼容的（写入深度）
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Texture2D.MipSlice = 0;
    m_Device->CreateDepthStencilView(m_ShadowMap.Get(), &dsvDesc, m_hCpuDsv);
}

void ShadowMap::BuildResource()
{
    // 资源格式：
    // 1. 必须是 D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    // 2. 必须是 TYPELESS，这样它才能被 DSV (D24_S8) 和 SRV (R24_X8) 两种视图使用
    DXGI_FORMAT resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;

    CD3DX12_RESOURCE_DESC texDesc(
        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        0, // alignment
        m_Width,
        m_Height,
        1, // arraySize
        1, // mipLevels
        resourceFormat,
        1, 0, // sampleDesc
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL // 关键标志
    );

    // 为深度缓冲区指定优化的清除值
    D3D12_CLEAR_VALUE optClear = {};
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 清除格式必须匹配 DSV 格式
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    // 创建资源
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        // 初始状态：GENERIC_READ
        // 我们在渲染时会通过资源屏障将其转换为 DEPTH_WRITE (Pass 1)
        // 和 PIXEL_SHADER_RESOURCE (Pass 2)
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&m_ShadowMap)
    ));

    m_ShadowMap->SetName(L"Shadow Map Resource");
}
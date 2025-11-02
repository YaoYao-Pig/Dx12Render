// D3D12App.h
#pragma once

#include "PCH.h"
#include "PSOContainer.h"
#include "ShaderCompiler.h"
#include "Camera.h" // ** 新增: 包含 Camera.h **
#include "Geometry.h"
#include "ResourceManager.h"
#include "DX12Object.h"
#include "Light.h"
#include "ShadowMap.h"
class D3D12App
{
public:
    D3D12App(UINT width, UINT height, const wchar_t* title);
    virtual ~D3D12App();
    void Run();
    virtual LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    void InitWindow();
    void InitD3D12();
    virtual void CreateDescriptorHeaps();
    virtual void CreateRootSignature();
    virtual void CreatePSO();
    virtual void CreateResources();

    void RunCommand();
    void Cleanup();
    void WaitForGpu();
    virtual void RenderMainPass();
    virtual void RenderShadowMap();

    virtual void OnLoadAssets();
    // ** 修改: OnUpdate 现在需要 deltaTime **
    virtual void OnUpdate(float deltaTime);
    virtual void OnRender();

    // --- 成员变量 ---
    HWND m_hWnd;
    UINT m_Width;
    UINT m_Height;
    std::wstring m_Title;

    // ... (D3D12 核心, 堆, 深度缓冲 - 同前) ...
    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<IDXGISwapChain3> m_SwapChain;
    ComPtr<ID3D12Resource> m_RenderTargets[2];
    ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_ConstHeap;
    UINT m_RtvDescriptorSize;
    ComPtr<ID3D12Resource> m_DepthStencilBuffer;

    // ... (管线, 几何体, 纹理 - 同前) ...
    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12RootSignature> m_ShadowRootSignature; //阴影通道的根签名


    ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
    ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    ComPtr<ID3D12Resource> m_Texture;

    ComPtr<ID3D12Resource> m_PlaneVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_PlaneVertexBufferView;
    ComPtr<ID3D12Resource> m_PlaneIndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_PlaneIndexBufferView;


    // ... (常量缓冲区 - 同前) ...
    ComPtr<ID3D12Resource> m_ConstantBuffer;
    ComPtr<ID3D12Resource> m_LightConstantBuffer;
    UINT8* m_pConstantBufferDataBegin;
    UINT8* m_lightConstBufferDataBegin;
    struct SceneConstants 
    { 
        XMFLOAT4X4 wvpMatrix; 
        XMFLOAT4X4 worldMatrix; 
        XMFLOAT4X4 lightWvpMatrix;
        XMFLOAT4 textureInfo;
    };
    SceneConstants m_Constants;

    //  阴影通道的常量缓冲区
    // 这个缓冲区只包含光源的 WVP，用于渲染到阴影贴图
    ComPtr<ID3D12Resource> m_ShadowConstantBuffer;
    UINT8* m_pShadowConstantBufferDataBegin;
    struct ShadowConstants
    {
        XMFLOAT4X4 lightWvpMatrix; // 光源 WVP
        float padding[48];         // 填充到 256 字节
    };
    ShadowConstants m_ShadowConstants;


    // ... (同步 - 同前) ...
    ComPtr<ID3D12Fence> m_Fence;
    UINT64 m_FenceValue;
    HANDLE m_FenceEvent;
    UINT m_FrameIndex;

    float m_TotalTime; // (现在只用于立方体旋转)
    Camera m_Camera;
    InputState m_InputState;
    POINT m_LastMousePos;
    

    Geometry geometry;
    ShaderCompiler shaderCompiler;
    PSOContainer psoContainer;
    ResourceManager resourceManager;

    DX12Object dx12Object;

    Light light;


    std::unique_ptr<ShadowMap> m_ShadowMap;
    XMFLOAT4X4 m_LightViewMatrix;
    XMFLOAT4X4 m_LightProjMatrix;

    XMFLOAT4X4 m_CubeWorldMatrix;
    XMFLOAT4X4 m_PlaneWorldMatrix;

    const UINT c_alignedSceneConstantBufferSize = (sizeof(D3D12App::SceneConstants) + 255) & ~255;
    const UINT c_alignedShadowConstantBufferSize = (sizeof(D3D12App::ShadowConstants) + 255) & ~255;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};